#include "videopage.h"

#include "device/videodevice.h"
#include "workers/videoworker.h"

#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QPixmap>
#include <QSizePolicy>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>

VideoPage::VideoPage(QWidget *parent)
    : QWidget(parent)
    , m_previewLabel(nullptr)
    , m_deviceCombo(nullptr)
    , m_scanDevicesButton(nullptr)
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
    stopMonitor(true);
}

void VideoPage::initUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(16, 56, 16, 12);
    mainLayout->setSpacing(16);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setFixedSize(640, 480);
    m_previewLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setText("Video Preview");
    m_previewLabel->setStyleSheet(
        "QLabel {"
        "  background: #111827;"
        "  border: 1px solid #374151;"
        "  color: #e5e7eb;"
        "  font-size: 22px;"
        "}");

    QWidget *controlPanel = new QWidget(this);
    controlPanel->setFixedWidth(300);
    controlPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    QVBoxLayout *controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(12);

    QLabel *titleLabel = new QLabel("Video Control", controlPanel);
    titleLabel->setStyleSheet(
        "QLabel {"
        "  color: #1f2937;"
        "  font-size: 22px;"
        "  font-weight: 700;"
        "}");

    QLabel *devicePathLabel = new QLabel("Device:", controlPanel);
    m_deviceCombo = new QComboBox(controlPanel);
    m_deviceCombo->setMinimumContentsLength(14);
    m_deviceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_scanDevicesButton = new QPushButton("Scan", controlPanel);

    QVBoxLayout *deviceLayout = new QVBoxLayout();
    deviceLayout->setSpacing(6);
    deviceLayout->addWidget(devicePathLabel);
    deviceLayout->addWidget(m_deviceCombo, 1);
    deviceLayout->addWidget(m_scanDevicesButton);

    m_startMonitorButton = new QPushButton("Start monitoring", controlPanel);
    m_stopMonitorButton = new QPushButton("Stop monitoring", controlPanel);
    m_captureButton = new QPushButton("Save", controlPanel);
    m_startRecordButton = new QPushButton("Start recording", controlPanel);
    m_stopRecordButton = new QPushButton("Stop recording", controlPanel);
    m_stopMonitorButton->setEnabled(false);

    QGridLayout *buttonLayout = new QGridLayout();
    buttonLayout->setHorizontalSpacing(8);
    buttonLayout->setVerticalSpacing(8);
    buttonLayout->addWidget(m_startMonitorButton, 0, 0);
    buttonLayout->addWidget(m_stopMonitorButton, 0, 1);
    buttonLayout->addWidget(m_captureButton, 1, 0, 1, 2);
    buttonLayout->addWidget(m_startRecordButton, 2, 0);
    buttonLayout->addWidget(m_stopRecordButton, 2, 1);

    m_logEdit = new QTextEdit(controlPanel);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMinimumHeight(190);
    m_logEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    controlLayout->addWidget(titleLabel);
    controlLayout->addLayout(deviceLayout);
    controlLayout->addLayout(buttonLayout);
    controlLayout->addWidget(m_logEdit, 1);

    mainLayout->addWidget(controlPanel);
    mainLayout->addWidget(m_previewLabel, 1);

    refreshVideoDevices();
    appendLog("Video page initialized");
}

void VideoPage::initConnections()
{
    connect(m_scanDevicesButton, &QPushButton::clicked,
            this, &VideoPage::onScanDevicesClicked);
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

void VideoPage::onScanDevicesClicked()
{
    refreshVideoDevices();
}

void VideoPage::onStartMonitorClicked()
{
    if (m_videoThread || m_videoWorker) {
        appendLog("Video monitor is already running");
        return;
    }

    const QString devicePath = selectedDevicePath();
    if (devicePath.isEmpty()) {
        appendLog("No video device selected");
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
    m_deviceCombo->setEnabled(false);
    m_scanDevicesButton->setEnabled(false);

    appendLog("Start monitor clicked, device: " + devicePath);
    m_videoThread->start();
}

void VideoPage::onStopMonitorClicked()
{
    stopMonitor(false);
}

void VideoPage::stopMonitor(bool waitForFinished)
{
    if (!m_videoThread || !m_videoWorker) {
        return;
    }

    appendLog("Stop monitor clicked");

    m_stopMonitorButton->setEnabled(false);
    m_startMonitorButton->setEnabled(false);
    m_scanDevicesButton->setEnabled(false);

    QMetaObject::invokeMethod(m_videoWorker, "stop", Qt::DirectConnection);

    if (waitForFinished) {
        m_videoThread->wait();
    }
}

void VideoPage::onFrameReady(const QImage &image)
{
    m_lastFrame = image;

    if (!isVisible()) {
        return;
    }

    if (m_frameUpdateTimer.isValid() && m_frameUpdateTimer.elapsed() < 33) {
        return;
    }

    const QSize previewSize = m_previewLabel->contentsRect().size();
    if (!previewSize.isValid() || previewSize.isEmpty()) {
        return;
    }

    m_frameUpdateTimer.restart();
    QPixmap pixmap = QPixmap::fromImage(m_lastFrame);
    if (m_lastFrame.size() != previewSize) {
        pixmap = pixmap.scaled(previewSize,
                               Qt::KeepAspectRatio,
                               Qt::FastTransformation);
    }

    m_previewLabel->setPixmap(pixmap);
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

void VideoPage::refreshVideoDevices()
{
    const QString previousDevice = selectedDevicePath();
    const QStringList devices = VideoDevice::availableDevices();

    m_deviceCombo->clear();
    for (const QString &device : devices) {
        m_deviceCombo->addItem(device, device);
    }

    const int previousIndex = devices.indexOf(previousDevice);
    if (previousIndex >= 0) {
        m_deviceCombo->setCurrentIndex(previousIndex);
    } else {
        const int uvcIndex = devices.indexOf(QStringLiteral("/dev/video1"));
        if (uvcIndex >= 0) {
            m_deviceCombo->setCurrentIndex(uvcIndex);
        }
    }

    const bool hasDevices = !devices.isEmpty();
    if (!hasDevices) {
        m_deviceCombo->addItem("No video devices", QString());
    }

    m_deviceCombo->setEnabled(hasDevices);
    m_startMonitorButton->setEnabled(hasDevices);
    appendLog(hasDevices
              ? QString("Found video devices: %1").arg(devices.join(", "))
              : QString("No usable video devices found"));
}

QString VideoPage::selectedDevicePath() const
{
    if (!m_deviceCombo || m_deviceCombo->currentIndex() < 0) {
        return QString();
    }

    return m_deviceCombo->currentData().toString().trimmed();
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

    const bool hasDevices = !selectedDevicePath().isEmpty();
    m_startMonitorButton->setEnabled(hasDevices);
    m_stopMonitorButton->setEnabled(false);
    m_deviceCombo->setEnabled(hasDevices);
    m_scanDevicesButton->setEnabled(true);
}

void VideoPage::appendLog(const QString &message)
{
    const QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_logEdit->append(QString("[%1] %2").arg(time, message));
}
