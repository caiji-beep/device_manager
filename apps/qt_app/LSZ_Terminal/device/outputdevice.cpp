#include "outputdevice.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

OutputDevice::OutputDevice(const QString &devicePath)
    : m_devicePath(devicePath),
      m_fd(-1)
{

}

OutputDevice::~OutputDevice()
{
    close();

}
bool OutputDevice::open()
{
    if(m_fd >= 0)
        return true;
    QByteArray path = m_devicePath.toLocal8Bit();
    m_fd = ::open(path.constData(),O_RDWR);

    if(m_fd < 0){
        m_lastError = QString("Failed to open device: %1").arg(strerror(errno));
        return false;
    }
    m_lastError.clear();
    return true;
}
void OutputDevice::close()
{
    if(m_fd >= 0){
        ::close(m_fd);
        m_fd = -1;
    }

}


bool OutputDevice::isReady() const
{
    return m_fd >= 0;
}

QString OutputDevice::lastError() const
{
    return m_lastError;
}
bool OutputDevice::writeValue(unsigned char value)
{
    if(m_fd < 0)
    {
        m_lastError = "Device not ready";
        return false;
    }
    ssize_t ret = ::write(m_fd,&value,1);

    if (ret != 1) {
        m_lastError = QString("Failed to write device: %1").arg(strerror(errno));
        return false;
    }

    m_lastError.clear();
    return true;
}

