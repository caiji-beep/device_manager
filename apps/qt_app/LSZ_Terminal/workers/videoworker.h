#ifndef VIDEOWORKER_H
#define VIDEOWORKER_H

#include <QObject>
#include <QImage>
#include <QString>

#include <atomic>

#include "device/videodevice.h"

class VideoWorker : public QObject
{
    Q_OBJECT

public:
    explicit VideoWorker(QObject *parent = nullptr);
    ~VideoWorker() override;

public slots:
    void start(const QString &devicePath, quint32 pixelFormat, int width, int height);
    void setBrightness(int value);
    void setTestPattern(int value);
    void stop();

signals:
    void frameReady(const QImage &image);
    void errorMessage(const QString &msg);
    void logMessage(const QString &msg);

private:
    VideoDevice m_device;
    std::atomic_bool m_running;
};

#endif // VIDEOWORKER_H
