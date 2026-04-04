#ifndef CONTROLPAGE_H
#define CONTROLPAGE_H

#include <QWidget>
#include <QString>

#include "device/leddevice.h"
#include "device/beepdevice.h"

namespace Ui {
class ControlPage;
}

class ControlPage : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPage(QWidget *parent = nullptr);
    ~ControlPage();

    void setDevices(LedDevice *led, BeepDevice *beep);
    void setLedAvailable(bool ok);
    void setBeepAvailable(bool ok);

signals:
    void statusMessage(const QString &msg,int timeout);

private slots:
    void on_LedOnButton_clicked();

    void on_LedOffButton_clicked();

    void on_BeepOnButton_clicked();

    void on_BeepOffButton_clicked();

private:
    Ui::ControlPage *ui;
    LedDevice *m_led;
    BeepDevice *m_beep;
};

#endif // CONTROLPAGE_H
