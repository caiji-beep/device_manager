#include "videopage.h"

#include "device/videodevice.h"
#include "media/mediastore.h"
#include "workers/videoworker.h"

#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QTextEdit>
#include <QThread>
#include <QVariant>
#include <QVBoxLayout>

VideoPage::VideoPage(QWidget *parent)
    : QWidget(parent)
    , m_previewLabel(nullptr)
    , m_deviceCombo(nullptr)
    , m_formatCombo(nullptr)
    , m_sizeCombo(nullptr)
    , m_scanDevicesButton(nullptr)
    , m_startMonitorButton(nullptr)
    , m_stopMonitorButton(nullptr)
    , m_captureButton(nullptr)
    , m_startRecordButton(nullptr)
    , m_stopRecordButton(nullptr)
    , m_brightnessSlider(nullptr)
    , m_brightnessValueLabel(nullptr)
    , m_testPatternLabel(nullptr)
    , m_testPatternCombo(nullptr)
    , m_logEdit(nullptr)
    , m_videoThread(nullptr)
    , m_videoWorker(nullptr)
    , m_formats()
    , m_recordWriter()
    , m_recordingPath()
    , m_recording(false)
{
    initUi();
    initConnections();
}

VideoPage::~VideoPage()
{
    finishRecording(false);
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
    controlLayout->setSpacing(8);

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
    deviceLayout->setSpacing(4);
    deviceLayout->addWidget(devicePathLabel);
    deviceLayout->addWidget(m_deviceCombo, 1);
    deviceLayout->addWidget(m_scanDevicesButton);

    QLabel *formatLabel = new QLabel("Format:", controlPanel);
    m_formatCombo = new QComboBox(controlPanel);
    QLabel *sizeLabel = new QLabel("Size:", controlPanel);
    m_sizeCombo = new QComboBox(controlPanel);

    QGridLayout *formatLayout = new QGridLayout();
    formatLayout->setHorizontalSpacing(8);
    formatLayout->setVerticalSpacing(4);
    formatLayout->addWidget(formatLabel, 0, 0);
    formatLayout->addWidget(m_formatCombo, 0, 1);
    formatLayout->addWidget(sizeLabel, 1, 0);
    formatLayout->addWidget(m_sizeCombo, 1, 1);

    QLabel *brightnessLabel = new QLabel("Brightness:", controlPanel);
    m_brightnessSlider = new QSlider(Qt::Horizontal, controlPanel);
    m_brightnessSlider->setRange(0, 255);
    m_brightnessSlider->setValue(128);
    m_brightnessValueLabel = new QLabel("128", controlPanel);
    m_brightnessValueLabel->setMinimumWidth(34);
    m_brightnessValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QGridLayout *controlValueLayout = new QGridLayout();
    controlValueLayout->setHorizontalSpacing(8);
    controlValueLayout->setVerticalSpacing(4);
    controlValueLayout->addWidget(brightnessLabel, 0, 0, 1, 2);
    controlValueLayout->addWidget(m_brightnessSlider, 1, 0);
    controlValueLayout->addWidget(m_brightnessValueLabel, 1, 1);

    m_testPatternLabel = new QLabel("Pattern:", controlPanel);
    m_testPatternCombo = new QComboBox(controlPanel);
    m_testPatternLabel->setVisible(false);
    m_testPatternCombo->setVisible(false);
    controlValueLayout->addWidget(m_testPatternLabel, 2, 0, 1, 2);
    controlValueLayout->addWidget(m_testPatternCombo, 3, 0, 1, 2);

    m_startMonitorButton = new QPushButton("Start monitoring", controlPanel);
    m_stopMonitorButton = new QPushButton("Stop monitoring", controlPanel);
    m_captureButton = new QPushButton("Save", controlPanel);
    m_startRecordButton = new QPushButton("Start recording", controlPanel);
    m_stopRecordButton = new QPushButton("Stop recording", controlPanel);
    m_stopMonitorButton->setEnabled(false);
    m_startRecordButton->setEnabled(false);
    m_stopRecordButton->setEnabled(false);

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
    m_logEdit->setMinimumHeight(110);
    m_logEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    controlLayout->addWidget(titleLabel);
    controlLayout->addLayout(deviceLayout);
    controlLayout->addLayout(formatLayout);
    controlLayout->addLayout(controlValueLayout);
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
    connect(m_deviceCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &VideoPage::onDeviceSelectionChanged);
    connect(m_formatCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &VideoPage::onFormatSelectionChanged);
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
    connect(m_brightnessSlider, &QSlider::valueChanged,
            this, &VideoPage::onBrightnessChanged);
    connect(m_testPatternCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &VideoPage::onTestPatternChanged);
}

void VideoPage::onScanDevicesClicked()
{
    refreshVideoDevices();
}

void VideoPage::onDeviceSelectionChanged()
{
    refreshVideoOptions();
    refreshControlOptions();
}

void VideoPage::onFormatSelectionChanged()
{
    QSignalBlocker blocker(m_sizeCombo);
    m_sizeCombo->clear();

    const int formatIndex = m_formatCombo->currentData().toInt();
    if (formatIndex < 0 || formatIndex >= m_formats.size()) {
        return;
    }

    const VideoDevice::FormatInfo &format = m_formats[formatIndex];
    for (const QSize &size : format.frameSizes) {
        m_sizeCombo->addItem(QString("%1x%2").arg(size.width()).arg(size.height()), size);
    }

    refreshControlOptions();
}

void VideoPage::onStartMonitorClicked()
{
    if (m_videoThread || m_videoWorker) {
        appendLog("Video monitor is already running");
        return;
    }

    const QString devicePath = selectedDevicePath();
    const QSize frameSize = selectedFrameSize();
    const quint32 pixelFormat = selectedPixelFormat();
    if (devicePath.isEmpty()) {
        appendLog("No video device selected");
        return;
    }
    if (pixelFormat == 0 || frameSize.isEmpty()) {
        appendLog("No video format selected");
        return;
    }

    m_videoThread = new QThread(this);
    m_videoWorker = new VideoWorker();
    m_videoWorker->moveToThread(m_videoThread);

    connect(m_videoThread, &QThread::started,
            m_videoWorker, [this, devicePath, pixelFormat, frameSize]() {
        m_videoWorker->start(devicePath, pixelFormat, frameSize.width(), frameSize.height());
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
    m_startRecordButton->setEnabled(false);
    m_stopRecordButton->setEnabled(false);
    setVideoSettingsEnabled(false);

    appendLog(QString("Start monitor clicked, device: %1, format: %2 %3x%4")
              .arg(devicePath)
              .arg(m_formatCombo->currentText())
              .arg(frameSize.width())
              .arg(frameSize.height()));
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
    finishRecording(false);

    m_stopMonitorButton->setEnabled(false);
    m_startMonitorButton->setEnabled(false);
    setVideoSettingsEnabled(false);

    QMetaObject::invokeMethod(m_videoWorker, "stop", Qt::DirectConnection);

    if (waitForFinished) {
        m_videoThread->wait();
    }
}

void VideoPage::onFrameReady(const QImage &image)
{
    m_lastFrame = image;

    if (!m_lastFrame.isNull() && m_videoWorker && !m_recording) {
        m_startRecordButton->setEnabled(true);
    }

    if (m_recording) {
        writeRecordingFrame(m_lastFrame);
    }

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
    if (m_lastFrame.isNull()) {
        appendLog("No frame available for photo");
        return;
    }

    QString error;
    if (!MediaStore::ensureDirectories(&error)) {
        appendLog("[ERROR] " + error);
        return;
    }

    const QString filePath = MediaStore::newPhotoPath();
    if (!m_lastFrame.save(filePath, "JPG", 90)) {
        appendLog("[ERROR] save photo failed: " + filePath);
        return;
    }

    appendLog("Photo saved: " + filePath);
    emit mediaSaved(filePath);
}

void VideoPage::onStartRecordClicked()
{
    if (m_recording) {
        appendLog("Recording is already running");
        return;
    }

    if (!m_videoWorker) {
        appendLog("Start monitoring before recording");
        return;
    }

    if (m_lastFrame.isNull()) {
        appendLog("No frame available for recording");
        return;
    }

    QString error;
    if (!MediaStore::ensureDirectories(&error)) {
        appendLog("[ERROR] " + error);
        return;
    }

    const QString filePath = MediaStore::newVideoPath();
    if (!m_recordWriter.open(filePath, m_lastFrame.size(), 30, &error)) {
        appendLog("[ERROR] " + error);
        return;
    }

    m_recording = true;
    m_recordingPath = filePath;
    m_recordFrameTimer.invalidate();
    m_startRecordButton->setEnabled(false);
    m_stopRecordButton->setEnabled(true);

    appendLog("Recording started: " + filePath);
    writeRecordingFrame(m_lastFrame);
}

void VideoPage::onStopRecordClicked()
{
    finishRecording(true);
}

void VideoPage::onBrightnessChanged(int value)
{
    m_brightnessValueLabel->setText(QString::number(value));
    if (!m_videoWorker) {
        VideoDevice::setBrightnessValue(selectedDevicePath(), value);
        return;
    }

    QMetaObject::invokeMethod(m_videoWorker,
                              "setBrightness",
                              Qt::QueuedConnection,
                              Q_ARG(int, value));
}

void VideoPage::onTestPatternChanged(int index)
{
    if (index < 0) {
        return;
    }

    const int value = m_testPatternCombo->currentData().toInt();
    if (!m_videoWorker) {
        VideoDevice::setTestPatternValue(selectedDevicePath(), value);
        return;
    }

    QMetaObject::invokeMethod(m_videoWorker,
                              "setTestPattern",
                              Qt::QueuedConnection,
                              Q_ARG(int, value));
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
    refreshVideoOptions();
    refreshControlOptions();
    appendLog(hasDevices
              ? QString("Found video devices: %1").arg(devices.join(", "))
              : QString("No usable video devices found"));
}

void VideoPage::refreshVideoOptions()
{
    const QString devicePath = selectedDevicePath();
    {
        QSignalBlocker formatBlocker(m_formatCombo);
        QSignalBlocker sizeBlocker(m_sizeCombo);
        m_formatCombo->clear();
        m_sizeCombo->clear();
        m_formats.clear();

        if (devicePath.isEmpty()) {
            m_formatCombo->setEnabled(false);
            m_sizeCombo->setEnabled(false);
            return;
        }

        m_formats = VideoDevice::availableFormats(devicePath);
        for (int i = 0; i < m_formats.size(); ++i) {
            const VideoDevice::FormatInfo &format = m_formats[i];
            m_formatCombo->addItem(QString("%1 (%2)")
                                   .arg(format.description)
                                   .arg(format.fourcc),
                                   i);
        }
    }

    m_formatCombo->setEnabled(!m_formats.isEmpty());
    m_sizeCombo->setEnabled(!m_formats.isEmpty());
    onFormatSelectionChanged();

    if (m_formats.isEmpty()) {
        appendLog("No supported MJPEG/YUYV formats found for " + devicePath);
    }
}

void VideoPage::refreshControlOptions()
{
    const QString devicePath = selectedDevicePath();

    const VideoDevice::ControlInfo brightness = VideoDevice::brightnessInfo(devicePath);
    {
        QSignalBlocker blocker(m_brightnessSlider);
        m_brightnessSlider->setEnabled(brightness.available);
        if (brightness.available) {
            m_brightnessSlider->setRange(brightness.minimum, brightness.maximum);
            m_brightnessSlider->setSingleStep(brightness.step);
            m_brightnessSlider->setValue(brightness.value);
            m_brightnessValueLabel->setText(QString::number(brightness.value));
        } else {
            m_brightnessSlider->setRange(0, 255);
            m_brightnessSlider->setValue(128);
            m_brightnessValueLabel->setText("-");
        }
    }

    bool yuyvSelected = false;
    const int formatIndex = m_formatCombo->currentData().toInt();
    if (formatIndex >= 0 && formatIndex < m_formats.size()) {
        yuyvSelected = (m_formats[formatIndex].fourcc == QStringLiteral("YUYV"));
    }

    const VideoDevice::ControlInfo testPattern = yuyvSelected
            ? VideoDevice::testPatternInfo(devicePath)
            : VideoDevice::ControlInfo();
    const bool showPattern = testPattern.available && yuyvSelected;
    {
        QSignalBlocker blocker(m_testPatternCombo);
        m_testPatternCombo->clear();
        if (showPattern) {
            const QStringList items = VideoDevice::testPatternMenu(devicePath);
            for (int i = testPattern.minimum; i <= testPattern.maximum; ++i) {
                const int itemIndex = i - testPattern.minimum;
                const QString label = itemIndex >= 0 && itemIndex < items.size()
                        ? items[itemIndex]
                        : QString("Pattern %1").arg(i);
                m_testPatternCombo->addItem(label, i);
            }

            const int currentIndex = testPattern.value - testPattern.minimum;
            if (currentIndex >= 0 && currentIndex < m_testPatternCombo->count()) {
                m_testPatternCombo->setCurrentIndex(currentIndex);
            }
        }
    }
    m_testPatternLabel->setVisible(showPattern);
    m_testPatternCombo->setVisible(showPattern);
    m_testPatternCombo->setEnabled(showPattern);

}

QString VideoPage::selectedDevicePath() const
{
    if (!m_deviceCombo || m_deviceCombo->currentIndex() < 0) {
        return QString();
    }

    return m_deviceCombo->currentData().toString().trimmed();
}

quint32 VideoPage::selectedPixelFormat() const
{
    const int formatIndex = m_formatCombo->currentData().toInt();
    if (formatIndex < 0 || formatIndex >= m_formats.size()) {
        return 0;
    }

    return m_formats[formatIndex].pixelFormat;
}

QSize VideoPage::selectedFrameSize() const
{
    return m_sizeCombo->currentData().toSize();
}

void VideoPage::setVideoSettingsEnabled(bool enabled)
{
    const bool hasDevice = !selectedDevicePath().isEmpty();
    m_deviceCombo->setEnabled(enabled && hasDevice);
    m_scanDevicesButton->setEnabled(enabled);
    m_formatCombo->setEnabled(enabled && m_formatCombo->count() > 0);
    m_sizeCombo->setEnabled(enabled && m_sizeCombo->count() > 0);
}

void VideoPage::writeRecordingFrame(const QImage &image)
{
    if (!m_recording || image.isNull()) {
        return;
    }

    if (m_recordFrameTimer.isValid() && m_recordFrameTimer.elapsed() < 33) {
        return;
    }

    QString error;
    if (!m_recordWriter.addFrame(image, &error)) {
        appendLog("[ERROR] " + error);
        finishRecording(true);
        return;
    }

    m_recordFrameTimer.restart();
}

void VideoPage::finishRecording(bool deleteEmptyFile)
{
    if (!m_recording && !m_recordWriter.isOpen()) {
        return;
    }

    const QString filePath = m_recordingPath;
    const int frames = m_recordWriter.frameCount();
    QString error;
    const bool closeOk = m_recordWriter.close(&error);

    m_recording = false;
    m_recordingPath.clear();
    m_recordFrameTimer.invalidate();
    m_stopRecordButton->setEnabled(false);
    m_startRecordButton->setEnabled(m_videoWorker && !m_lastFrame.isNull());

    if (!closeOk) {
        appendLog("[ERROR] " + error);
        return;
    }

    if (frames <= 0 && deleteEmptyFile) {
        QFile::remove(filePath);
        appendLog("Recording discarded: no frames");
        return;
    }

    appendLog(QString("Recording saved: %1 (%2 frames)").arg(filePath).arg(frames));
    emit mediaSaved(filePath);
}

void VideoPage::cleanupWorker()
{
    finishRecording(false);

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
    m_startRecordButton->setEnabled(false);
    m_stopRecordButton->setEnabled(false);
    setVideoSettingsEnabled(true);
}

void VideoPage::appendLog(const QString &message)
{
    const QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_logEdit->append(QString("[%1] %2").arg(time, message));
}
