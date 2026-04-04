#ifndef ICM20608WORKER_H
#define ICM20608WORKER_H


#include <QObject>
#include <QTimer>

#include "device/icm20608device.h"

class Icm20608Worker : public QObject
{
    Q_OBJECT

public:
    explicit Icm20608Worker(const QString &devicePath = "/dev/icm20608", QObject *parent = nullptr);
    ~Icm20608Worker();

public slots:
    void startWork(int intervalMs);
    void stopWork();

private slots:
    void onTimeout();

signals:
    void started();
    void stopped();
    void errorOccurred(const QString &msg);
    void dataReady(const Icm20608Data &data);

private:
    QString m_devicePath;
    Icm20608Device m_device;
    QTimer *m_timer;
    bool m_running;
};

#endif // ICM20608WORKER_H
