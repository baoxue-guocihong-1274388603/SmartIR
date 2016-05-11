#ifndef GLOBALCONFIG_H
#define GLOBALCONFIG_H

#include "CommonSetting.h"

class GlobalConfig
{
public:
    static QString ServerIP;                  //告警上报服务器IP
    static QString ServerPort;                //告警上报服务器端口
    static QString MainIP;                    //主控制杆IP，主要用于辅助控制杆报警时填充本IP
    static QString SubIP;               //辅助控制杆IP，用于向辅助控制杆发送配置信息和获取辅助控制杆采集的图片
    static QString MainDefenceID;             //主控制杆报警时填充本防区号
    static QString SubDefenceID;              //辅助控制杆报警时填充本防区号
    static int CameraSleepTime;               //采集间隔（单位毫秒）
    static QString DeviceMacAddr;             //设备MAC地址
    static QString DeviceIPAddrPrefix;        //设备网段
    static int TcpConnectTimeout;             //tcp连接超时

    static int MainStreamFactor;              //主控制杆放大倍数
    static QRect MainStreamSelectRect;        //主控制杆框选的矩形区域
    static QList<QPoint> MainStreamLightPoint;//主控制杆选择的亮点坐标

    static int SubStreamFactor;               //辅助控制杆放大倍数
    static QRect SubStreamSelectRect;         //辅助控制杆框选的矩形区域
    static QList<QPoint> SubStreamLightPoint; //辅助控制杆选择的亮点坐标

    static int AlarmHostServerPort;           //监听报警主机连接端口
    static int MainControlServerPort;         //监听主控制杆连接端口
    static int UpgradePort;                   //升级端口

    static QString GroupAddr;
    static int GroupPort;

    static QString LocalHostIP;               //本设备IP,主要用于判断报警主机发送的消息是否为发给本设备
    static QString Mask;
    static QString Gateway;
    static QString MAC;

    static QString AppPath;
    static QString AppName;
    static QString ConfigFileName;

    static void init();

    static QRect GetMainStreamSelectRect();
    static QRect GetSubStreamSelectRect();

    static QList<QPoint> GetMainStreamLightPoint();
    static QList<QPoint> GetSubStreamLightPoint();   

    static QString GenerateMAC();             //随机生成MAC地址
};

#endif // GLOBALCONFIG_H
