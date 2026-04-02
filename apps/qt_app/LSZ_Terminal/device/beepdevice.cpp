#include "beepdevice.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

namespace  {
    constexpr unsigned char kBeepOnValue = 1;
    constexpr unsigned char kBeepOffValue = 0;

}


BeepDevice::BeepDevice(const QString &devicePath)
    : OutputDevice(devicePath)
{

}
BeepDevice::~BeepDevice()
{
    shutdown();
}

bool BeepDevice::open()
{
    return OutputDevice::open();
}
void BeepDevice::shutdown()
{
    OutputDevice::close();
}
bool BeepDevice::turnOn()
{
    return writeValue(kBeepOnValue);
}

bool BeepDevice::turnOff()
{
    return writeValue(kBeepOffValue);
}




