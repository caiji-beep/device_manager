#include "controlpage.h"
#include "ui_controlpage.h"

#include <QMessageBox>

ControlPage::ControlPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ControlPage)
    , m_led(nullptr)
    , m_beep(nullptr)
{
    ui->setupUi(this);
}

ControlPage::~ControlPage()
{
    delete ui;
}

void ControlPage::setDevices(LedDevice *led, BeepDevice *beep)
{
    m_led = led;
    m_beep = beep;
}

void ControlPage::setLedAvailable(bool ok)
{
    ui->LedOnButton->setEnabled(ok);
    ui->LedOffButton->setEnabled(ok);
}

void ControlPage::setBeepAvailable(bool ok)
{
    ui->BeepOnButton->setEnabled(ok);
    ui->BeepOffButton->setEnabled(ok);
}

void ControlPage::on_LedOnButton_clicked()
{
    if (!m_led || !m_led->isReady()) {
        QMessageBox::warning(this, "Prompt", "LED device not ready");
        emit statusMessage("LED device not ready", 3000);
        return;
    }

    if (!m_led->turnOn()) {
        QMessageBox::warning(this, "Prompt",
                             "Failed to turn on the light\n" + m_led->lastError());
        emit statusMessage("Failed to turn on LED", 3000);
        return;
    }

    emit statusMessage("The LED is on", 2000);
}

void ControlPage::on_LedOffButton_clicked()
{
    if (!m_led || !m_led->isReady()) {
        QMessageBox::warning(this, "Prompt", "LED device not ready");
        emit statusMessage("LED device not ready", 3000);
        return;
    }

    if (!m_led->turnOff()) {
        QMessageBox::warning(this, "Prompt",
                             "Failed to turn off the light\n" + m_led->lastError());
        emit statusMessage("Failed to turn off LED", 3000);
        return;
    }

    emit statusMessage("The LED is off", 2000);
}

void ControlPage::on_BeepOnButton_clicked()
{
    if (!m_beep || !m_beep->isReady()) {
        QMessageBox::warning(this, "Prompt", "Beep device not ready");
        emit statusMessage("Beep device not ready", 3000);
        return;
    }

    if (!m_beep->turnOn()) {
        QMessageBox::warning(this, "Prompt",
                             "Failed to turn on the beep\n" + m_beep->lastError());
        emit statusMessage("Failed to turn on Beep", 3000);
        return;
    }

    emit statusMessage("The Beep is on", 2000);
}

void ControlPage::on_BeepOffButton_clicked()
{
    if (!m_beep || !m_beep->isReady()) {
        QMessageBox::warning(this, "Prompt", "Beep device not ready");
        emit statusMessage("Beep device not ready", 3000);
        return;
    }

    if (!m_beep->turnOff()) {
        QMessageBox::warning(this, "Prompt",
                             "Failed to turn off the beep\n" + m_beep->lastError());
        emit statusMessage("Failed to turn off Beep", 3000);
        return;
    }

    emit statusMessage("The Beep is off", 2000);
}
