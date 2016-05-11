#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "getcamerainfo.h"
#include "devicecontrol.h"
#include "ipconfig.h"
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
    void SendAlarmHostConfigData(QString Msg, QTcpSocket *AlarmHostConfigSocket);//发送数据包给报警主机
    void WriteData(QString Msg);//发送数据包给辅助控制杆
    void SendMainCameraStateInfo(QString info);
    void SendSubCameraStateInfo(QString info);

public slots:
    void slotProcessAlarmHostConfigConnection();//处理报警主机发送配置信息的连接
    void slotRecvAlarmHostConfigMsg();
    void slotAlarmHostConfigDisconnect();

    void slotProcessMainControlConnection();//处理主控制杆的连接
    void slotRecvMainControlMsg();
    void slotMainControlDisconnect();

    void slotAlarmHostAlarmDisConnected();//主动上报告警信息断开连接
    void slotAlarmHostAlarmDisplayError(QAbstractSocket::SocketError socketError);

    void slotSubControlConnected();//主控制杆连接辅助控制杆成功
    void slotRecvSubControlMsg();
    void slotSubControlDisConnected();
    void slotSubControlDisplayError(QAbstractSocket::SocketError socketError);

    void slotMainAlarmImage();//主控制杆报警图像
    void slotSubAlarmImage();//辅助控制杆报警图像
    void slotOtherAlarmMsg(quint8 id);//文字报警信息

    void slotProcessAlarmHostConfigMsg();//处理报警主机下发的配置信息和获取主码流图片
    void slotSendAlarmMsg();//发送主控制杆报警图像和辅助控制杆报警图像以及文字报警信息

    void slotMainUSBCameraOffline();//主控制杆摄像头掉线
    void slotMainUSBCameraOnline();

    void slotSubUSBCameraOffline();//辅助控制杆摄像头掉线
    void slotSubUSBCameraOnline();

    void slotSetSystemTime();

    void slotReConnectSubControl();//重连辅助控制杆
    void slotCheckWorkThreadState();//检测主工作线程和辅助工作线程的状态

private:
    QTcpServer *AlarmHostServer; //监听报警主机
    QTcpServer *MainControlSever;//监听主控制杆

    QTcpSocket *MainControlSocket; //用来与左边主控制杆通信-->使用tcp长连接
    QTcpSocket *SubControlSocket;  //用来与右边的辅助控制杆通信-->使用tcp长连接

    QByteArray MainControlBuffer;//用于存放左边主控制杆的所有消息
    QByteArray SubControlBuffer;//用于存放右边辅助控制杆的所有消息

    WorkThread *MainWorkThread;//主控制杆采集线程
    WorkThread *SubWorkThread;//辅助控制杆采集线程

    enum ConnectState{
        ConnectedState,
        DisConnectedState
    };
    volatile enum ConnectState SubControlConnectStateFlag;//主控制杆与辅助控制杆通信tcp连接状态

    QList<QString> AlarmHostConfigBuffer;//用于存放报警主机下发的所有消息--->连接对象(支持多个)
    QTimer *ProcessAlarmHostConfigMsgTimer;//用来处理报警主机下发的配置信息
    QList<QTcpSocket *> tcpClients;//每个报警主机的连接对象(支持多个)-->使用tcp长连接
    QList<QTcpSocket *> tcpClientsUnique;//每个报警主机的连接对象只保存一个实例

    QList<QString> AlarmMsgBuffer;//用来存放所有报警信息
    QTimer *SendAlarmMsgTimer;//用来发送所有报警信息，包括主控制杆和辅助控制杆以及其他报警信息
    QList<QTcpSocket *> tcpServers;//主动上报告警信息连接对象(支持多个)-->使用tcp短连接

    QString SystemTime;
    QTimer *SetSystemTimeTimer;

    QTimer *ReConnectSubControlTimer;//用来重连右边的辅助控制杆

    QTimer *CheckWorkThreadStateTimer;//主要用来检测工作线程的状态
    bool isMainThreadExceptionQuit;//主码流工作线程异常退出
    bool isSubThreadExceptionQuit;//辅码流工作线程异常退出

    OperateSerial *operate_serial;

    QString MainStreamVideoName;//主控制杆摄像头设备文件
    QString SubStreamVideoName;//辅助控制杆摄像头设备文件
};

#endif // TCPSERVER_H
