#include "serialdevice.h"



SerialDevice::SerialDevice(QObject *parent)
    : QObject{parent}
    , m_serial(this)
{
    connect(&m_serial,&QSerialPort::readyRead,this,&SerialDevice::onReadyRead);
    connect(&m_serial,&QSerialPort::errorOccurred,this,&SerialDevice::onErrorOccurred);
}

SerialDevice::~SerialDevice()
{
    SerialDevice::close();
}

bool SerialDevice::open(const QString &portName,int baudRate)
{
    /*如果串口当前处于打开状态，就先把它关掉*/
    if(m_serial.isOpen())
    {
        m_serial.close();
    }
    m_serial.setPortName(portName);
    m_serial.setBaudRate(baudRate);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    if(!m_serial.open(QIODevice::ReadWrite)){
        m_lastError = m_serial.errorString();
        emit errorMessage(QString("open %1 fail: %2").arg(portName,m_lastError));
        return false;
    }

    m_rxBuffer.clear();
    m_lastError.clear();
    emit logMessage(QString("serial open ok: %1,%2 8N1").arg(portName).arg(baudRate));
    return true;
}

void SerialDevice::close()
{
    if (m_serial.isOpen()) {
        m_serial.close();
        emit logMessage("serial closed");
    }
}

bool SerialDevice::isReady() const
{
    return m_serial.isOpen();
}

QString SerialDevice::lastError() const
{
    return m_lastError;
}

bool SerialDevice::sendLine(const QString &text)
{
    if (!m_serial.isOpen()) {
        m_lastError = "serial not ready";
        emit errorMessage(m_lastError);
        return false;
    }

    QByteArray data = text.toUtf8();
    if (!data.endsWith('\n')) {
        data += "\r\n";
    }

    qint64 ret = m_serial.write(data);
    if (ret != data.size()) {
        m_lastError = m_serial.errorString();
        emit errorMessage(QString("serial write failed: %1").arg(m_lastError));
        return false;
    }

    emit logMessage(QString("TX: %1").arg(QString::fromUtf8(data).trimmed()));
    return true;
}

void SerialDevice::onReadyRead()
{
    m_rxBuffer += m_serial.readAll();

    while (true) {
        int pos = m_rxBuffer.indexOf('\n');
        if (pos < 0) {
            break;
        }

        QByteArray oneLine = m_rxBuffer.left(pos + 1);
        m_rxBuffer.remove(0, pos + 1);

        QString line = QString::fromUtf8(oneLine).trimmed();
        if (line.isEmpty()) {
            continue;
        }

        emit logMessage(QString("RX: %1").arg(line));
        emit lineReceived(line);
    }
}

void SerialDevice::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    m_lastError = m_serial.errorString();
    emit errorMessage(QString("serial error: %1").arg(m_lastError));
}




