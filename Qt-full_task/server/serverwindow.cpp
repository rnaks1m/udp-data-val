#include "ServerWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QHeaderView>

ServerWindow::ServerWindow(QWidget *parent) : QWidget(parent) {
    socket_ = new QUdpSocket(this);
    LoadLimits();

    port_spin_ = new QSpinBox();
    port_spin_->setRange(1024, 65535);
    port_spin_->setValue(9999);

    listen_btn_ = new QPushButton("Запустить сервер");

    clients_table_ = new QTableWidget(0, 2);
    clients_table_->setHorizontalHeaderLabels({"IP Клиента", "Количество ошибок"});
    clients_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    clients_table_->setSortingEnabled(true);    // разрешаем сортировку таблицы

    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel("Порт:"));
    topLayout->addWidget(port_spin_);
    topLayout->addWidget(listen_btn_);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(clients_table_);

    connect(listen_btn_, &QPushButton::clicked, this, &ServerWindow::ToggleListening);
    connect(socket_, &QUdpSocket::readyRead, this, &ServerWindow::ProcessPendingDatagrams);
}

void ServerWindow::LoadLimits() {
    QFile file("limits.json");

    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось найти limits.json");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject obj = doc.object().value("limits").toArray()[0].toObject();

    auto GetIntRange = [&](const QString& key, int& min, int& max) {
        QJsonArray arr = obj[key].toArray();
        min = arr[0].toInt(); max = arr[1].toInt();
    };

    auto GetDoubleRange = [&](const QString& key, double& min, double& max) {
        QJsonArray arr = obj[key].toArray();
        min = arr[0].toDouble(); max = arr[1].toDouble();
    };

    GetIntRange("X", limits_.X_min, limits_.X_max);
    GetIntRange("Y", limits_.Y_min, limits_.Y_max);
    GetIntRange("V", limits_.V_min, limits_.V_max);
    GetIntRange("M", limits_.M_min, limits_.M_max);
    GetIntRange("S", limits_.S_min, limits_.S_max);
    GetDoubleRange("A", limits_.A_min, limits_.A_max);
    GetIntRange("P", limits_.P_min, limits_.P_max);
}

void ServerWindow::ToggleListening() {
    if (socket_->state() == QAbstractSocket::BoundState) {
        socket_->close();
        listen_btn_->setText("Запустить сервер");
        port_spin_->setEnabled(true);
    }
    else {
        if (socket_->bind(QHostAddress::Any, port_spin_->value())) {
            listen_btn_->setText("Остановить");
            port_spin_->setEnabled(false);
        }
    }
}

Packet unpackPacket(const QByteArray& data) {
    Packet pack;

    pack.X = data[1] & 0x3F;
    pack.Y = ((data[0] & 0x3F) << 2 | (data[1] >> 6)) - 32;

    std::array<uint16_t, 4> words;
    for (int i = 0; i < 4; ++i) {
        words[i] = static_cast<uint8_t>(data[2 * i]) << 8 | static_cast<uint8_t>(data[2 * i + 1]);
    }

    pack.X = words[0] & 0x3F;
    pack.Y = ((words[0] >> 8) & 0x3F) - 32;
    pack.V = words[1] & 0xFF;
    pack.M = (words[1] >> 8) & 0x03;
    pack.S = (words[1] >> 12) & 0x03;

    // обратная распаковка смещенного ускорения A
    int a_encoded = words[2] & 0xFF;
    pack.A = (a_encoded - 127) / 10.0;

    pack.P = (words[2] >> 8) & 0xFF;

    return pack;
}

bool ServerWindow::CheckLimits(const QByteArray& data) {
    if (data.size() != 8) {
        return false;
    }

    Packet pack = unpackPacket(data);

    return pack.X >= limits_.X_min && pack.X <= limits_.X_max &&
           pack.Y >= limits_.Y_min && pack.Y <= limits_.Y_max &&
           pack.V >= limits_.V_min && pack.V <= limits_.V_max &&
           pack.M >= limits_.M_min && pack.M <= limits_.M_max &&
           pack.S >= limits_.S_min && pack.S <= limits_.S_max &&
           pack.A >= limits_.A_min && pack.A <= limits_.A_max &&
           pack.P >= limits_.P_min && pack.P <= limits_.P_max;
}

void ServerWindow::ProcessPendingDatagrams() {
    while (socket_->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(socket_->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        socket_->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        QString ipString = sender.toString();
        if (ipString.startsWith("::ffff:")) {
            ipString.remove("::ffff:");
        }

        bool isValid = CheckLimits(datagram);
        UpdateClientsTable(ipString, !isValid);

        QByteArray response(2, 0);
        response[1] = isValid ? 1 : 0;
        socket_->writeDatagram(response, sender, senderPort);
    }
}

void ServerWindow::UpdateClientsTable(const QString& ip, bool isError) {
    if (!client_errors_.contains(ip)) {
        client_errors_[ip] = 0;
        int row = clients_table_->rowCount();
        clients_table_->insertRow(row);

        QTableWidgetItem *ipItem = new QTableWidgetItem(ip);
        QTableWidgetItem *errItem = new QTableWidgetItem("0");

        // настройка данных для корректной числовой сортировки
        errItem->setData(Qt::DisplayRole, 0);

        clients_table_->setItem(row, 0, ipItem);
        clients_table_->setItem(row, 1, errItem);
    }

    if (isError) {
        client_errors_[ip]++;
        for (int i = 0; i < clients_table_->rowCount(); ++i) {
            if (clients_table_->item(i, 0)->text() == ip) {
                QTableWidgetItem *errItem = clients_table_->item(i, 1);
                errItem->setData(Qt::DisplayRole, client_errors_[ip]);
                break;
            }
        }
    }
    // вызов сортировки по колонке ошибок по убыванию
    clients_table_->sortItems(1, Qt::DescendingOrder);
}