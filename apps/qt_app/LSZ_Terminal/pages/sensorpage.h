#ifndef SENSORPAGE_H
#define SENSORPAGE_H

#include <QWidget>
#include "device/ap3216cdevice.h"
#include "device/icm20608device.h"

namespace Ui {
class SensorPage;
}

class SensorPage : public QWidget
{
    Q_OBJECT

public:
    explicit SensorPage(QWidget *parent = nullptr);
    ~SensorPage();

    void setDevices(Ap3216cDevice *ap3216c, Icm20608Device *icm20608);
    void setAp3216cAvailable(bool ok);
    void setIcm20608Available(bool ok);

signals:
    void statusMessage(const QString &msg, int timeout);

private slots:
    void on_Ap3216cReadButton_clicked();
    void on_Icm20608ReadButton_clicked();

private:
    Ui::SensorPage *ui;
    Ap3216cDevice *m_ap3216c;
    Icm20608Device *m_icm20608;
};

#endif // SENSORPAGE_H
