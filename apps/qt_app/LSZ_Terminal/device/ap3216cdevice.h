#ifndef AP3216CDEVICE_H
#define AP3216CDEVICE_H

#include <QString>
#include <QMetaType>

struct Ap3216cData
{
    unsigned short ir = 0;
    unsigned short als = 0;
    unsigned short ps = 0;
};
Q_DECLARE_METATYPE(Ap3216cData)

class Ap3216cDevice
{
public:
    explicit Ap3216cDevice(const QString &devicePath = "/dev/ap3216c");
    ~Ap3216cDevice();

    bool open();
    void close();
    bool isReady() const;
    QString lastError() const;

    bool readData(Ap3216cData &data);


private:
    QString m_devicePath;
    int m_fd;
    QString m_lastError;

};

#endif // AP3216CDEVICE_H
