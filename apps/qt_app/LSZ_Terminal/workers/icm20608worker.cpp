#include "icm20608worker.h"

Icm20608Worker::Icm20608Worker(const QString &devicePath, QObject *parent)
    : QObject(parent)
    , m_devicePath(devicePath)
    , m_device(devicePath)
    , m_timer(nullptr)
    , m_running(false)
{
}

Icm20608Worker::~Icm20608Worker()
{
    stopWork();
}

void Icm20608Worker::startWork(int intervalMs)
{
    if (m_running) {
        return;
    }

    if (!m_device.open()) {
        emit errorOccurred("ICM20608 open failed: " + m_device.lastError());
        return;
    }

    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setTimerType(Qt::PreciseTimer);
        connect(m_timer, &QTimer::timeout,
                this, &Icm20608Worker::onTimeout);
    }

    m_timer->start(intervalMs);
    m_running = true;
    emit started();
}

void Icm20608Worker::stopWork()
{
    if (m_timer && m_timer->isActive()) {
        m_timer->stop();
    }

    if (m_running) {
        m_device.close();
        m_running = false;
        emit stopped();
    }
}

void Icm20608Worker::onTimeout()
{
    Icm20608Data data;
    if (!m_device.readData(data)) {
        emit errorOccurred("ICM20608 read failed: " + m_device.lastError());
        return;
    }

    emit dataReady(data);
}
