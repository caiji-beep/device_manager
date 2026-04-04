#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QStatusBar>
#include <QListWidget>
#include <QStackedWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_led("/dev/led")
    , m_beep("/dev/beep")
    , m_serial(this)
    , m_serialCtrl(&m_serial, &m_led, &m_beep, this)
    , m_controlPage(nullptr)
    , m_serialPage(nullptr)
    , m_sensorPage(nullptr)
{
    ui->setupUi(this);

    setupPages();  //搭建前厅界面
    setupDevices(); //唤醒本地硬件
    setupSerial();  //建立串口通信
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupPages()
{
    //清理默认界面
    while (ui->PageStack->count() > 0) {
        QWidget *page = ui->PageStack->widget(0);
        ui->PageStack->removeWidget(page);
        page->deleteLater();
    }

    m_controlPage = new ControlPage(this);
    m_serialPage  = new SerialPage(this);
    m_sensorPage  = new SensorPage(this);

    m_controlPage->setDevices(&m_led, &m_beep);
    m_serialPage->setSerialDevice(&m_serial);

    ui->PageStack->addWidget(m_controlPage);
    ui->PageStack->addWidget(m_serialPage);
    ui->PageStack->addWidget(m_sensorPage);

    ui->NavListWidget->addItem("Device Control");
    ui->NavListWidget->addItem("Serial Monitor");
    ui->NavListWidget->addItem("Sensors");

    connect(ui->NavListWidget, &QListWidget::currentRowChanged,
            ui->PageStack, &QStackedWidget::setCurrentIndex);

    ui->NavListWidget->setCurrentRow(0);

    connect(m_controlPage, &ControlPage::statusMessage,
            this, [this](const QString &msg, int timeout){
        statusBar()->showMessage(msg, timeout);
    });

    connect(m_serialPage, &SerialPage::statusMessage,
            this, [this](const QString &msg, int timeout){
        statusBar()->showMessage(msg, timeout);
    });

    connect(m_sensorPage, &SensorPage::statusMessage,
            this, [this](const QString &msg, int timeout){
        statusBar()->showMessage(msg, timeout);
    });
}

void MainWindow::setupDevices()
{
    bool ledOk = m_led.open();
    if (!ledOk) {
        QMessageBox::critical(this,
                              "Error",
                              "open /dev/led fail\n" + m_led.lastError());
    }
    m_controlPage->setLedAvailable(ledOk);

    bool beepOk = m_beep.open();
    if (!beepOk) {
        QMessageBox::critical(this,
                              "Error",
                              "open /dev/beep fail\n" + m_beep.lastError());
    }
    m_controlPage->setBeepAvailable(beepOk);


    if (ledOk && beepOk) {
        statusBar()->showMessage("All local devices are ready", 3000);
    } else {
        statusBar()->showMessage("Some local devices failed to open", 5000);
    }
}

void MainWindow::setupSerial()
{
    connect(&m_serial, &SerialDevice::lineReceived,
            &m_serialCtrl, &SerialController::handleLine);

    connect(&m_serial, &SerialDevice::logMessage,
            m_serialPage, &SerialPage::appendLogMessage);

    connect(&m_serial, &SerialDevice::errorMessage,
            m_serialPage, &SerialPage::appendErrorMessage);

    connect(&m_serialCtrl, &SerialController::logMessage,
            m_serialPage, &SerialPage::appendLogMessage);

    connect(&m_serialCtrl, &SerialController::errorMessage,
            m_serialPage, &SerialPage::appendErrorMessage);

    if (!m_serial.open("/dev/ttymxc2", 115200)) {
        m_serialPage->setSerialReady(false);
        m_serialPage->appendErrorMessage("open /dev/ttymxc2 failed: " + m_serial.lastError());
        statusBar()->showMessage("Serial initialization failed", 5000);
        return;
    }

    m_serialPage->setSerialReady(true);
    m_serialPage->appendLogMessage("Serial: Ready");
    m_serial.sendLine("READY");

    statusBar()->showMessage("Serial is ready", 3000);
}
