#ifndef OPERATESERIAL_H
#define OPERATESERIAL_H

#include <QObject>
#include <QTimer>
#include "QextSerialPort/qextserialport.h"

class OperateSerial : public QObject
{
    Q_OBJECT
public:
    explicit OperateSerial(QObject *parent = 0);
    void SetPWM(quint8 value, quint8 id);

public slots:
    void slotReadSerialMsg();

signals:
    void signalAlarmMsg(quint8 id);

private:
    QextSerialPort *mySerial;
    QTimer *ReadSerialTimer;
};

#endif // OPERATESERIAL_H
