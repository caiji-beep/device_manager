#ifndef OUTPUTDEVICE_H
#define OUTPUTDEVICE_H

#include <QString>

class OutputDevice
{
public:
    explicit OutputDevice(const QString &devicePath);
    virtual ~OutputDevice();

    bool open();
    void close();
    bool isReady() const;
    QString lastError() const;
protected:
    bool writeValue(unsigned char value);

protected:
    QString m_devicePath;
    int m_fd;
    QString m_lastError;

};

#endif // OUTPUTDEVICE_H
