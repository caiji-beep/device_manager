#include "ap3216cdevice.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

Ap3216cDevice::Ap3216cDevice(const QString &devicePath)
    : m_devicePath(devicePath)
    , m_fd(-1)
{
}

Ap3216cDevice::~Ap3216cDevice()
{
    close();
}

bool Ap3216cDevice::open()
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

void Ap3216cDevice::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool Ap3216cDevice::isReady() const
{
    return m_fd >= 0;
}

QString Ap3216cDevice::lastError() const
{
    return m_lastError;
}

bool Ap3216cDevice::readData(Ap3216cData &data)
{
    if (m_fd < 0) {
        m_lastError = "Device not ready";
        return false;
    }

    unsigned short buf[3] = {0};
    ssize_t ret = ::read(m_fd, buf, sizeof(buf));

    if (ret != static_cast<ssize_t>(sizeof(buf))) {
        m_lastError = QString("Read %1 failed: ret=%2, err=%3")
                          .arg(m_devicePath)
                          .arg(ret)
                          .arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }

    data.ir  = buf[0];
    data.als = buf[1];
    data.ps  = buf[2];

    m_lastError.clear();
    return true;
}
