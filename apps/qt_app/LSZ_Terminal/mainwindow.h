#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "device/leddevice.h"
#include "device/beepdevice.h"
#include "device/serialdevice.h"
#include "controller/serialcontroller.h"
#include "device/ap3216cdevice.h"
#include "device/icm20608device.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void on_LedOnButton_clicked();
    void on_LedOffButton_clicked();
    void on_BeepOnButton_clicked();
    void on_BeepOffButton_clicked();

    void onSerialLogMessage(const QString &msg);
    void onSerialErrorMessage(const QString &msg);


    void on_SerialSendButton_clicked();

    void on_SerialClearLogButton_clicked();

    void on_Ap3216cReadButton_clicked();

    void on_Icm20608ReadButton_clicked();

private:
    void appendSerialLog(const QString &msg);

private:
    Ui::MainWindow *ui;
    LedDevice m_led;
    BeepDevice m_beep;
    SerialDevice m_serial;
    SerialController m_serialCtrl;
    Ap3216cDevice m_ap3216c;
    Icm20608Device m_icm20608;
};
#endif // MAINWINDOW_H
