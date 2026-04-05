#include "sensorpage.h"
#include "ui_sensorpage.h"

#include <QMessageBox>
#include <QMetaType>

SensorPage::SensorPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SensorPage)
    , m_apThread(nullptr)
    , m_icmThread(nullptr)
    , m_apWorker(nullptr)
    , m_icmWorker(nullptr)
{
    ui->setupUi(this);

    qRegisterMetaType<Ap3216cData>("Ap3216cData");
    qRegisterMetaType<Icm20608Data>("Icm20608Data");

    setupWorkers();

    ui->Ap3216cStopButton->setEnabled(false);
    ui->Icm20608StopButton->setEnabled(false);
}

SensorPage::~SensorPage()
{
    stopWorkers();
    delete ui;
}


void SensorPage::setupWorkers()
{
    // AP3216C
    m_apThread = new QThread(this);
    m_apWorker = new Ap3216cWorker("/dev/ap3216c");
    m_apWorker->moveToThread(m_apThread);

    connect(m_apThread, &QThread::finished,
            m_apWorker, &QObject::deleteLater);

    connect(this, &SensorPage::startApWorker,
            m_apWorker, &Ap3216cWorker::startWork);

    connect(this, &SensorPage::stopApWorker,
            m_apWorker, &Ap3216cWorker::stopWork);

    connect(m_apWorker, &Ap3216cWorker::dataReady,
            this, &SensorPage::onApDataReady);

    connect(m_apWorker, &Ap3216cWorker::errorOccurred,
            this, &SensorPage::onApError);

    connect(m_apWorker, &Ap3216cWorker::started,
            this, [this]() {
        ui->Ap3216cStartButton->setEnabled(false);
        ui->Ap3216cStopButton->setEnabled(true);
        emit statusMessage("AP3216C worker started", 2000);
    });

    connect(m_apWorker, &Ap3216cWorker::stopped,
            this, [this]() {
        ui->Ap3216cStartButton->setEnabled(true);
        ui->Ap3216cStopButton->setEnabled(false);
        emit statusMessage("AP3216C worker stopped", 2000);
    });

    m_apThread->start();

    // ICM20608
    m_icmThread = new QThread(this);
    m_icmWorker = new Icm20608Worker("/dev/icm20608");
    m_icmWorker->moveToThread(m_icmThread);

    connect(m_icmThread, &QThread::finished,
            m_icmWorker, &QObject::deleteLater);

    connect(this, &SensorPage::startIcmWorker,
            m_icmWorker, &Icm20608Worker::startWork);

    connect(this, &SensorPage::stopIcmWorker,
            m_icmWorker, &Icm20608Worker::stopWork);

    connect(m_icmWorker, &Icm20608Worker::dataReady,
            this, &SensorPage::onIcmDataReady);

    connect(m_icmWorker, &Icm20608Worker::errorOccurred,
            this, &SensorPage::onIcmError);

    connect(m_icmWorker, &Icm20608Worker::started,
            this, [this]() {
        ui->Icm20608StartButton->setEnabled(false);
        ui->Icm20608StopButton->setEnabled(true);
        emit statusMessage("ICM20608 worker started", 2000);
    });

    connect(m_icmWorker, &Icm20608Worker::stopped,
            this, [this]() {
        ui->Icm20608StartButton->setEnabled(true);
        ui->Icm20608StopButton->setEnabled(false);
        emit statusMessage("ICM20608 worker stopped", 2000);
    });

    m_icmThread->start();
}


void SensorPage::stopWorkers()
{
    if (m_apWorker) {
        QMetaObject::invokeMethod(
            m_apWorker, "stopWork", Qt::BlockingQueuedConnection);
    }
    if (m_apThread) {
        m_apThread->quit();
        m_apThread->wait();
    }
    if (m_icmWorker) {
        QMetaObject::invokeMethod(
            m_icmWorker, "stopWork", Qt::BlockingQueuedConnection);
    }
    if (m_icmThread) {
        m_icmThread->quit();
        m_icmThread->wait();
    }
}



void SensorPage::on_Ap3216cStartButton_clicked()
{
    int intervalMs = ui->Ap3216cIntervalBox->currentText().toInt();
    emit startApWorker(intervalMs);
}

void SensorPage::on_Ap3216cStopButton_clicked()
{
    emit stopApWorker();
}

void SensorPage::on_Icm20608StartButton_clicked()
{
    int intervalMs = ui->Icm20608IntervalBox->currentText().toInt();
    emit startIcmWorker(intervalMs);
}

void SensorPage::on_Icm20608StopButton_clicked()
{
    emit stopIcmWorker();
}

void SensorPage::onApDataReady(const Ap3216cData &data)
{
    ui->Ap3216cIrLabel->setText(QString("IR: %1").arg(data.ir));
    ui->Ap3216cAlsLabel->setText(QString("ALS: %1").arg(data.als));
    ui->Ap3216cPsLabel->setText(QString("PS: %1").arg(data.ps));
}

void SensorPage::onIcmDataReady(const Icm20608Data &data)
{
    ui->IcmAccelXLabel->setText(QString::asprintf("Accel X: %.2f g", data.accelX));
    ui->IcmAccelYLabel->setText(QString::asprintf("Accel Y: %.2f g", data.accelY));
    ui->IcmAccelZLabel->setText(QString::asprintf("Accel Z: %.2f g", data.accelZ));

    ui->IcmGyroXLabel->setText(QString::asprintf("Gyro X: %.2f °/s", data.gyroX));
    ui->IcmGyroYLabel->setText(QString::asprintf("Gyro Y: %.2f °/s", data.gyroY));
    ui->IcmGyroZLabel->setText(QString::asprintf("Gyro Z: %.2f °/s", data.gyroZ));

    ui->IcmTempLabel->setText(QString::asprintf("Temp: %.2f °C", data.temp));
}

void SensorPage::onApError(const QString &msg)
{
    emit statusMessage(msg, 3000);
    QMessageBox::warning(this, "AP3216C Error", msg);
}

void SensorPage::onIcmError(const QString &msg)
{
    emit statusMessage(msg, 3000);
    QMessageBox::warning(this, "ICM20608 Error", msg);
}


