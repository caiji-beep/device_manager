#ifndef BEEPDEVICE_H
#define BEEPDEVICE_H

#include "device/outputdevice.h"
#include <QString>

class BeepDevice : public OutputDevice
{
public:
    explicit BeepDevice(const QString &devicePath = "/dev/beep");
    ~BeepDevice();
    bool open();
    void shutdown();
    bool turnOn();
    bool turnOff();

};

#endif // BEEPDEVICE_H
