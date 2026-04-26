#ifndef VIDEODEVICE_H
#define VIDEODEVICE_H

#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

class VideoDevice
{
public:
    VideoDevice();
    ~VideoDevice();

    static QStringList availableDevices();

    bool openDevice(const QString &devicePath);
    bool initMjpeg(int width, int height);
    bool startStream();
    bool readFrame(QImage *image);
    void stopStream();
    void closeDevice();
    QString lastError() const;

private:
    struct Buffer {
        void *start = nullptr;
        unsigned int length = 0;
    };

private:
    bool configureFrameRate(int fps);
    bool waitForFrame(int timeoutMs);
    bool xioctl(unsigned long request, void *arg);
    void setError(const QString &message);
    void unmapBuffers();

private:
    QString m_devicePath;
    int m_fd;
    QVector<Buffer> m_buffers;
    bool m_streaming;
    QString m_lastError;
};

#endif // VIDEODEVICE_H
