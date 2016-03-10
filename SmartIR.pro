#-------------------------------------------------
#
# Project created by QtCreator 2015-10-10T10:58:27
#
#-------------------------------------------------

QT       += core gui network xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = SmartIR
TEMPLATE = app


SOURCES += main.cpp\
    ipconfig.cpp \
    getcamerainfo.cpp \
    operatecamera.cpp \
    tcpserver.cpp \
    udpgroupclient.cpp \
    workthread.cpp \
    globalconfig.cpp \
    devicecontrol.cpp \
    QextSerialPort/qextserialport.cpp \
    QextSerialPort/qextserialport_unix.cpp \
    QextSerialPort/operateserial.cpp \
    receivefileserver.cpp \
    receivefilethread.cpp \
    jpegencode.cpp

HEADERS  += ipconfig.h \
    getcamerainfo.h \
    operatecamera.h \
    tcpserver.h \
    udpgroupclient.h \
    CommonSetting.h \
    workthread.h \
    globalconfig.h \
    devicecontrol.h \
    QextSerialPort/qextserialport.h \
    QextSerialPort/qextserialport_global.h \
    QextSerialPort/qextserialport_p.h \
    QextSerialPort/operateserial.h \
    receivefileserver.h \
    receivefilethread.h \
    jpegencode.h

unix{
INCLUDEPATH += opencv/linux/arm/include
LIBS += -Lopencv/linux/arm/lib -lopencv_core \
        -lopencv_highgui \
        -lopencv_imgproc
}

unix{
INCLUDEPATH += $$PWD/ffmpeg/include
LIBS += -Lffmpeg/lib/arm/ -lavcodec \
        -Lffmpeg/lib/arm/ -lavfilter\
        -Lffmpeg/lib/arm/ -lavformat\
        -Lffmpeg/lib/arm/ -lswscale\
        -Lffmpeg/lib/arm/ -lavutil \
        -Lffmpeg/lib/arm/ -lswresample \
        -Lffmpeg/lib/arm/ -lpostproc
}

DESTDIR=bin
MOC_DIR=temp/moc
RCC_DIR=temp/rcc
UI_DIR=temp/ui
OBJECTS_DIR=temp/obj
