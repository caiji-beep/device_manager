#ifndef ICM20608DEVICE_H
#define ICM20608DEVICE_H


#include <QString>

struct Icm20608Data
{
    float accelX;
    float accelY;
    float accelZ;
    float gyroX;
    float gyroY;
    float gyroZ;
    float temp;

};

class Icm20608Device
{
public:
    explicit Icm20608Device(const QString &devicePath = "/dev/icm20608");
    ~Icm20608Device();
    bool open();
    void close();
    bool isReady() const;
    QString lastError() const;

    bool readData(Icm20608Data &data);
private:
    QString m_devicePath;
    int m_fd;
    QString m_lastError;
};

#endif // ICM20608DEVICE_H
