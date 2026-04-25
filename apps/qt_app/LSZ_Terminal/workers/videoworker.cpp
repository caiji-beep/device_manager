#include "videoworker.h"

#include <QCoreApplication>
#include <QThread>

VideoWorker::VideoWorker(QObject *parent)
    : QObject(parent)
    , m_running(false)
{
}

VideoWorker::~VideoWorker()
{
    stop();
    m_device.closeDevice();
}

void VideoWorker::start(const QString &devicePath, int width, int height)
{
    if (m_running.load()) {
        emit logMessage("video worker is already running");
        return;
    }

    emit logMessage(QString("opening video device: %1").arg(devicePath));

    if (!m_device.openDevice(devicePath)) {
        emit errorMessage(m_device.lastError());
        return;
    }

    if (!m_device.initMjpeg(width, height)) {
        emit errorMessage(m_device.lastError());
        m_device.closeDevice();
        return;
    }

    if (!m_device.startStream()) {
        emit errorMessage(m_device.lastError());
        m_device.closeDevice();
        return;
    }

    m_running.store(true);
    emit logMessage(QString("video stream started: %1x%2 MJPEG").arg(width).arg(height));

    while (m_running.load()) {
        QImage image;
        if (!m_device.readFrame(&image)) {
            emit errorMessage(m_device.lastError());
            break;
        }

        emit frameReady(image);
        QCoreApplication::processEvents();//强制处理当前线程的事件队列
        QThread::msleep(1);

    }

    m_running.store(false);
    m_device.stopStream();
    m_device.closeDevice();
    emit logMessage("video stream stopped");
}

void VideoWorker::stop()
{
    m_running.store(false);
    m_device.stopStream();
}
