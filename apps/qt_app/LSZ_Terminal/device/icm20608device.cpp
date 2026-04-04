#include "icm20608device.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


Icm20608Device::Icm20608Device(const QString &devicePath) :
    m_devicePath(devicePath),
    m_fd(-1)
{

}

Icm20608Device::~Icm20608Device()
{
    close();
}
bool Icm20608Device::open()
{
    if (m_fd >= 0) {
        return true;
    }

    QByteArray path = m_devicePath.toLocal8Bit();
    m_fd = ::open(path.constData(), O_RDWR);
    if (m_fd < 0) {
        m_lastError = QString("Failed to open %1: %2")
                          .arg(m_devicePath)
                          .arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }

    m_lastError.clear();
    return true;
}
void Icm20608Device::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    return;
}
bool Icm20608Device::isReady() const
{
    return m_fd >= 0;
}
QString Icm20608Device::lastError() const
{
    return m_lastError;
}

bool Icm20608Device::readData(Icm20608Data &data)
{
    if (m_fd < 0) {
        m_lastError = "Device not ready";
        return false;
    }
    signed short buf[7];
    ssize_t ret = ::read(m_fd,buf,sizeof(buf));
    if (ret != static_cast<ssize_t>(sizeof(buf))) {
        m_lastError = QString("Read %1 failed: ret=%2, err=%3")
                          .arg(m_devicePath)
                          .arg(ret)
                          .arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }


    data.gyroX = (float)buf[0]/16.4f;
    data.gyroY = (float)buf[1]/16.4f;
    data.gyroZ = (float)buf[2]/16.4f;
    data.accelX = (float)buf[3]/2048.0f;
    data.accelY = (float)buf[4]/2048.0f;
    data.accelZ = (float)buf[5]/2048.0f;;
    data.temp = ((float)buf[6] - 0.0f)/326.8f + 25.0f;

    m_lastError.clear();
    return true;

}

