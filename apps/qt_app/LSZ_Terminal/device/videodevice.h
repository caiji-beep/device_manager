#ifndef VIDEODEVICE_H
#define VIDEODEVICE_H

#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

class VideoDevice
{
public:
    struct FormatInfo {
        QString description;
        quint32 pixelFormat = 0;
        QString fourcc;
        QVector<QSize> frameSizes;
    };

    struct ControlInfo {
        bool available = false;
        int minimum = 0;
        int maximum = 0;
        int step = 1;
        int defaultValue = 0;
        int value = 0;
    };

    VideoDevice();
    ~VideoDevice();

    static QStringList availableDevices();
    static QVector<FormatInfo> availableFormats(const QString &devicePath);
    static ControlInfo brightnessInfo(const QString &devicePath);
    static ControlInfo testPatternInfo(const QString &devicePath);
    static QStringList testPatternMenu(const QString &devicePath);
    static bool setBrightnessValue(const QString &devicePath, int value);
    static bool setTestPatternValue(const QString &devicePath, int value);

    bool openDevice(const QString &devicePath);
    bool initFormat(quint32 pixelFormat, int width, int height);
    bool initMjpeg(int width, int height);
    bool startStream();
    bool readFrame(QImage *image);
    bool setBrightness(int value);
    bool setTestPattern(int value);
    void stopStream();
    void closeDevice();
    QString lastError() const;

private:
    struct Buffer {
        void *start = nullptr;
        unsigned int length = 0;
    };

private:
    static bool queryControl(const QString &devicePath, quint32 controlId, ControlInfo *info);
    static bool setControlValue(const QString &devicePath, quint32 controlId, int value);
    static QString fourccToString(quint32 pixelFormat);
    static bool isSupportedPixelFormat(quint32 pixelFormat);
    static bool isVirtualDevice(int fd);

private:
    bool setControl(quint32 controlId, int value);
    bool decodeMjpegFrame(const void *data, int bytesUsed, QImage *image);
    bool decodeYuyvFrame(const void *data, int bytesUsed, QImage *image);
    bool configureFrameRate(int fps);
    bool waitForFrame(int timeoutMs);
    bool xioctl(unsigned long request, void *arg);
    void setError(const QString &message);
    void unmapBuffers();

private:
    QString m_devicePath;
    int m_fd;
    quint32 m_pixelFormat;
    int m_width;
    int m_height;
    QVector<Buffer> m_buffers;
    bool m_streaming;
    QString m_lastError;
};

#endif // VIDEODEVICE_H
