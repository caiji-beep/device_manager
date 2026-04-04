#include "serialpage.h"
#include "ui_serialpage.h"

SerialPage::SerialPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SerialPage)
    , m_serial(nullptr)
{
    ui->setupUi(this);

    ui->SerialLogEdit->setReadOnly(true);
    ui->SerialInputlineEdit->setPlaceholderText("Enter command, e.g. ping / status / led on");

    connect(ui->SerialInputlineEdit, &QLineEdit::returnPressed,
            this, &SerialPage::on_SerialSendButton_clicked);
}

SerialPage::~SerialPage()
{
    delete ui;
}

void SerialPage::setSerialDevice(SerialDevice *serial)
{
    m_serial = serial;
}

void SerialPage::setSerialReady(bool ok)
{
    ui->SerialStatusLabel->setText(ok ? "Serial: Ready" : "Serial: Error");
}

void SerialPage::appendLogMessage(const QString &msg)
{
    ui->SerialLogEdit->appendPlainText(msg);
}

void SerialPage::appendErrorMessage(const QString &msg)
{
    ui->SerialLogEdit->appendPlainText("[ERROR] " + msg);
    ui->SerialStatusLabel->setText("Serial: Error");
}

void SerialPage::on_SerialSendButton_clicked()
{
    if (!m_serial || !m_serial->isReady()) {
        appendErrorMessage("serial not ready");
        emit statusMessage("Serial not ready", 3000);
        return;
    }

    QString text = ui->SerialInputlineEdit->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    if (!m_serial->sendLine(text)) {
        appendErrorMessage("send failed: " + m_serial->lastError());
        emit statusMessage("Serial send failed", 3000);
        return;
    }

    ui->SerialInputlineEdit->clear();
    emit statusMessage("Serial send success", 1500);
}

void SerialPage::on_SerialClearLogButton_clicked()
{
    ui->SerialLogEdit->clear();
}
