#ifndef GALLERYPAGE_H
#define GALLERYPAGE_H

#include <QWidget>

class QGridLayout;
class QLabel;
class QWidget;

class GalleryPage : public QWidget
{
    Q_OBJECT

public:
    explicit GalleryPage(QWidget *parent = nullptr);

signals:
    void statusMessage(const QString &message, int timeout);

public slots:
    void refresh();

private:
    void clearItems();
    QWidget *createMediaCard(const QString &filePath);
    void deleteMediaFile(const QString &filePath);

private:
    QWidget *m_contentWidget;
    QGridLayout *m_gridLayout;
    QLabel *m_emptyLabel;
};

#endif // GALLERYPAGE_H
