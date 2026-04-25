#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QStatusBar>
#include <QStackedWidget>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int HomePageIndex = 0;
constexpr int ControlPageIndex = 1;
constexpr int SerialPageIndex = 2;
constexpr int SensorPageIndex = 3;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_led("/dev/led")
    , m_beep("/dev/beep")
    , m_serial(this)
    , m_serialCtrl(&m_serial, &m_led, &m_beep, this)
    , m_homePage(nullptr)
    , m_controlPage(nullptr)
    , m_serialPage(nullptr)
    , m_sensorPage(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("LSZ Device Manager");

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

    m_homePage = createHomePage();
    m_controlPage = new ControlPage(this);
    m_serialPage  = new SerialPage(this);
    m_sensorPage  = new SensorPage(this);

    m_controlPage->setDevices(&m_led, &m_beep);
    m_serialPage->setSerialDevice(&m_serial);

    addBackButton(m_controlPage);
    addBackButton(m_serialPage);
    addBackButton(m_sensorPage);

    ui->PageStack->addWidget(m_homePage);
    ui->PageStack->addWidget(m_controlPage);
    ui->PageStack->addWidget(m_serialPage);
    ui->PageStack->addWidget(m_sensorPage);

    goHome();

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

QWidget *MainWindow::createHomePage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("HomePage");

    QVBoxLayout *root = new QVBoxLayout(page);
    root->setContentsMargins(80, 48, 80, 48);
    root->setSpacing(28);

    QLabel *title = new QLabel("LSZ Device Manager", page);
    title->setAlignment(Qt::AlignCenter);
    title->setObjectName("HomeTitle");

    QLabel *subtitle = new QLabel("选择一个模块开始操作", page);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setObjectName("HomeSubtitle");

    QGridLayout *grid = new QGridLayout();
    grid->setHorizontalSpacing(28);
    grid->setVerticalSpacing(24);

    auto createModuleButton = [page](const QString &title,
                                     const QString &detail) {
        QPushButton *button = new QPushButton(page);
        button->setMinimumSize(240, 150);
        button->setCursor(Qt::PointingHandCursor);
        button->setText(title + "\n" + detail);
        button->setObjectName("HomeModuleButton");
        return button;
    };

    QPushButton *controlButton = createModuleButton("设备控制", "LED / Beep");
    QPushButton *serialButton = createModuleButton("串口监视", "接收日志 / 发送命令");
    QPushButton *sensorButton = createModuleButton("传感器", "AP3216C / ICM20608");

    connect(controlButton, &QPushButton::clicked,
            this, [this]() { showPage(ControlPageIndex); });
    connect(serialButton, &QPushButton::clicked,
            this, [this]() { showPage(SerialPageIndex); });
    connect(sensorButton, &QPushButton::clicked,
            this, [this]() { showPage(SensorPageIndex); });

    grid->addWidget(controlButton, 0, 0);
    grid->addWidget(serialButton, 0, 1);
    grid->addWidget(sensorButton, 0, 2);

    root->addStretch(1);
    root->addWidget(title);
    root->addWidget(subtitle);
    root->addSpacing(8);
    root->addLayout(grid);
    root->addStretch(2);

    page->setStyleSheet(
        "#HomePage { background: #f5f7fb; }"
        "#HomeTitle { color: #1f2937; font-size: 34px; font-weight: 700; }"
        "#HomeSubtitle { color: #64748b; font-size: 20px; }"
        "#HomeModuleButton {"
        "  background: white;"
        "  border: 1px solid #d8dee9;"
        "  border-radius: 8px;"
        "  color: #1f2937;"
        "  font-size: 20px;"
        "  line-height: 150%;"
        "  padding: 18px;"
        "  text-align: center;"
        "}"
        "#HomeModuleButton:pressed { background: #e8eef7; }"
        "#HomeModuleButton:focus { border: 2px solid #2563eb; }");

    return page;
}

void MainWindow::addBackButton(QWidget *page)
{
    QPushButton *backButton = new QPushButton("返回主页", page);
    backButton->setGeometry(20, 16, 120, 40);
    backButton->setCursor(Qt::PointingHandCursor);
    backButton->raise();
    backButton->setStyleSheet(
        "QPushButton {"
        "  background: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  color: #1f2937;"
        "  font-size: 16px;"
        "}"
        "QPushButton:pressed { background: #e8eef7; }");

    connect(backButton, &QPushButton::clicked,
            this, &MainWindow::goHome);
}

void MainWindow::showPage(int index)
{
    ui->PageStack->setCurrentIndex(index);
}

void MainWindow::goHome()
{
    showPage(HomePageIndex);
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
