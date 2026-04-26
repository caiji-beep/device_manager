#include "videodevice.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

namespace {
constexpr int SupportedWidth = 640;
constexpr int SupportedHeight = 480;
constexpr int TargetFps = 30;
constexpr int BufferCount = 4;

int videoDeviceNumber(const QString &path)
{
    const QString name = QFileInfo(path).fileName();
    bool ok = false;
    const int number = name.mid(QStringLiteral("video").size()).toInt(&ok);
    return ok ? number : 10000;
}

bool isUsableVideoDevice(const QString &path)
{
    const int fd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    const bool queryOk = (::ioctl(fd, VIDIOC_QUERYCAP, &cap) >= 0);
    ::close(fd);

    if (!queryOk) {
        return false;
    }

    const unsigned int caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS)
            ? cap.device_caps
            : cap.capabilities;
    return (caps & V4L2_CAP_VIDEO_CAPTURE) && (caps & V4L2_CAP_STREAMING);
}
}

VideoDevice::VideoDevice()
    : m_devicePath()
    , m_fd(-1)
    , m_streaming(false)
{
}

VideoDevice::~VideoDevice()
{
    closeDevice();
}

QStringList VideoDevice::availableDevices()
{
    QStringList devices;
    const QFileInfoList entries = QDir(QStringLiteral("/dev")).entryInfoList(
                QStringList() << QStringLiteral("video*"),
                QDir::System | QDir::NoDotAndDotDot,
                QDir::Name);

    for (const QFileInfo &entry : entries) {
        const QString path = entry.absoluteFilePath();
        if (isUsableVideoDevice(path)) {
            devices.append(path);
        }
    }

    std::sort(devices.begin(), devices.end(), [](const QString &left, const QString &right) {
        const int leftNumber = videoDeviceNumber(left);
        const int rightNumber = videoDeviceNumber(right);
        if (leftNumber == rightNumber) {
            return left < right;
        }
        return leftNumber < rightNumber;
    });

    return devices;
}

bool VideoDevice::openDevice(const QString &devicePath)
{
    closeDevice();

    if (!devicePath.startsWith(QStringLiteral("/dev/video"))) {
        setError(QString("invalid video device path: %1").arg(devicePath));
        return false;
    }

    m_fd = ::open(devicePath.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (m_fd < 0) {
        setError(QString("open %1 failed: %2")
                 .arg(devicePath, QString::fromLocal8Bit(strerror(errno))));
        return false;
    }

    m_devicePath = devicePath;
    m_lastError.clear();
    return true;
}

bool VideoDevice::initMjpeg(int width, int height)
{
    if (m_fd < 0) {
        setError("video device is not open");
        return false;
    }

    if (width != SupportedWidth || height != SupportedHeight) {
        setError("Only 640x480 is supported in this version");
        return false;
    }

    v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (!xioctl(VIDIOC_QUERYCAP, &cap)) {
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        setError("device does not support video capture");
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        setError("device does not support streaming I/O");
        return false;
    }

    v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (!xioctl(VIDIOC_S_FMT, &fmt)) {
        return false;
    }

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
        setError("device did not accept MJPEG format");
        return false;
    }

    if (fmt.fmt.pix.width != static_cast<unsigned int>(width)
            || fmt.fmt.pix.height != static_cast<unsigned int>(height)) {
        setError(QString("device did not accept %1x%2").arg(width).arg(height));
        return false;
    }

    if (m_devicePath == QStringLiteral("/dev/video1")
            && !configureFrameRate(TargetFps)) {
        return false;
    }

    unmapBuffers();//解除用户空间内存映射

    v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = BufferCount;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (!xioctl(VIDIOC_REQBUFS, &req)) {
        return false;
    }

    if (req.count < 2) {
        setError("insufficient mmap buffers");
        return false;
    }

    m_buffers.resize(static_cast<int>(req.count));

    for (unsigned int i = 0; i < req.count; ++i) {
        v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (!xioctl(VIDIOC_QUERYBUF, &buf)) {
            unmapBuffers();
            return false;
        }

        void *start = mmap(nullptr,
                           buf.length,
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED,
                           m_fd,
                           buf.m.offset);
        if (start == MAP_FAILED) {
            setError(QString("mmap buffer %1 failed: %2")
                     .arg(i)
                     .arg(QString::fromLocal8Bit(strerror(errno))));
            unmapBuffers();
            return false;
        }

        m_buffers[static_cast<int>(i)].start = start;
        m_buffers[static_cast<int>(i)].length = buf.length;

        if (!xioctl(VIDIOC_QBUF, &buf)) {
            unmapBuffers();
            return false;
        }
    }

    m_lastError.clear();
    return true;
}

bool VideoDevice::startStream()
{
    if (m_fd < 0) {
        setError("video device is not open");
        return false;
    }

    if (m_buffers.isEmpty()) {
        setError("video buffers are not initialized");
        return false;
    }

    if (m_streaming) {
        m_lastError.clear();
        return true;
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (!xioctl(VIDIOC_STREAMON, &type)) {
        return false;
    }

    m_streaming = true;
    m_lastError.clear();
    return true;
}

bool VideoDevice::readFrame(QImage *image)
{
    if (!image) {
        setError("image output pointer is null");
        return false;
    }

    if (m_fd < 0) {
        setError("video device is not open");
        return false;
    }

    if (!m_streaming) {
        setError("video stream is not started");
        return false;
    }

    if (!waitForFrame(100)) {
        return false;
    }

    v4l2_buffer latestBuf;
    memset(&latestBuf, 0, sizeof(latestBuf));
    bool hasLatest = false;

    for (int i = 0; i < m_buffers.size(); ++i) {
        v4l2_buffer nextBuf;
        memset(&nextBuf, 0, sizeof(nextBuf));
        nextBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        nextBuf.memory = V4L2_MEMORY_MMAP;

        int ret = 0;
        do {
            ret = ::ioctl(m_fd, VIDIOC_DQBUF, &nextBuf);
        } while (ret < 0 && errno == EINTR);

        if (ret < 0) {
            if (errno == EAGAIN && hasLatest) {
                break;
            }

            setError(QString("ioctl 0x%1 failed: %2")
                     .arg(static_cast<unsigned long>(VIDIOC_DQBUF), 0, 16)
                     .arg(QString::fromLocal8Bit(strerror(errno))));
            return false;
        }

        if (nextBuf.index >= static_cast<unsigned int>(m_buffers.size())) {
            setError("driver returned an invalid buffer index");
            return false;
        }

        if (hasLatest && !xioctl(VIDIOC_QBUF, &latestBuf)) {
            xioctl(VIDIOC_QBUF, &nextBuf);
            return false;
        }

        latestBuf = nextBuf;
        hasLatest = true;
    }

    if (!hasLatest) {
        setError("no video frame dequeued");
        return false;
    }

    const Buffer &buffer = m_buffers[static_cast<int>(latestBuf.index)];
    //从 V4L2 读取到的一帧 JPEG 数据解码成 QImage 对象
    *image = QImage::fromData(static_cast<const uchar *>(buffer.start),
                              static_cast<int>(latestBuf.bytesused),
                              "JPG");

    if (image->isNull()) {
        setError("failed to decode MJPEG frame");
        xioctl(VIDIOC_QBUF, &latestBuf);
        return false;
    }

    if (image->width() != SupportedWidth || image->height() != SupportedHeight) {
        *image = image->scaled(SupportedWidth,
                               SupportedHeight,
                               Qt::IgnoreAspectRatio,
                               Qt::FastTransformation);
    }

    if (!xioctl(VIDIOC_QBUF, &latestBuf)) {
        return false;
    }

    m_lastError.clear();
    return true;
}

void VideoDevice::stopStream()
{
    if (m_fd < 0 || !m_streaming) {
        return;
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (!xioctl(VIDIOC_STREAMOFF, &type)) {
        return;
    }

    m_streaming = false;
}

void VideoDevice::closeDevice()
{
    stopStream();
    unmapBuffers();

    if (m_fd >= 0) {
        if (::close(m_fd) < 0) {
            setError(QString("close video device failed: %1")
                     .arg(QString::fromLocal8Bit(strerror(errno))));
        }
        m_fd = -1;
        m_devicePath.clear();
    }
}

QString VideoDevice::lastError() const
{
    return m_lastError;
}

bool VideoDevice::xioctl(unsigned long request, void *arg)
{
    int ret = 0;
    do {
        ret = ::ioctl(m_fd, request, arg);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
        setError(QString("ioctl 0x%1 failed: %2")
                 .arg(request, 0, 16)
                 .arg(QString::fromLocal8Bit(strerror(errno))));
        return false;
    }

    return true;
}

bool VideoDevice::configureFrameRate(int fps)
{
    v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (!xioctl(VIDIOC_G_PARM, &parm)) {
        return false;
    }

    if (!(parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
        setError("device does not support frame interval control");
        return false;
    }

    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = fps;

    if (!xioctl(VIDIOC_S_PARM, &parm)) {
        return false;
    }

    const unsigned int numerator = parm.parm.capture.timeperframe.numerator;
    const unsigned int denominator = parm.parm.capture.timeperframe.denominator;
    if (numerator == 0
            || static_cast<double>(denominator) / numerator < fps - 0.5) {
        setError(QString("device did not accept %1 fps").arg(fps));
        return false;
    }

    return true;
}

bool VideoDevice::waitForFrame(int timeoutMs)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_fd, &fds);

    timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

    int ret = 0;
    do {
        ret = ::select(m_fd + 1, &fds, nullptr, nullptr, &timeout);
    } while (ret < 0 && errno == EINTR);

    if (ret == 0) {
        setError("video frame timeout");
        return false;
    }

    if (ret < 0) {
        setError(QString("select video frame failed: %1")
                 .arg(QString::fromLocal8Bit(strerror(errno))));
        return false;
    }

    return true;
}

void VideoDevice::setError(const QString &message)
{
    m_lastError = message;
}

void VideoDevice::unmapBuffers()
{
    for (int i = 0; i < m_buffers.size(); ++i) {
        Buffer &buffer = m_buffers[i];
        if (buffer.start && buffer.length > 0) {
            //调用 munmap 解除映射
            if (munmap(buffer.start, buffer.length) < 0) {
                setError(QString("munmap buffer %1 failed: %2")
                         .arg(i)
                         .arg(QString::fromLocal8Bit(strerror(errno))));
            }
            buffer.start = nullptr;
            buffer.length = 0;
        }
    }

    m_buffers.clear();
}
