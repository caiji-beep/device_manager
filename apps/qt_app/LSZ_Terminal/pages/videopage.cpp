#include "videopage.h"

#include "workers/videoworker.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPushButton>
#include <QPixmap>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>

VideoPage::VideoPage(QWidget *parent)
    : QWidget(parent)
    , m_previewLabel(nullptr)
    , m_devicePathEdit(nullptr)
    , m_startMonitorButton(nullptr)
    , m_stopMonitorButton(nullptr)
    , m_captureButton(nullptr)
    , m_startRecordButton(nullptr)
    , m_stopRecordButton(nullptr)
    , m_logEdit(nullptr)
    , m_videoThread(nullptr)
    , m_videoWorker(nullptr)
{
    initUi();
    initConnections();
}

VideoPage::~VideoPage()
{
    onStopMonitorClicked();
}

void VideoPage::initUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setMinimumSize(640, 320);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setText("Video Preview");
    m_previewLabel->setStyleSheet(
        "QLabel {"
        "  background: #111827;"
        "  border: 1px solid #374151;"
        "  color: #e5e7eb;"
        "  font-size: 22px;"
        "}");

    QLabel *devicePathLabel = new QLabel("Path:", this);
    m_devicePathEdit = new QLineEdit(this);
    m_devicePathEdit->setText("/dev/video2");

    QHBoxLayout *deviceLayout = new QHBoxLayout();
    deviceLayout->addWidget(devicePathLabel);
    deviceLayout->addWidget(m_devicePathEdit, 1);

    m_startMonitorButton = new QPushButton("Start monitoring", this);
    m_stopMonitorButton = new QPushButton("Stop monitoring", this);
    m_captureButton = new QPushButton("Save", this);
    m_startRecordButton = new QPushButton("Start recording", this);
    m_stopRecordButton = new QPushButton("Stop recording", this);
    m_stopMonitorButton->setEnabled(false);

    QGridLayout *buttonLayout = new QGridLayout();
    buttonLayout->setHorizontalSpacing(12);
    buttonLayout->setVerticalSpacing(10);
    buttonLayout->addWidget(m_startMonitorButton, 0, 0);
    buttonLayout->addWidget(m_stopMonitorButton, 0, 1);
    buttonLayout->addWidget(m_captureButton, 0, 2);
    buttonLayout->addWidget(m_startRecordButton, 1, 0);
    buttonLayout->addWidget(m_stopRecordButton, 1, 1);

    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMinimumHeight(120);

    mainLayout->addWidget(m_previewLabel, 1);
    mainLayout->addLayout(deviceLayout);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_logEdit);

    appendLog("Video page initialized");
}

void VideoPage::initConnections()
{
    connect(m_startMonitorButton, &QPushButton::clicked,
            this, &VideoPage::onStartMonitorClicked);
    connect(m_stopMonitorButton, &QPushButton::clicked,
            this, &VideoPage::onStopMonitorClicked);
    connect(m_captureButton, &QPushButton::clicked,
            this, &VideoPage::onCaptureClicked);
    connect(m_startRecordButton, &QPushButton::clicked,
            this, &VideoPage::onStartRecordClicked);
    connect(m_stopRecordButton, &QPushButton::clicked,
            this, &VideoPage::onStopRecordClicked);
}

void VideoPage::onStartMonitorClicked()
{
    if (m_videoThread || m_videoWorker) {
        appendLog("Video monitor is already running");
        return;
    }

    const QString devicePath = m_devicePathEdit->text().trimmed();
    if (devicePath.isEmpty()) {
        appendLog("Device path is empty");
        return;
    }

    m_videoThread = new QThread(this);
    m_videoWorker = new VideoWorker();
    m_videoWorker->moveToThread(m_videoThread);

    connect(m_videoThread, &QThread::started,
            m_videoWorker, [this, devicePath]() {
        m_videoWorker->start(devicePath, 640, 480);
        m_videoThread->quit();//quit() 会告诉子线程可以结束事件循环finish
    });

    connect(m_videoWorker, &VideoWorker::frameReady,
            this, &VideoPage::onFrameReady);

    connect(m_videoWorker, &VideoWorker::logMessage,
            this, &VideoPage::appendLog);

    connect(m_videoWorker, &VideoWorker::errorMessage,
            this, [this](const QString &msg) {
        appendLog("[ERROR] " + msg);
    });

    connect(m_videoThread, &QThread::finished,
            this, &VideoPage::cleanupWorker);

    m_startMonitorButton->setEnabled(false);
    m_stopMonitorButton->setEnabled(true);
    m_devicePathEdit->setEnabled(false);

    appendLog("Start monitor clicked, device: " + devicePath);
    m_videoThread->start();
}

void VideoPage::onStopMonitorClicked()
{
    if (!m_videoThread || !m_videoWorker) {
        return;
    }

    appendLog("Stop monitor clicked");

    QMetaObject::invokeMethod(m_videoWorker, "stop", Qt::QueuedConnection);
    m_videoThread->quit();
    m_videoThread->wait();
    cleanupWorker();
}

void VideoPage::onFrameReady(const QImage &image)
{
    m_lastFrame = image;
    m_previewLabel->setPixmap(QPixmap::fromImage(m_lastFrame)
                              .scaled(m_previewLabel->size(),
                                      Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation));
}

void VideoPage::onCaptureClicked()
{
    appendLog("Capture image clicked");
}

void VideoPage::onStartRecordClicked()
{
    appendLog("Start record clicked");
}

void VideoPage::onStopRecordClicked()
{
    appendLog("Stop record clicked");
}

void VideoPage::cleanupWorker()
{
    if (m_videoWorker) {
        delete m_videoWorker;
        m_videoWorker = nullptr;
    }

    if (m_videoThread) {
        delete m_videoThread;
        m_videoThread = nullptr;
    }

    m_startMonitorButton->setEnabled(true);
    m_stopMonitorButton->setEnabled(false);
    m_devicePathEdit->setEnabled(true);
}

void VideoPage::appendLog(const QString &message)
{
    const QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_logEdit->append(QString("[%1] %2").arg(time, message));
}
