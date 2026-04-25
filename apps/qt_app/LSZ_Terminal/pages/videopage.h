#ifndef VIDEOPAGE_H
#define VIDEOPAGE_H

#include <QImage>
#include <QWidget>

class QLabel;
class QLineEdit;
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


private slots:
    void onStartMonitorClicked();
    void onStopMonitorClicked();
    void onFrameReady(const QImage &image);
    void onCaptureClicked();
    void onStartRecordClicked();
    void onStopRecordClicked();

private:
    void initUi();
    void initConnections();
    void cleanupWorker();
    void appendLog(const QString &message);

private:
    QLabel *m_previewLabel;
    QLineEdit *m_devicePathEdit;
    QPushButton *m_startMonitorButton;
    QPushButton *m_stopMonitorButton;
    QPushButton *m_captureButton;
    QPushButton *m_startRecordButton;
    QPushButton *m_stopRecordButton;
    QTextEdit *m_logEdit;
    QThread *m_videoThread;
    VideoWorker *m_videoWorker;
    QImage m_lastFrame;
};

#endif // VIDEOPAGE_H
