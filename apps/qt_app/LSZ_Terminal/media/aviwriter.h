#ifndef AVIWRITER_H
#define AVIWRITER_H

#include <QFile>
#include <QImage>
#include <QSize>
#include <QString>
#include <QVector>

class AviWriter
{
public:
    AviWriter();
    ~AviWriter();

    bool open(const QString &filePath, const QSize &frameSize, int fps, QString *error = nullptr);
    bool addFrame(const QImage &image, QString *error = nullptr);
    bool close(QString *error = nullptr);

    bool isOpen() const;
    int frameCount() const;
    QString filePath() const;

private:
    struct IndexEntry {
        quint32 offset = 0;
        quint32 size = 0;
    };

private:
    bool writeHeader(QString *error);
    bool writeFourcc(const char *fourcc);
    bool writeU16(quint16 value);
    bool writeU32(quint32 value);
    bool patchU32(qint64 pos, quint32 value);
    void setError(QString *error, const QString &message) const;

private:
    QFile m_file;
    QSize m_frameSize;
    int m_fps;
    QVector<IndexEntry> m_index;
    qint64 m_riffSizePos;
    qint64 m_totalFramesPos;
    qint64 m_streamLengthPos;
    qint64 m_moviListPos;
    qint64 m_moviSizePos;
    qint64 m_moviDataPos;
};

#endif // AVIWRITER_H
