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

namespace {
QString fourccToString(quint32 pixelFormat)
{
    char text[5] = {
        static_cast<char>(pixelFormat & 0xff),
        static_cast<char>((pixelFormat >> 8) & 0xff),
        static_cast<char>((pixelFormat >> 16) & 0xff),
        static_cast<char>((pixelFormat >> 24) & 0xff),
        '\0'
    };
    return QString::fromLatin1(text);
}
}

void VideoWorker::start(const QString &devicePath, quint32 pixelFormat, int width, int height)
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

    if (!m_device.initFormat(pixelFormat, width, height)) {
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
    emit logMessage(QString("video stream started: %1x%2 %3")
                    .arg(width)
                    .arg(height)
                    .arg(fourccToString(pixelFormat)));

    while (m_running.load()) {
        QImage image;
        if (!m_device.readFrame(&image)) {
            if (!m_running.load()) {
                break;
            }
            if (m_device.lastError() == QStringLiteral("video frame timeout")) {
                continue;
            }
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

void VideoWorker::setBrightness(int value)
{
    if (!m_device.setBrightness(value)) {
        emit errorMessage(m_device.lastError());
        return;
    }

    emit logMessage(QString("brightness set to %1").arg(value));
}

void VideoWorker::setTestPattern(int value)
{
    if (!m_device.setTestPattern(value)) {
        emit errorMessage(m_device.lastError());
        return;
    }

    emit logMessage(QString("test pattern set to %1").arg(value));
}

void VideoWorker::stop()
{
    m_running.store(false);
}
