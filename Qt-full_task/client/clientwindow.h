#pragma once
#include <QWidget>
#include <QUdpSocket>
#include <QTimer>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>

class ClientWindow : public QWidget {
    Q_OBJECT
public:
    explicit ClientWindow(QWidget *parent = nullptr);

private slots:
    void ToggleSending();
    void SendData();
    void ReadResponse();

private:
    QUdpSocket *socket_;
    QTimer *timer_;

    // сетевые настройки
    QLineEdit *ip_edit_;
    QSpinBox *port_spin_;

    // ввод параметров
    QSpinBox *x_spin_, *y_spin_, *v_spin_, *m_spin_, *s_spin_, *p_spin_;
    QDoubleSpinBox *a_spin_;

    QPushButton *start_btn_;
    QTextEdit *log_area_;

    std::array<uint16_t, 4> PackData();
};
