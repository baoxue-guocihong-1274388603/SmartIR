#include "operateserial.h"
#include <QDebug>
#include <QTime>

OperateSerial::OperateSerial(QObject *parent) :
    QObject(parent)
{
    mySerial = new QextSerialPort("/dev/ttySAC1");
    if(mySerial->open(QIODevice::ReadWrite)){
        mySerial->setBaudRate(BAUD9600);
        mySerial->setDataBits(DATA_8);
        mySerial->setParity(PAR_NONE);
        mySerial->setStopBits(STOP_1);
        mySerial->setFlowControl(FLOW_OFF);
        mySerial->setTimeout(10);
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "/dev/ttySAC1 Open Succeed!";
    }

    ReadSerialTimer = new QTimer(this);
    ReadSerialTimer->setInterval(5000);
    connect(ReadSerialTimer,SIGNAL(timeout()),this,SLOT(slotReadSerialMsg()));
//    ReadSerialTimer->start();
}

//id用来区分主控制杆和辅助控制杆0x00主控制杆   0x01辅助控制杆
void OperateSerial::SetPWM(quint8 value,quint8 id)
{
    quint8 head = 0x16;
    quint8 destAddress = 0xFF;
    quint8 hostAddress = 0x01;
    quint8 cmdLength = 0x02;
    quint8 cmdID = 0xE2;
//    quint8 cmd = id;
    quint8 param_1 = value;

    quint8 parity = (destAddress + hostAddress + cmdLength + cmdID + param_1) % 256;
    QByteArray ba;
    ba[0] = head;
    ba[1] = destAddress;
    ba[2] = hostAddress;
    ba[3] = cmdLength;
    ba[4] = cmdID;
//    ba[5] = cmd;
    ba[5] = param_1;
    ba[6] = parity;
    mySerial->write(ba);
}

void OperateSerial::slotReadSerialMsg()
{
    QTime time;
    time= QTime::currentTime();
    qsrand(time.msec() + time.second() * 1000);

    int id = qrand() % 5;
    emit signalAlarmMsg(id);
}
