#include "aviwriter.h"

#include <QBuffer>

namespace {
constexpr quint32 AviHasIndex = 0x00000010;
constexpr quint32 KeyFrame = 0x00000010;
constexpr int JpegQuality = 85;

quint32 toU32(qint64 value)
{
    return value < 0 ? 0u : static_cast<quint32>(value);
}
}

AviWriter::AviWriter()
    : m_fps(30)
    , m_riffSizePos(-1)
    , m_totalFramesPos(-1)
    , m_streamLengthPos(-1)
    , m_moviListPos(-1)
    , m_moviSizePos(-1)
    , m_moviDataPos(-1)
{
}

AviWriter::~AviWriter()
{
    close();
}

bool AviWriter::open(const QString &filePath, const QSize &frameSize, int fps, QString *error)
{
    close();

    if (!frameSize.isValid() || frameSize.isEmpty()) {
        setError(error, QStringLiteral("invalid video frame size"));
        return false;
    }

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("open video file failed: ") + filePath);
        return false;
    }

    m_frameSize = frameSize;
    m_fps = fps > 0 ? fps : 30;
    m_index.clear();

    if (!writeHeader(error)) {
        m_file.close();
        m_file.remove();
        return false;
    }

    return true;
}

bool AviWriter::addFrame(const QImage &image, QString *error)
{
    if (!m_file.isOpen()) {
        setError(error, QStringLiteral("video file is not open"));
        return false;
    }

    if (image.isNull()) {
        setError(error, QStringLiteral("video frame is empty"));
        return false;
    }

    QImage frame = image;
    if (frame.size() != m_frameSize) {
        frame = frame.scaled(m_frameSize,
                             Qt::IgnoreAspectRatio,
                             Qt::FastTransformation);
    }
    if (frame.format() != QImage::Format_RGB888) {
        frame = frame.convertToFormat(QImage::Format_RGB888);
    }

    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);
    if (!frame.save(&buffer, "JPG", JpegQuality) || jpegData.isEmpty()) {
        setError(error, QStringLiteral("encode video frame failed"));
        return false;
    }

    const qint64 chunkPos = m_file.pos();
    if (!writeFourcc("00dc") || !writeU32(static_cast<quint32>(jpegData.size()))) {
        setError(error, QStringLiteral("write video frame header failed"));
        return false;
    }

    if (m_file.write(jpegData.constData(), jpegData.size()) != jpegData.size()) {
        setError(error, QStringLiteral("write video frame data failed"));
        return false;
    }

    if ((jpegData.size() & 1) != 0) {
        const char pad = '\0';
        if (m_file.write(&pad, 1) != 1) {
            setError(error, QStringLiteral("write video frame padding failed"));
            return false;
        }
    }

    IndexEntry entry;
    entry.offset = toU32(chunkPos - m_moviDataPos);
    entry.size = static_cast<quint32>(jpegData.size());
    m_index.append(entry);
    return true;
}

bool AviWriter::close(QString *error)
{
    if (!m_file.isOpen()) {
        return true;
    }

    const qint64 idx1Pos = m_file.pos();
    if (!writeFourcc("idx1") || !writeU32(static_cast<quint32>(m_index.size() * 16))) {
        setError(error, QStringLiteral("write avi index header failed"));
        m_file.close();
        return false;
    }

    for (const IndexEntry &entry : m_index) {
        if (!writeFourcc("00dc")
                || !writeU32(KeyFrame)
                || !writeU32(entry.offset)
                || !writeU32(entry.size)) {
            setError(error, QStringLiteral("write avi index entry failed"));
            m_file.close();
            return false;
        }
    }

    const qint64 fileEnd = m_file.pos();
    const quint32 frameCountValue = static_cast<quint32>(m_index.size());
    if (!patchU32(m_riffSizePos, toU32(fileEnd - 8))
            || !patchU32(m_totalFramesPos, frameCountValue)
            || !patchU32(m_streamLengthPos, frameCountValue)
            || !patchU32(m_moviSizePos, toU32(idx1Pos - (m_moviListPos + 8)))) {
        setError(error, QStringLiteral("patch avi header failed"));
        m_file.close();
        return false;
    }

    m_file.close();
    return true;
}

bool AviWriter::isOpen() const
{
    return m_file.isOpen();
}

int AviWriter::frameCount() const
{
    return m_index.size();
}

QString AviWriter::filePath() const
{
    return m_file.fileName();
}

bool AviWriter::writeHeader(QString *error)
{
    const quint32 suggestedBufferSize =
            static_cast<quint32>(m_frameSize.width() * m_frameSize.height() * 3);

    if (!writeFourcc("RIFF")) {
        setError(error, QStringLiteral("write avi riff header failed"));
        return false;
    }
    m_riffSizePos = m_file.pos();
    writeU32(0);
    writeFourcc("AVI ");

    const qint64 hdrlListPos = m_file.pos();
    writeFourcc("LIST");
    const qint64 hdrlSizePos = m_file.pos();
    writeU32(0);
    writeFourcc("hdrl");

    writeFourcc("avih");
    writeU32(56);
    writeU32(static_cast<quint32>(1000000 / m_fps));
    writeU32(suggestedBufferSize * static_cast<quint32>(m_fps));
    writeU32(0);
    writeU32(AviHasIndex);
    m_totalFramesPos = m_file.pos();
    writeU32(0);
    writeU32(0);
    writeU32(1);
    writeU32(suggestedBufferSize);
    writeU32(static_cast<quint32>(m_frameSize.width()));
    writeU32(static_cast<quint32>(m_frameSize.height()));
    writeU32(0);
    writeU32(0);
    writeU32(0);
    writeU32(0);

    const qint64 strlListPos = m_file.pos();
    writeFourcc("LIST");
    const qint64 strlSizePos = m_file.pos();
    writeU32(0);
    writeFourcc("strl");

    writeFourcc("strh");
    writeU32(56);
    writeFourcc("vids");
    writeFourcc("MJPG");
    writeU32(0);
    writeU16(0);
    writeU16(0);
    writeU32(0);
    writeU32(1);
    writeU32(static_cast<quint32>(m_fps));
    writeU32(0);
    m_streamLengthPos = m_file.pos();
    writeU32(0);
    writeU32(suggestedBufferSize);
    writeU32(0xffffffffu);
    writeU32(0);
    writeU16(0);
    writeU16(0);
    writeU16(static_cast<quint16>(m_frameSize.width()));
    writeU16(static_cast<quint16>(m_frameSize.height()));

    writeFourcc("strf");
    writeU32(40);
    writeU32(40);
    writeU32(static_cast<quint32>(m_frameSize.width()));
    writeU32(static_cast<quint32>(m_frameSize.height()));
    writeU16(1);
    writeU16(24);
    writeFourcc("MJPG");
    writeU32(suggestedBufferSize);
    writeU32(0);
    writeU32(0);
    writeU32(0);
    writeU32(0);

    const qint64 afterStrl = m_file.pos();
    patchU32(strlSizePos, toU32(afterStrl - (strlListPos + 8)));

    const qint64 afterHdrl = m_file.pos();
    patchU32(hdrlSizePos, toU32(afterHdrl - (hdrlListPos + 8)));

    m_moviListPos = m_file.pos();
    writeFourcc("LIST");
    m_moviSizePos = m_file.pos();
    writeU32(0);
    writeFourcc("movi");
    m_moviDataPos = m_file.pos();

    return m_file.error() == QFile::NoError;
}

bool AviWriter::writeFourcc(const char *fourcc)
{
    return m_file.write(fourcc, 4) == 4;
}

bool AviWriter::writeU16(quint16 value)
{
    char data[2] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff)
    };
    return m_file.write(data, sizeof(data)) == sizeof(data);
}

bool AviWriter::writeU32(quint32 value)
{
    char data[4] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff),
        static_cast<char>((value >> 24) & 0xff)
    };
    return m_file.write(data, sizeof(data)) == sizeof(data);
}

bool AviWriter::patchU32(qint64 pos, quint32 value)
{
    const qint64 oldPos = m_file.pos();
    if (!m_file.seek(pos)) {
        return false;
    }

    const bool ok = writeU32(value);
    return m_file.seek(oldPos) && ok;
}

void AviWriter::setError(QString *error, const QString &message) const
{
    if (error) {
        *error = message;
    }
}
