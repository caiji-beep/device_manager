#ifndef SERIALCONTROLLER_H
#define SERIALCONTROLLER_H

#include <QObject>
#include <QString>

class SerialDevice;
class LedDevice;
class BeepDevice;

class SerialController : public QObject
{
    Q_OBJECT

public:
    explicit SerialController(SerialDevice *serial,
                              LedDevice *led,
                              BeepDevice *beep,
                              QObject *parent = nullptr);

    bool ledOn();
    bool ledOff();
    bool beepOn();
    bool beepOff();

    int ledState() const;
    int beepState() const;

signals:
    void logMessage(const QString &msg);
    void errorMessage(const QString &msg);

public slots:
    void handleLine(const QString &line);

private:
    bool handleCommand(const QString &cmd, QString &resp);

private:
    SerialDevice *m_serial;
    LedDevice *m_led;
    BeepDevice *m_beep;

    int m_ledState;
    int m_beepState;
};

#endif // SERIALCONTROLLER_H
