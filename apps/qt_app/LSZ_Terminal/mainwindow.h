#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "device/leddevice.h"
#include "device/beepdevice.h"
#include "device/serialdevice.h"
#include "controller/serialcontroller.h"
#include "device/ap3216cdevice.h"
#include "device/icm20608device.h"
#include "pages/controlpage.h"
#include "pages/serialpage.h"
#include "pages/sensorpage.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupPages();
    void setupDevices();
    void setupSerial();
    QWidget *createHomePage();
    void addBackButton(QWidget *page);
    void showPage(int index);
    void goHome();

private:
    Ui::MainWindow *ui;

    LedDevice m_led;
    BeepDevice m_beep;
    SerialDevice m_serial;
    SerialController m_serialCtrl;

    QWidget *m_homePage;

    ControlPage *m_controlPage;
    SerialPage *m_serialPage;
    SensorPage *m_sensorPage;
};

#endif // MAINWINDOW_H
