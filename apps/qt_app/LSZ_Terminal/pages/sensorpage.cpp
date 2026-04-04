#include "sensorpage.h"
#include "ui_sensorpage.h"

#include <QMessageBox>

SensorPage::SensorPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SensorPage)
    , m_ap3216c(nullptr)
    , m_icm20608(nullptr)
{
    ui->setupUi(this);
}

SensorPage::~SensorPage()
{
    delete ui;
}

void SensorPage::setDevices(Ap3216cDevice *ap3216c, Icm20608Device *icm20608)
{
    m_ap3216c = ap3216c;
    m_icm20608 = icm20608;
}

void SensorPage::setAp3216cAvailable(bool ok)
{
    ui->Ap3216cReadButton->setEnabled(ok);
}

void SensorPage::setIcm20608Available(bool ok)
{
    ui->Icm20608ReadButton->setEnabled(ok);
}

void SensorPage::on_Ap3216cReadButton_clicked()
{
    if (!m_ap3216c || !m_ap3216c->isReady()) {
        QMessageBox::warning(this, "Prompt", "AP3216C device not ready");
        emit statusMessage("AP3216C device not ready", 3000);
        return;
    }

    Ap3216cData data;
    if (!m_ap3216c->readData(data)) {
        QMessageBox::warning(this, "Prompt",
                             "Read AP3216C failed:\n" + m_ap3216c->lastError());
        emit statusMessage("Read AP3216C failed", 3000);
        return;
    }

    ui->Ap3216cIrLabel->setText(QString("IR: %1").arg(data.ir));
    ui->Ap3216cAlsLabel->setText(QString("ALS: %1").arg(data.als));
    ui->Ap3216cPsLabel->setText(QString("PS: %1").arg(data.ps));

    emit statusMessage("AP3216C read success", 1500);
}

void SensorPage::on_Icm20608ReadButton_clicked()
{
    if (!m_icm20608 || !m_icm20608->isReady()) {
        QMessageBox::warning(this, "Prompt", "ICM20608 device not ready");
        emit statusMessage("ICM20608 device not ready", 3000);
        return;
    }

    Icm20608Data data;
    if (!m_icm20608->readData(data)) {
        QMessageBox::warning(this, "Prompt",
                             "Read ICM20608 failed:\n" + m_icm20608->lastError());
        emit statusMessage("Read ICM20608 failed", 3000);
        return;
    }

    ui->IcmAccelXLabel->setText(QString::asprintf("Accel X: %.2f g", data.accelX));
    ui->IcmAccelYLabel->setText(QString::asprintf("Accel Y: %.2f g", data.accelY));
    ui->IcmAccelZLabel->setText(QString::asprintf("Accel Z: %.2f g", data.accelZ));

    ui->IcmGyroXLabel->setText(QString::asprintf("Gyro X: %.2f °/s", data.gyroX));
    ui->IcmGyroYLabel->setText(QString::asprintf("Gyro Y: %.2f °/s", data.gyroY));
    ui->IcmGyroZLabel->setText(QString::asprintf("Gyro Z: %.2f °/s", data.gyroZ));

    ui->IcmTempLabel->setText(QString::asprintf("Temp: %.2f °C", data.temp));

    emit statusMessage("ICM20608 read success", 1500);
}
