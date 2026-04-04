#ifndef AP3216CWORKER_H
#define AP3216CWORKER_H

#include <QObject>
#include <QTimer>
#include "device/ap3216cdevice.h"

class Ap3216cWorker : public QObject
{
    Q_OBJECT

public:
    explicit Ap3216cWorker(const QString &devicePath = "/dev/ap3216c", QObject *parent = nullptr);
    ~Ap3216cWorker();

public slots:
    void startWork(int intervalMs);
    void stopWork();

private slots:
    void onTimeout();

signals:
    void started();
    void stopped();
    void errorOccurred(const QString &msg);
    void dataReady(const Ap3216cData &data);

private:
    QString m_devicePath;
    Ap3216cDevice m_device;
    QTimer *m_timer;
    bool m_running;
};

#endif // AP3216CWORKER_H
