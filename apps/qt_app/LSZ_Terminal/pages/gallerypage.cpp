#include "gallerypage.h"

#include "media/mediastore.h"

#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {
constexpr int GalleryColumns = 4;
constexpr int ThumbWidth = 150;
constexpr int ThumbHeight = 82;

bool isVideoFile(const QString &filePath)
{
    return QFileInfo(filePath).suffix().compare(QStringLiteral("avi"), Qt::CaseInsensitive) == 0;
}
}

GalleryPage::GalleryPage(QWidget *parent)
    : QWidget(parent)
    , m_contentWidget(nullptr)
    , m_gridLayout(nullptr)
    , m_emptyLabel(nullptr)
{
    setObjectName(QStringLiteral("GalleryPage"));

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 62, 24, 16);
    root->setSpacing(12);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *titleLabel = new QLabel(QStringLiteral("Gallery"), this);
    titleLabel->setObjectName(QStringLiteral("GalleryTitle"));
    QPushButton *refreshButton = new QPushButton(QStringLiteral("Refresh"), this);
    refreshButton->setCursor(Qt::PointingHandCursor);

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch(1);
    headerLayout->addWidget(refreshButton);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    m_contentWidget = new QWidget(scrollArea);
    m_gridLayout = new QGridLayout(m_contentWidget);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setHorizontalSpacing(12);
    m_gridLayout->setVerticalSpacing(12);
    scrollArea->setWidget(m_contentWidget);

    m_emptyLabel = new QLabel(QStringLiteral("No photos or videos saved"), this);
    m_emptyLabel->setObjectName(QStringLiteral("GalleryEmpty"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);

    root->addLayout(headerLayout);
    root->addWidget(scrollArea, 1);
    root->addWidget(m_emptyLabel);

    setStyleSheet(
        "#GalleryPage { background: #f5f7fb; }"
        "#GalleryTitle { color: #1f2937; font-size: 24px; font-weight: 700; }"
        "#GalleryEmpty { color: #64748b; font-size: 18px; }"
        "QPushButton {"
        "  background: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  color: #1f2937;"
        "  font-size: 16px;"
        "  padding: 8px 18px;"
        "}"
        "QPushButton:pressed { background: #e8eef7; }");

    connect(refreshButton, &QPushButton::clicked,
            this, &GalleryPage::refresh);

    refresh();
}

void GalleryPage::refresh()
{
    QString error;
    MediaStore::ensureDirectories(&error);

    clearItems();

    const QStringList files = MediaStore::mediaFiles();
    m_emptyLabel->setVisible(files.isEmpty());

    for (int i = 0; i < files.size(); ++i) {
        QWidget *card = createMediaCard(files[i]);
        m_gridLayout->addWidget(card, i / GalleryColumns, i % GalleryColumns);
    }

    m_gridLayout->setRowStretch((files.size() + GalleryColumns - 1) / GalleryColumns, 1);
    m_gridLayout->setColumnStretch(GalleryColumns, 1);
}

void GalleryPage::clearItems()
{
    while (QLayoutItem *item = m_gridLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

QWidget *GalleryPage::createMediaCard(const QString &filePath)
{
    const QFileInfo info(filePath);
    const bool video = isVideoFile(filePath);

    QFrame *card = new QFrame(m_contentWidget);
    card->setObjectName(QStringLiteral("MediaCard"));
    card->setFixedSize(180, 184);
    card->setToolTip(filePath);
    card->setStyleSheet(
        "#MediaCard {"
        "  background: #ffffff;"
        "  border: 1px solid #d8dee9;"
        "  border-radius: 8px;"
        "}");

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 10, 10, 8);
    layout->setSpacing(6);

    QLabel *thumb = new QLabel(card);
    thumb->setFixedSize(ThumbWidth, ThumbHeight);
    thumb->setAlignment(Qt::AlignCenter);
    thumb->setStyleSheet(
        "QLabel {"
        "  background: #111827;"
        "  color: #e5e7eb;"
        "  border-radius: 6px;"
        "  font-size: 18px;"
        "  font-weight: 700;"
        "}");

    if (video) {
        thumb->setText(QStringLiteral("AVI\nVideo"));
    } else {
        const QPixmap pixmap(filePath);
        if (!pixmap.isNull()) {
            thumb->setPixmap(pixmap.scaled(ThumbWidth,
                                           ThumbHeight,
                                           Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
        } else {
            thumb->setText(QStringLiteral("Photo"));
        }
    }

    QLabel *nameLabel = new QLabel(info.fileName(), card);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setWordWrap(true);
    nameLabel->setFixedHeight(34);
    nameLabel->setStyleSheet("QLabel { color: #1f2937; font-size: 12px; }");

    QLabel *typeLabel = new QLabel(video ? QStringLiteral("Video") : QStringLiteral("Photo"), card);
    typeLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    typeLabel->setStyleSheet("QLabel { color: #64748b; font-size: 12px; }");

    QPushButton *deleteButton = new QPushButton(QStringLiteral("Delete"), card);
    deleteButton->setCursor(Qt::PointingHandCursor);
    deleteButton->setFixedHeight(28);
    deleteButton->setStyleSheet(
        "QPushButton {"
        "  background: #fff1f2;"
        "  border: 1px solid #fecdd3;"
        "  border-radius: 5px;"
        "  color: #be123c;"
        "  font-size: 12px;"
        "  padding: 4px 10px;"
        "}"
        "QPushButton:pressed { background: #ffe4e6; }");

    QHBoxLayout *footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setSpacing(6);
    footerLayout->addWidget(typeLabel, 1);
    footerLayout->addWidget(deleteButton);

    connect(deleteButton, &QPushButton::clicked,
            this, [this, filePath]() {
        deleteMediaFile(filePath);
    });

    layout->addWidget(thumb, 0, Qt::AlignCenter);
    layout->addWidget(nameLabel);
    layout->addLayout(footerLayout);
    return card;
}

void GalleryPage::deleteMediaFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists()) {
        emit statusMessage(QStringLiteral("File already removed: ") + info.fileName(), 3000);
        refresh();
        return;
    }

    if (!QFile::remove(filePath)) {
        emit statusMessage(QStringLiteral("Delete failed: ") + info.fileName(), 4000);
        return;
    }

    emit statusMessage(QStringLiteral("Deleted: ") + info.fileName(), 3000);
    refresh();
}
