#include "leddevice.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

namespace {

    constexpr unsigned char kLedOnValue = 1;
    constexpr unsigned char kLedOffValue = 0;


}

LedDevice::LedDevice(const QString &devicePath)
    : OutputDevice(devicePath)
{

}
LedDevice::~LedDevice()
{
    shutdown();
}

bool LedDevice::open()
{
    return OutputDevice::open();
}

void LedDevice::shutdown()
{
    OutputDevice::close();
}

bool LedDevice::turnOn()
{
    return writeValue(kLedOnValue);
}

bool LedDevice::turnOff()
{
    return writeValue(kLedOffValue);
}

