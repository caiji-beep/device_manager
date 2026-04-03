#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QStatusBar>
#include <QKeyEvent>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_led("/dev/led")
    , m_beep("/dev/beep")
    , m_serial(this)
    , m_serialCtrl(&m_serial,&m_led,&m_beep,this)
    , m_ap3216c("/dev/ap3216c")
{
    ui->setupUi(this);
    if(!m_led.open()){
        QMessageBox::critical(this,
                              "Error",
                              "open /dev/led fail\n" + m_led.lastError());
        ui->LedOnButton->setEnabled(false);   //禁用按钮
        ui->LedOffButton->setEnabled(false);

        statusBar()->showMessage("LED device initialization failed");

    }
    if(!m_beep.open()){
        QMessageBox::critical(this,
                              "Error",
                              "open /dev/beep fail\n" + m_beep.lastError());
        ui->BeepOnButton->setEnabled(false);//禁用按钮
        ui->BeepOffButton->setEnabled(false);

        statusBar()->showMessage("Beep device initialization failed");

    }
    if(m_led.isReady()&&m_beep.isReady())
    {
        statusBar()->showMessage("The LED and beep device are ready");
    }
    else if(m_led.isReady())
    {
        statusBar()->showMessage("The LED device is ready");
    }
    else if(m_beep.isReady())
    {
        statusBar()->showMessage("The Beep device is ready");
    }

    if (!m_ap3216c.open()) {
        QMessageBox::warning(this,
                             "Warning",
                             "Open /dev/ap3216c failed:\n" + m_ap3216c.lastError());

        ui->Ap3216cReadButton->setEnabled(false);
        statusBar()->showMessage("AP3216C open failed", 3000);
    } else {
        statusBar()->showMessage("AP3216C ready", 3000);
    }

    connect(&m_serial, &SerialDevice::lineReceived,
            &m_serialCtrl, &SerialController::handleLine);

    connect(&m_serial, &SerialDevice::logMessage,
            this, &MainWindow::onSerialLogMessage);
    connect(&m_serial, &SerialDevice::errorMessage,
            this, &MainWindow::onSerialErrorMessage);

    connect(&m_serialCtrl, &SerialController::logMessage,
            this, &MainWindow::onSerialLogMessage);
    connect(&m_serialCtrl, &SerialController::errorMessage,
            this, &MainWindow::onSerialErrorMessage);

    connect(ui->SerialInputlineEdit, &QLineEdit::returnPressed,
            this, &MainWindow::on_SerialSendButton_clicked);


    if(ui->SerialLogEdit){
        ui->SerialLogEdit->setReadOnly(true);
    }
    if(ui->SerialInputlineEdit){
        ui->SerialInputlineEdit->setPlaceholderText("Enter command,e.g. ping / status / led on");
    }

    if (!m_serial.open("/dev/ttymxc2", 115200)) {
        statusBar()->showMessage("Serial initialization failed");
        if(ui->SerialStatusLabel)
        {
            ui->SerialStatusLabel->setText("Serial: Error");
        }
        appendSerialLog("[ERROR] open /dev/ttymxc2 failed: " + m_serial.lastError());
    } else {
        m_serial.sendLine("READY");
        statusBar()->showMessage("Serial is ready");
        if(ui->SerialStatusLabel)
        {
            ui->SerialStatusLabel->setText("Serial: Ready");
        }
    }
    appendSerialLog("Serial: Ready");
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_LedOnButton_clicked()
{
    if (!m_led.turnOn()) {
        QMessageBox::warning(this,
                             "Prompt",
                             "Failed to turn on the light\n" + m_led.lastError());
        return;
    }

    appendSerialLog("LOCAL: led on");
    statusBar()->showMessage("The LED is on", 2000);
}


void MainWindow::on_LedOffButton_clicked()
{
    if (!m_led.turnOff()) {
        QMessageBox::warning(this,
                             "Prompt",
                             "Failed to turn off the light\n" + m_led.lastError());
        return;
    }

    appendSerialLog("LOCAL: led off");
    statusBar()->showMessage("The LED is off", 2000);
}


void MainWindow::on_BeepOnButton_clicked()
{
    if (!m_beep.turnOn()) {
        QMessageBox::warning(this,
                             "Prompt",
                             "Failed to turn on the beep\n" + m_beep.lastError());
        return;
    }

    appendSerialLog("LOCAL: beep on");
    statusBar()->showMessage("The Beep is on", 2000);
}


void MainWindow::on_BeepOffButton_clicked()
{
    if (!m_beep.turnOff()) {
        QMessageBox::warning(this,
                             "Prompt",
                             "Failed to turn off the beep\n" + m_beep.lastError());
        return;
    }

    appendSerialLog("LOCAL: beep off");
    statusBar()->showMessage("The Beep is off", 2000);
}

void MainWindow::onSerialLogMessage(const QString &msg)
{
    qDebug() << msg;
    appendSerialLog(msg);
    statusBar()->showMessage(msg, 3000);
}

void MainWindow::onSerialErrorMessage(const QString &msg)
{
    qDebug() << msg;
    appendSerialLog("[ERROR]" + msg);
    statusBar()->showMessage(msg, 5000);
    if(ui->SerialStatusLabel){
        ui->SerialStatusLabel->setText("Serial: Error");
    }
}

void MainWindow::appendSerialLog(const QString &msg)
{
    if(!ui || !ui->SerialLogEdit)
    {
        return;
    }
    ui->SerialLogEdit->appendPlainText(msg);
}

void MainWindow::on_SerialSendButton_clicked()
{
    if (!m_serial.isReady()) {
        appendSerialLog("[ERROR] serial not ready");
        statusBar()->showMessage("Serial not ready", 3000);
        if (ui->SerialStatusLabel) {
            ui->SerialStatusLabel->setText("Serial: Not Ready");
        }
        return;
    }

    QString text = ui->SerialInputlineEdit->text().trimmed();
    if(text.isEmpty()){
        return;
    }
    if(!m_serial.sendLine(text))
    {
        appendSerialLog("[ERROR] send failed: " + m_serial.lastError());
        statusBar()->showMessage("Serial send failed",3000);
    }

    ui->SerialInputlineEdit->clear();

    return;
}


void MainWindow::on_SerialClearLogButton_clicked()
{
    if(ui->SerialLogEdit)
    {
        ui->SerialLogEdit->clear();
    }
}


void MainWindow::on_Ap3216cReadButton_clicked()
{
    Ap3216cData data;
    if (!m_ap3216c.isReady()) {
        QMessageBox::warning(this, "Prompt", "AP3216C device not ready");
        return;
    }

    if (!m_ap3216c.readData(data)) {
        QMessageBox::warning(this,
                             "Prompt",
                             "Read AP3216C failed:\n" + m_ap3216c.lastError());
        return;
    }
    ui->Ap3216cIrLabel->setText(QString("IR: %1").arg(data.ir));
    ui->Ap3216cAlsLabel->setText(QString("ALS: %1").arg(data.als));
    ui->Ap3216cPsLabel->setText(QString("PS: %1").arg(data.ps));
}

