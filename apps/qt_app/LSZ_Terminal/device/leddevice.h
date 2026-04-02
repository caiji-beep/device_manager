#ifndef LEDDEVICE_H
#define LEDDEVICE_H

#include <QString>
#include "device/outputdevice.h"

class LedDevice : public OutputDevice
{
public:
    explicit LedDevice(const QString &devicePath = "/dev/led");
    ~LedDevice();

    bool open();  //打开设备
    void shutdown();//关闭设备

    bool turnOn();  //开灯
    bool turnOff(); //关灯


};

#endif // LEDDEVICE_H
