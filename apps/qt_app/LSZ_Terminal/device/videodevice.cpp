#include "videodevice.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QDebug>

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
constexpr int TargetFps = 30;
constexpr int BufferCount = 4;
constexpr unsigned int VirtualPreparedWidthA = 640;
constexpr unsigned int VirtualPreparedHeightA = 480;
constexpr unsigned int VirtualPreparedWidthB = 800;
constexpr unsigned int VirtualPreparedHeightB = 600;

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

int clampToByte(int value)
{
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

int yuvToRgb(int y, int u, int v, int channel)
{
    const int c = y - 16;
    const int d = u - 128;
    const int e = v - 128;

    if (channel == 0) {
        return clampToByte((298 * c + 409 * e + 128) >> 8);
    }
    if (channel == 1) {
        return clampToByte((298 * c - 100 * d - 208 * e + 128) >> 8);
    }
    return clampToByte((298 * c + 516 * d + 128) >> 8);
}
}

VideoDevice::VideoDevice()
    : m_devicePath()
    , m_fd(-1)
    , m_pixelFormat(0)
    , m_width(0)
    , m_height(0)
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

QVector<VideoDevice::FormatInfo> VideoDevice::availableFormats(const QString &devicePath)
{
    QVector<FormatInfo> formats;
    const int fd = ::open(devicePath.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return formats;
    }

    const bool virtualDevice = isVirtualDevice(fd);

    for (unsigned int index = 0;; ++index) {
        v4l2_fmtdesc desc;
        memset(&desc, 0, sizeof(desc));
        desc.index = index;
        desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (::ioctl(fd, VIDIOC_ENUM_FMT, &desc) < 0) {
            break;
        }

        if (!isSupportedPixelFormat(desc.pixelformat)) {
            continue;
        }

        FormatInfo info;
        info.description = QString::fromLocal8Bit(reinterpret_cast<const char *>(desc.description));
        info.pixelFormat = desc.pixelformat;
        info.fourcc = fourccToString(desc.pixelformat);

        for (unsigned int sizeIndex = 0;; ++sizeIndex) {
            v4l2_frmsizeenum sizeEnum;
            memset(&sizeEnum, 0, sizeof(sizeEnum));
            sizeEnum.index = sizeIndex;
            sizeEnum.pixel_format = desc.pixelformat;
            if (::ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &sizeEnum) < 0) {
                break;
            }

            if (sizeEnum.type != V4L2_FRMSIZE_TYPE_DISCRETE) {
                continue;
            }

            const QSize size(static_cast<int>(sizeEnum.discrete.width),
                             static_cast<int>(sizeEnum.discrete.height));
            if (virtualDevice && desc.pixelformat == V4L2_PIX_FMT_MJPEG) {
                const bool preparedSize =
                        (sizeEnum.discrete.width == VirtualPreparedWidthA
                         && sizeEnum.discrete.height == VirtualPreparedHeightA)
                        || (sizeEnum.discrete.width == VirtualPreparedWidthB
                            && sizeEnum.discrete.height == VirtualPreparedHeightB);
                if (!preparedSize) {
                    continue;
                }
            }

            info.frameSizes.append(size);
        }

        if (!info.frameSizes.isEmpty()) {
            formats.append(info);
        }
    }

    ::close(fd);
    return formats;
}

VideoDevice::ControlInfo VideoDevice::brightnessInfo(const QString &devicePath)
{
    ControlInfo info;
    queryControl(devicePath, V4L2_CID_BRIGHTNESS, &info);
    return info;
}

VideoDevice::ControlInfo VideoDevice::testPatternInfo(const QString &devicePath)
{
    ControlInfo info;
    queryControl(devicePath, V4L2_CID_TEST_PATTERN, &info);
    return info;
}

QStringList VideoDevice::testPatternMenu(const QString &devicePath)
{
    QStringList items;
    const int fd = ::open(devicePath.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return items;
    }

    v4l2_queryctrl query;
    memset(&query, 0, sizeof(query));
    query.id = V4L2_CID_TEST_PATTERN;
    if (::ioctl(fd, VIDIOC_QUERYCTRL, &query) < 0 || query.type != V4L2_CTRL_TYPE_MENU) {
        ::close(fd);
        return items;
    }

    for (int index = query.minimum; index <= query.maximum; ++index) {
        v4l2_querymenu menu;
        memset(&menu, 0, sizeof(menu));
        menu.id = V4L2_CID_TEST_PATTERN;
        menu.index = static_cast<unsigned int>(index);
        if (::ioctl(fd, VIDIOC_QUERYMENU, &menu) == 0) {
            items.append(QString::fromLocal8Bit(reinterpret_cast<const char *>(menu.name)));
        } else {
            items.append(QString("Pattern %1").arg(index));
        }
    }

    ::close(fd);
    return items;
}

bool VideoDevice::setBrightnessValue(const QString &devicePath, int value)
{
    return setControlValue(devicePath, V4L2_CID_BRIGHTNESS, value);
}

bool VideoDevice::setTestPatternValue(const QString &devicePath, int value)
{
    return setControlValue(devicePath, V4L2_CID_TEST_PATTERN, value);
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

bool VideoDevice::initFormat(quint32 pixelFormat, int width, int height)
{
    if (m_fd < 0) {
        setError("video device is not open");
        return false;
    }

    if (!isSupportedPixelFormat(pixelFormat)) {
        setError(QString("unsupported pixel format: %1").arg(fourccToString(pixelFormat)));
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
    fmt.fmt.pix.pixelformat = pixelFormat;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (!xioctl(VIDIOC_S_FMT, &fmt)) {
        return false;
    }

    if (fmt.fmt.pix.pixelformat != pixelFormat) {
        setError(QString("device did not accept %1 format").arg(fourccToString(pixelFormat)));
        return false;
    }

    if (fmt.fmt.pix.width != static_cast<unsigned int>(width)
            || fmt.fmt.pix.height != static_cast<unsigned int>(height)) {
        setError(QString("device did not accept %1x%2").arg(width).arg(height));
        return false;
    }

    if (!configureFrameRate(TargetFps)) {
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
    m_pixelFormat = pixelFormat;
    m_width = width;
    m_height = height;
    return true;
}

bool VideoDevice::initMjpeg(int width, int height)
{
    return initFormat(V4L2_PIX_FMT_MJPEG, width, height);
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
    bool decodeOk = false;
    if (m_pixelFormat == V4L2_PIX_FMT_MJPEG) {
        decodeOk = decodeMjpegFrame(buffer.start, static_cast<int>(latestBuf.bytesused), image);
    } else if (m_pixelFormat == V4L2_PIX_FMT_YUYV) {
        decodeOk = decodeYuyvFrame(buffer.start, static_cast<int>(latestBuf.bytesused), image);
    } else {
        setError(QString("unsupported frame format: %1").arg(fourccToString(m_pixelFormat)));
    }

    if (!xioctl(VIDIOC_QBUF, &latestBuf)) {
        return false;
    }

    if (!decodeOk) {
        return false;
    }

    m_lastError.clear();
    return true;
}

bool VideoDevice::setBrightness(int value)
{
    return setControl(V4L2_CID_BRIGHTNESS, value);
}

bool VideoDevice::setTestPattern(int value)
{
    return setControl(V4L2_CID_TEST_PATTERN, value);
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
        m_pixelFormat = 0;
        m_width = 0;
        m_height = 0;
    }
}

QString VideoDevice::lastError() const
{
    return m_lastError;
}

bool VideoDevice::queryControl(const QString &devicePath, quint32 controlId, ControlInfo *info)
{
    if (!info) {
        return false;
    }

    const int fd = ::open(devicePath.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    v4l2_queryctrl query;
    memset(&query, 0, sizeof(query));
    query.id = controlId;
    if (::ioctl(fd, VIDIOC_QUERYCTRL, &query) < 0
            || (query.flags & V4L2_CTRL_FLAG_DISABLED)) {
        ::close(fd);
        return false;
    }

    v4l2_control control;
    memset(&control, 0, sizeof(control));
    control.id = controlId;
    if (::ioctl(fd, VIDIOC_G_CTRL, &control) < 0) {
        control.value = query.default_value;
    }

    info->available = true;
    info->minimum = query.minimum;
    info->maximum = query.maximum;
    info->step = query.step > 0 ? query.step : 1;
    info->defaultValue = query.default_value;
    info->value = control.value;

    ::close(fd);
    return true;
}

bool VideoDevice::setControlValue(const QString &devicePath, quint32 controlId, int value)
{
    const int fd = ::open(devicePath.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    v4l2_control control;
    memset(&control, 0, sizeof(control));
    control.id = controlId;
    control.value = value;

    const bool ok = (::ioctl(fd, VIDIOC_S_CTRL, &control) == 0);
    ::close(fd);
    return ok;
}

QString VideoDevice::fourccToString(quint32 pixelFormat)
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

bool VideoDevice::isSupportedPixelFormat(quint32 pixelFormat)
{
    return pixelFormat == V4L2_PIX_FMT_MJPEG
            || pixelFormat == V4L2_PIX_FMT_YUYV;
}

bool VideoDevice::isVirtualDevice(int fd)
{
    v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (::ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        return false;
    }

    const QString driver = QString::fromLocal8Bit(reinterpret_cast<const char *>(cap.driver));
    const QString card = QString::fromLocal8Bit(reinterpret_cast<const char *>(cap.card));
    return driver.contains(QStringLiteral("virtual"), Qt::CaseInsensitive)
            || card.contains(QStringLiteral("virtual"), Qt::CaseInsensitive);
}

bool VideoDevice::setControl(quint32 controlId, int value)
{
    if (m_fd < 0) {
        setError("video device is not open");
        return false;
    }

    v4l2_control control;
    memset(&control, 0, sizeof(control));
    control.id = controlId;
    control.value = value;

    if (!xioctl(VIDIOC_S_CTRL, &control)) {
        return false;
    }

    m_lastError.clear();
    return true;
}

bool VideoDevice::decodeMjpegFrame(const void *data, int bytesUsed, QImage *image)
{
    *image = QImage::fromData(static_cast<const uchar *>(data), bytesUsed, "JPG");
    if (image->isNull()) {
        setError("failed to decode MJPEG frame");
        return false;
    }

    return true;
}

bool VideoDevice::decodeYuyvFrame(const void *data, int bytesUsed, QImage *image)
{
    const int expectedBytes = m_width * m_height * 2;
    if (m_width <= 0 || m_height <= 0 || bytesUsed < expectedBytes) {
        setError("invalid YUYV frame size");
        return false;
    }

    QImage rgbImage(m_width, m_height, QImage::Format_RGB888);
    const uchar *src = static_cast<const uchar *>(data);

    for (int y = 0; y < m_height; ++y) {
        uchar *dst = rgbImage.scanLine(y);
        for (int x = 0; x < m_width; x += 2) {
            const int y0 = *src++;
            const int u = *src++;
            const int y1 = *src++;
            const int v = *src++;

            *dst++ = static_cast<uchar>(yuvToRgb(y0, u, v, 0));
            *dst++ = static_cast<uchar>(yuvToRgb(y0, u, v, 1));
            *dst++ = static_cast<uchar>(yuvToRgb(y0, u, v, 2));

            *dst++ = static_cast<uchar>(yuvToRgb(y1, u, v, 0));
            *dst++ = static_cast<uchar>(yuvToRgb(y1, u, v, 1));
            *dst++ = static_cast<uchar>(yuvToRgb(y1, u, v, 2));
        }
    }

    *image = rgbImage;
    return true;
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
        return true;//不支持调帧率就用默认的帧率
    }


    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = fps;

    if (!xioctl(VIDIOC_S_PARM, &parm)) {
        return false;
    }

    const unsigned int numerator = parm.parm.capture.timeperframe.numerator;//分子
    const unsigned int denominator = parm.parm.capture.timeperframe.denominator;//分母
    if (numerator != 0) {
        double actual_fps = static_cast<double>(denominator) / numerator;
        // 只要不是 0，我们就接受 打印一条 Log 记录一下
        // （假设你有打 log 的宏或函数，没有的话可以直接 qDebug）
        fprintf(stderr, "\n\n=====> [HARDWARE INFO] Req FPS: %d, Actual FPS: %f <=====\n\n", fps, actual_fps);
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
