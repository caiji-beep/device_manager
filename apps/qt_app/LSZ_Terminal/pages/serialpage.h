#ifndef SERIALPAGE_H
#define SERIALPAGE_H

#include <QWidget>
#include "device/serialdevice.h"

namespace Ui {
class SerialPage;
}

class SerialPage : public QWidget
{
    Q_OBJECT

public:
    explicit SerialPage(QWidget *parent = nullptr);
    ~SerialPage();

    void setSerialDevice(SerialDevice *serial);
    void setSerialReady(bool ok);

public slots:
    void appendLogMessage(const QString &msg);
    void appendErrorMessage(const QString &msg);

signals:
    void statusMessage(const QString &msg, int timeout);

private slots:
    void on_SerialSendButton_clicked();
    void on_SerialClearLogButton_clicked();

private:
    Ui::SerialPage *ui;
    SerialDevice *m_serial;
};

#endif // SERIALPAGE_H
