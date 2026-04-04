#include "ap3216cworker.h"

Ap3216cWorker::Ap3216cWorker(const QString &devicePath, QObject *parent)
    : QObject(parent)
    , m_devicePath(devicePath)
    , m_device(devicePath)
    , m_timer(nullptr)
    , m_running(false)
{
}

Ap3216cWorker::~Ap3216cWorker()
{
    stopWork();
}

void Ap3216cWorker::startWork(int intervalMs)
{
    if (m_running) {
        return;
    }

    if (!m_device.open()) {
        emit errorOccurred("AP3216C open failed: " + m_device.lastError());
        return;
    }

    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setTimerType(Qt::PreciseTimer);
        connect(m_timer, &QTimer::timeout,
                this, &Ap3216cWorker::onTimeout);
    }

    m_timer->start(intervalMs);
    m_running = true;
    emit started();
}

void Ap3216cWorker::stopWork()
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

void Ap3216cWorker::onTimeout()
{
    Ap3216cData data;
    if (!m_device.readData(data)) {
        emit errorOccurred("AP3216C read failed: " + m_device.lastError());
        return;
    }

    emit dataReady(data);
}
