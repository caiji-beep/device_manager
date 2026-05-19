#ifndef VIDEOPAGE_H
#define VIDEOPAGE_H

#include <QElapsedTimer>
#include <QImage>
#include <QWidget>
#include <QVector>

#include "device/videodevice.h"
#include "media/aviwriter.h"

class QLabel;
class QComboBox;
class QPushButton;
class QSlider;
class QTextEdit;
class QThread;
class VideoWorker;

class VideoPage : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPage(QWidget *parent = nullptr);
    ~VideoPage() override;
    void stopMonitor(bool waitForFinished = false);

signals:
    void mediaSaved(const QString &path);

private slots:
    void onScanDevicesClicked();
    void onDeviceSelectionChanged();
    void onFormatSelectionChanged();
    void onStartMonitorClicked();
    void onStopMonitorClicked();
    void onFrameReady(const QImage &image);
    void onCaptureClicked();
    void onStartRecordClicked();
    void onStopRecordClicked();
    void onBrightnessChanged(int value);
    void onTestPatternChanged(int index);

private:
    void initUi();
    void initConnections();
    void refreshVideoDevices();
    void refreshVideoOptions();
    void refreshControlOptions();
    QString selectedDevicePath() const;
    quint32 selectedPixelFormat() const;
    QSize selectedFrameSize() const;
    void setVideoSettingsEnabled(bool enabled);
    void writeRecordingFrame(const QImage &image);
    void finishRecording(bool deleteEmptyFile);
    void cleanupWorker();
    void appendLog(const QString &message);

private:
    QLabel *m_previewLabel;
    QComboBox *m_deviceCombo;
    QComboBox *m_formatCombo;
    QComboBox *m_sizeCombo;
    QPushButton *m_scanDevicesButton;
    QPushButton *m_startMonitorButton;
    QPushButton *m_stopMonitorButton;
    QPushButton *m_captureButton;
    QPushButton *m_startRecordButton;
    QPushButton *m_stopRecordButton;
    QSlider *m_brightnessSlider;
    QLabel *m_brightnessValueLabel;
    QLabel *m_testPatternLabel;
    QComboBox *m_testPatternCombo;
    QTextEdit *m_logEdit;
    QThread *m_videoThread;
    VideoWorker *m_videoWorker;
    QImage m_lastFrame;
    QElapsedTimer m_frameUpdateTimer;
    QElapsedTimer m_recordFrameTimer;
    QVector<VideoDevice::FormatInfo> m_formats;
    AviWriter m_recordWriter;
    QString m_recordingPath;
    bool m_recording;
};

#endif // VIDEOPAGE_H
