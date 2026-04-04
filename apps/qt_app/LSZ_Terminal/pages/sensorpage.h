#ifndef SENSORPAGE_H
#define SENSORPAGE_H

#include <QWidget>
#include <QThread>
#include "device/ap3216cdevice.h"
#include "device/icm20608device.h"
#include "workers/ap3216cworker.h"
#include "workers/icm20608worker.h"

namespace Ui {
class SensorPage;
}

class SensorPage : public QWidget
{
    Q_OBJECT

public:
    explicit SensorPage(QWidget *parent = nullptr);
    ~SensorPage();

signals:
    void statusMessage(const QString &msg, int timeout);

    void startApWorker(int intervalMs);
    void stopApWorker();

    void startIcmWorker(int intervalMs);
    void stopIcmWorker();

private slots:


    void on_Ap3216cStartButton_clicked();

    void on_Ap3216cStopButton_clicked();

    void on_Icm20608StartButton_clicked();

    void on_Icm20608StopButton_clicked();

    void onApDataReady(const Ap3216cData &data);
    void onIcmDataReady(const Icm20608Data &data);

    void onApError(const QString &msg);
    void onIcmError(const QString &msg);

private:
    void setupWorkers();
    void stopWorkers();

private:
    Ui::SensorPage *ui;

    QThread *m_apThread;
    QThread *m_icmThread;

    Ap3216cWorker *m_apWorker;
    Icm20608Worker *m_icmWorker;

};

#endif // SENSORPAGE_H
