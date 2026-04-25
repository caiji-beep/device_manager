QT       += core gui widgets serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    controller/serialcontroller.cpp \
    device/ap3216cdevice.cpp \
    device/beepdevice.cpp \
    device/icm20608device.cpp \
    device/leddevice.cpp \
    device/outputdevice.cpp \
    device/serialdevice.cpp \
    device/videodevice.cpp \
    main.cpp \
    mainwindow.cpp \
    pages/controlpage.cpp \
    pages/sensorpage.cpp \
    pages/serialpage.cpp \
    pages/videopage.cpp \
    workers/ap3216cworker.cpp \
    workers/icm20608worker.cpp \
    workers/videoworker.cpp

HEADERS += \
    controller/serialcontroller.h \
    device/ap3216cdevice.h \
    device/beepdevice.h \
    device/icm20608device.h \
    device/leddevice.h \
    device/outputdevice.h \
    device/serialdevice.h \
    device/videodevice.h \
    mainwindow.h \
    pages/controlpage.h \
    pages/sensorpage.h \
    pages/serialpage.h \
    pages/videopage.h \
    workers/ap3216cworker.h \
    workers/icm20608worker.h \
    workers/videoworker.h

FORMS += \
    mainwindow.ui \
    pages/controlpage.ui \
    pages/sensorpage.ui \
    pages/serialpage.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
