#ifndef SERIALDEVICE_H
#define SERIALDEVICE_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QSerialPort>

class SerialDevice : public QObject
{
    Q_OBJECT
public:
    explicit SerialDevice(QObject *parent = nullptr);
    ~SerialDevice();

    bool open(const QString &portName,int baudRate = 115200);
    void close();
    bool isReady() const;
    QString lastError() const;

    bool sendLine(const QString &text); //自动补\r\n

signals:
    void lineReceived(const QString &line);
    void logMessage(const QString &msg);
    void errorMessage(const QString &line);

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    QSerialPort m_serial;
    QByteArray m_rxBuffer;
    QString m_lastError;
};

#endif // SERIALDEVICE_H
