#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "getcamerainfo.h"
#include "devicecontrol.h"
#include "globalconfig.h"
#include "QextSerialPort/operateserial.h"
#include "workthread.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/tcp.h>

class TcpServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpServer(QString MainVideoName,QString SubVideoName,QObject *parent = 0);
    void enableKeepAlive(int fd,int max_idle,int keep_count,int keep_interval);

signals:

public slots:
    void slotProcessAlarmHostConfigConnection();
    void slotRecvAlarmHostConfigMsg();
    void slotAlarmHostConfigDisconnect();

    void slotProcessMainControlConnection();
    void slotRecvMainControlMsg();
    void slotMainControlDisconnect();

    void slotMainCaputreNormalFrame(QImage &img);

    void slotMainCaputreAlarmFrame(QImage &img);
    void slotSubCaputreAlarmFrame(QImage &img);
    void slotOtherAlarmMsg(quint8 id);

    void slotAlarmHostAlarmConnected();
    void slotAlarmHostAlarmDisConnected();
    void slotAlarmHostAlarmDisplayError(QAbstractSocket::SocketError socketError);

    void slotRecvSubControlMsg();
    void slotSubControlConnected();
    void slotSubControlDisConnected();
    void slotSubControlDisplayError(QAbstractSocket::SocketError socketError);

    void slotProcessAlarmHostConfigMsg();
    void slotSendAlarmMsg();

    void slotUSBCameraOffline();

    void slotSetSystemTime();

    void slotReConnectSubControl();
    void slotCheckWorkThreadState();

private:
    QTcpServer *AlarmHostServer; //监听报警主机
    QTcpServer *MainControlSever;//监听主控制杆

    QTcpSocket *AlarmHostAlarmSocket;//用来主动上报报警信息-->使用tcp短连接
    QTcpSocket *AlarmHostConfigSocket;//用来与报警主机通信(配置)-->使用tcp长连接
    QTcpSocket *MainControlSocket;//用来与左边主控制杆通信-->使用tcp长连接
    QTcpSocket *SubControlSocket;//用来与右边的辅助控制杆通信-->使用tcp长连接

    QByteArray AlarmHostConfigBuffer;//用于存放报警主机发送所有的消息
    QByteArray MainControlBuffer;//用于存放左边主控制杆发送所有的消息
    QByteArray SubControlBuffer;//用于存放右边辅助控制杆发送所有的消息

    WorkThread *MainWorkThread;//主控制杆采集线程
    WorkThread *SubWorkThread;//辅助控制杆采集线程

    QImage MainNormalImage;//主控制杆正常图片
    QImage SubNormalImage;//辅助控制杆正常图片

    enum ConnectState{
        ConnectedState,
        DisConnectedState
    };
    volatile enum ConnectState AlarmHostAlarmConnectStateFlag;//主控制杆主动上传报警主机tcp连接状态
    volatile enum ConnectState SubControlConnectStateFlag;//主控制杆与辅助控制杆通信tcp连接状态


    QTimer *ProcessAlarmHostConfigMsgTimer;//用来处理报警主机发送的配置信息

    QString SystemTime;
    QTimer *SetSystemTimeTimer;

    QTimer *ReConnectSubControlTimer;//用来重连右边的辅助控制杆

    QTimer *SendAlarmMsgTimer;//用来发送所有报警信息，包括主控制杆和辅助控制杆以及其他报警信息
    QList<QString> AlarmMsgBuffer;//用来存放所有报警信息

    OperateSerial *operate_serial;

    QTimer *CheckWorkThreadStateTimer;//主要用来检测工作线程的状态

    QString MainStreamVideoName;
    QString SubStreamVideoName;

    qint64 PreMainThreadState;
    qint64 PreSubThreadState;
};

#endif // TCPSERVER_H
