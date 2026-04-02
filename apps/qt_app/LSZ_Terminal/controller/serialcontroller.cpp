#include "serialcontroller.h"

#include "device/serialdevice.h"
#include "device/leddevice.h"
#include "device/beepdevice.h"

SerialController::SerialController(SerialDevice *serial,
                                   LedDevice *led,
                                   BeepDevice *beep,
                                   QObject *parent)
    : QObject(parent)
    , m_serial(serial)
    , m_led(led)
    , m_beep(beep)
    , m_ledState(0)
    , m_beepState(0)
{
}

bool SerialController::ledOn()
{
    if (!m_led || !m_led->isReady()) {
        emit errorMessage("LED device not ready");
        return false;
    }

    if (!m_led->turnOn()) {
        emit errorMessage("LED on failed: " + m_led->lastError());
        return false;
    }

    m_ledState = 1;
    emit logMessage("LED -> ON");
    return true;
}

bool SerialController::ledOff()
{
    if (!m_led || !m_led->isReady()) {
        emit errorMessage("LED device not ready");
        return false;
    }

    if (!m_led->turnOff()) {
        emit errorMessage("LED off failed: " + m_led->lastError());
        return false;
    }

    m_ledState = 0;
    emit logMessage("LED -> OFF");
    return true;
}

bool SerialController::beepOn()
{
    if (!m_beep || !m_beep->isReady()) {
        emit errorMessage("Beep device not ready");
        return false;
    }

    if (!m_beep->turnOn()) {
        emit errorMessage("Beep on failed: " + m_beep->lastError());
        return false;
    }

    m_beepState = 1;
    emit logMessage("BEEP -> ON");
    return true;
}

bool SerialController::beepOff()
{
    if (!m_beep || !m_beep->isReady()) {
        emit errorMessage("Beep device not ready");
        return false;
    }

    if (!m_beep->turnOff()) {
        emit errorMessage("Beep off failed: " + m_beep->lastError());
        return false;
    }

    m_beepState = 0;
    emit logMessage("BEEP -> OFF");
    return true;
}

int SerialController::ledState() const
{
    return m_ledState;
}

int SerialController::beepState() const
{
    return m_beepState;
}

void SerialController::handleLine(const QString &line)
{
    QString resp;
    QString cmd = line.trimmed().toLower();

    emit logMessage(QString("parsed cmd: [%1]").arg(cmd));

    if (!handleCommand(cmd, resp)) {
        return;
    }

    if (m_serial && !resp.isEmpty()) {
        m_serial->sendLine(resp);
    }
}

bool SerialController::handleCommand(const QString &cmd, QString &resp)
{
    if (cmd == "ping") {
        resp = "PONG";
        return true;
    }

    if (cmd == "led on") {
        resp = ledOn() ? "OK led on" : "ERR led on";
        return true;
    }

    if (cmd == "led off") {
        resp = ledOff() ? "OK led off" : "ERR led off";
        return true;
    }

    if (cmd == "beep on") {
        resp = beepOn() ? "OK beep on" : "ERR beep on";
        return true;
    }

    if (cmd == "beep off") {
        resp = beepOff() ? "OK beep off" : "ERR beep off";
        return true;
    }

    if (cmd == "status") {
        resp = QString("STATUS led=%1 beep=%2")
                   .arg(m_ledState)
                   .arg(m_beepState);
        return true;
    }

    if (cmd == "quit") {
        resp = "BYE";
        return true;
    }

    resp = "ERR unknown cmd";
    return true;
}
