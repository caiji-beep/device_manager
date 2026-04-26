#ifndef VIDEOPAGE_H
#define VIDEOPAGE_H

#include <QElapsedTimer>
#include <QImage>
#include <QWidget>

class QLabel;
class QComboBox;
class QPushButton;
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


private slots:
    void onScanDevicesClicked();
    void onStartMonitorClicked();
    void onStopMonitorClicked();
    void onFrameReady(const QImage &image);
    void onCaptureClicked();
    void onStartRecordClicked();
    void onStopRecordClicked();

private:
    void initUi();
    void initConnections();
    void refreshVideoDevices();
    QString selectedDevicePath() const;
    void cleanupWorker();
    void appendLog(const QString &message);

private:
    QLabel *m_previewLabel;
    QComboBox *m_deviceCombo;
    QPushButton *m_scanDevicesButton;
    QPushButton *m_startMonitorButton;
    QPushButton *m_stopMonitorButton;
    QPushButton *m_captureButton;
    QPushButton *m_startRecordButton;
    QPushButton *m_stopRecordButton;
    QTextEdit *m_logEdit;
    QThread *m_videoThread;
    VideoWorker *m_videoWorker;
    QImage m_lastFrame;
    QElapsedTimer m_frameUpdateTimer;
};

#endif // VIDEOPAGE_H
