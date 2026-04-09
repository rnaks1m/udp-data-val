#pragma once
#include <QWidget>
#include <QUdpSocket>
#include <QTableWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QMap>

struct Limits {
    int X_min, X_max;
    int Y_min, Y_max;
    int V_min, V_max;
    int M_min, M_max;
    int S_min, S_max;
    double A_min, A_max;
    int P_min, P_max;
};

struct Packet {
    int X, Y, V, M, S, P;
    double A;
};

class ServerWindow : public QWidget {
    Q_OBJECT
public:
    explicit ServerWindow(QWidget *parent = nullptr);

private slots:
    void ToggleListening();
    void ProcessPendingDatagrams();

private:
    QUdpSocket *socket_;
    QSpinBox *port_spin_;
    QPushButton *listen_btn_;
    QTableWidget *clients_table_;

    Limits limits_;
    QMap<QString, int> client_errors_; // IP -> количество ошибок

    void LoadLimits();
    bool CheckLimits(const QByteArray& data);
    void UpdateClientsTable(const QString& ip, bool isError);
};
