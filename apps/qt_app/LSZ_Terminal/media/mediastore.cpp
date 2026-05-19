#include "mediastore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>

#include <algorithm>

QString MediaStore::mediaRoot()
{
    QString base = QDir::homePath();
    if (base.isEmpty() || base == QStringLiteral(".")) {
        base = QCoreApplication::applicationDirPath();
    }

    return QDir(base).filePath(QStringLiteral("LSZ_Terminal_Media"));
}

QString MediaStore::photoDir()
{
    return QDir(mediaRoot()).filePath(QStringLiteral("photos"));
}

QString MediaStore::videoDir()
{
    return QDir(mediaRoot()).filePath(QStringLiteral("videos"));
}

bool MediaStore::ensureDirectories(QString *error)
{
    QDir dir;
    if (!dir.mkpath(photoDir())) {
        if (error) {
            *error = QStringLiteral("create photo directory failed: ") + photoDir();
        }
        return false;
    }

    if (!dir.mkpath(videoDir())) {
        if (error) {
            *error = QStringLiteral("create video directory failed: ") + videoDir();
        }
        return false;
    }

    return true;
}

QString MediaStore::newPhotoPath()
{
    const QString name = QDateTime::currentDateTime().toString(
                QStringLiteral("yyyyMMdd_hhmmss_zzz"));
    return QDir(photoDir()).filePath(QStringLiteral("photo_%1.jpg").arg(name));
}

QString MediaStore::newVideoPath()
{
    const QString name = QDateTime::currentDateTime().toString(
                QStringLiteral("yyyyMMdd_hhmmss_zzz"));
    return QDir(videoDir()).filePath(QStringLiteral("video_%1.avi").arg(name));
}

QStringList MediaStore::mediaFiles()
{
    QFileInfoList entries;
    const QStringList photoFilters = QStringList()
            << QStringLiteral("*.jpg")
            << QStringLiteral("*.jpeg")
            << QStringLiteral("*.png");
    const QStringList videoFilters = QStringList()
            << QStringLiteral("*.avi");

    entries.append(QDir(photoDir()).entryInfoList(photoFilters,
                                                  QDir::Files | QDir::Readable,
                                                  QDir::Time));
    entries.append(QDir(videoDir()).entryInfoList(videoFilters,
                                                  QDir::Files | QDir::Readable,
                                                  QDir::Time));

    std::sort(entries.begin(), entries.end(), [](const QFileInfo &left,
                                                 const QFileInfo &right) {
        return left.lastModified() > right.lastModified();
    });

    QStringList files;
    for (const QFileInfo &entry : entries) {
        files.append(entry.absoluteFilePath());
    }

    return files;
}
