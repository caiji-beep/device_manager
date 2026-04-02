QT       += core gui widgets serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    controller/serialcontroller.cpp \
    device/beepdevice.cpp \
    device/leddevice.cpp \
    device/outputdevice.cpp \
    device/serialdevice.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    controller/serialcontroller.h \
    device/beepdevice.h \
    device/leddevice.h \
    device/outputdevice.h \
    device/serialdevice.h \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
