#ifndef MEDIASTORE_H
#define MEDIASTORE_H

#include <QString>
#include <QStringList>

class MediaStore
{
public:
    static QString mediaRoot();
    static QString photoDir();
    static QString videoDir();
    static bool ensureDirectories(QString *error = nullptr);
    static QString newPhotoPath();
    static QString newVideoPath();
    static QStringList mediaFiles();
};

#endif // MEDIASTORE_H
