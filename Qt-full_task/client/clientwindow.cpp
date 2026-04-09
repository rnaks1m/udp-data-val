#include "ClientWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHostAddress>
#include <cmath>
#include <algorithm>

ClientWindow::ClientWindow(QWidget *parent) : QWidget(parent) {
    socket_ = new QUdpSocket(this);
    timer_ = new QTimer(this);

    // интерфейс
    ip_edit_ = new QLineEdit("127.0.0.1");
    port_spin_ = new QSpinBox();
    port_spin_->setRange(1024, 65535);
    port_spin_->setValue(9999);

    x_spin_ = new QSpinBox(); x_spin_->setRange(0, 63);
    y_spin_ = new QSpinBox(); y_spin_->setRange(-32, 31);
    v_spin_ = new QSpinBox(); v_spin_->setRange(0, 255);
    m_spin_ = new QSpinBox(); m_spin_->setRange(0, 3);
    s_spin_ = new QSpinBox(); s_spin_->setRange(0, 3);
    a_spin_ = new QDoubleSpinBox(); a_spin_->setRange(-12.7, 12.8); a_spin_->setSingleStep(0.1);
    p_spin_ = new QSpinBox(); p_spin_->setRange(0, 130);

    start_btn_ = new QPushButton("Старт");
    log_area_ = new QTextEdit();
    log_area_->setReadOnly(true);

    QFormLayout *form = new QFormLayout();
    form->addRow("IP Сервера:", ip_edit_);
    form->addRow("Порт:", port_spin_);

    // настройка меток с диапазонами в интерфейсе
    form->addRow("Координата X (0..63):", x_spin_);
    form->addRow("Координата Y (-32..31):", y_spin_);
    form->addRow("Скорость V (0..255):", v_spin_);
    form->addRow("Режим работы M (0..3):", m_spin_);
    form->addRow("Состояние S (0..3):", s_spin_);
    form->addRow("Ускорение A (-12.7..12.8):", a_spin_);
    form->addRow("Параметр P (0..130):", p_spin_);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);
    mainLayout->addWidget(start_btn_);
    mainLayout->addWidget(new QLabel("Лог ответов сервера:"));
    mainLayout->addWidget(log_area_);

    connect(start_btn_, &QPushButton::clicked, this, &ClientWindow::ToggleSending);
    connect(timer_, &QTimer::timeout, this, &ClientWindow::SendData);
    connect(socket_, &QUdpSocket::readyRead, this, &ClientWindow::ReadResponse);
}

void ClientWindow::ToggleSending() {
    if (timer_->isActive()) {
        timer_->stop();
        start_btn_->setText("Старт");
    }
    else {
        timer_->start(1000); // отправка с частотой 1 Гц
        start_btn_->setText("Стоп");
    }
}

std::array<uint16_t, 4> ClientWindow::PackData() {
    std::array<uint16_t, 4> pack_data;

    int x = x_spin_->value();
    int y = y_spin_->value();
    int v = v_spin_->value();
    int m = m_spin_->value();
    int s = s_spin_->value();
    double a = a_spin_->value();
    int p = p_spin_->value();

    pack_data[0] = ((y + 32) & 0x3F) << 8 | (x & 0x3F);
    pack_data[1] = ((s & 0x03) << 12) | ((m & 0x03) << 8) | (v & 0xFF);

    // кодирование диапазона -12.7 .. 12.8 в битах
    int a_int = std::round(a * 10.0);
    uint8_t a_encoded = static_cast<uint8_t>(std::clamp(a_int + 127, 0, 255));

    pack_data[2] = ((p & 0xFF) << 8) | a_encoded;
    pack_data[3] = 0;

    return pack_data;
}

void ClientWindow::SendData() {
    auto pack = PackData();
    QByteArray buffer(8, 0);

    for (int i = 0; i < 4; ++i) {
        buffer[2 * i] = (pack[i] >> 8) & 0xFF;
        buffer[2 * i + 1] = pack[i] & 0xFF;
    }

    socket_->writeDatagram(buffer, QHostAddress(ip_edit_->text()), port_spin_->value());
}

void ClientWindow::ReadResponse() {
    while (socket_->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(socket_->pendingDatagramSize());
        socket_->readDatagram(datagram.data(), datagram.size());

        if (datagram.size() == 2) {
            bool valid = datagram.at(1) != 0;
            log_area_->append(valid ? "Ответ: Данные корректны" : "Ответ: ОШИБКА (Выход за лимиты)");
        }
    }
}
