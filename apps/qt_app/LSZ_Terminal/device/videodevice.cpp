#include "videodevice.h"

#include <QByteArray>

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {
constexpr int SupportedWidth = 640;
constexpr int SupportedHeight = 480;
constexpr int BufferCount = 32;
}

VideoDevice::VideoDevice()
    : m_fd(-1)
    , m_streaming(false)
{
}

VideoDevice::~VideoDevice()
{
    closeDevice();
}

bool VideoDevice::openDevice(const QString &devicePath)
{
    closeDevice();

    if (devicePath != "/dev/video2" && devicePath != "/dev/video1") {
        setError("Only /dev/video1 and  /dev/video2 is supported in this version");
        return false;
    }

    m_fd = ::open(devicePath.toLocal8Bit().constData(), O_RDWR);
    if (m_fd < 0) {
        setError(QString("open %1 failed: %2")
                 .arg(devicePath, QString::fromLocal8Bit(strerror(errno))));
        return false;
    }

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

    v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (!xioctl(VIDIOC_DQBUF, &buf)) {
        return false;
    }

    if (buf.index >= static_cast<unsigned int>(m_buffers.size())) {
        setError("driver returned an invalid buffer index");
        return false;
    }

    const Buffer &buffer = m_buffers[static_cast<int>(buf.index)];
    //从 V4L2 读取到的一帧 JPEG 数据解码成 QImage 对象
    *image = QImage::fromData(static_cast<const uchar *>(buffer.start),
                              static_cast<int>(buf.bytesused),
                              "JPG");

    if (image->isNull()) {
        setError("failed to decode MJPEG frame");
        xioctl(VIDIOC_QBUF, &buf);
        return false;
    }

    if (!xioctl(VIDIOC_QBUF, &buf)) {
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
    }
}

QString VideoDevice::lastError() const
{
    return m_lastError;
}

bool VideoDevice::xioctl(unsigned long request, void *arg)
{
    if (::ioctl(m_fd, request, arg) < 0) {
        setError(QString("ioctl 0x%1 failed: %2")
                 .arg(request, 0, 16)
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
