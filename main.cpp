#include <QApplication>
#include "ipconfig.h"
#include "getcamerainfo.h"
#include "tcpserver.h"
#include "udpgroupclient.h"
#include "receivefileserver.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //初始化
    GlobalConfig::init();

    //根据拨码开关设置ip地址
    IPConfig::Instance()->Update();

    //获取2个摄像头设备文件
    GetCameraInfo::Instance();

    //tcp初始化
    TcpServer tcpserver(GetCameraInfo::MainCamera,GetCameraInfo::SubCamera);

    //组播监听
    UdpGroupClient udpgroupclient;

    //升级
    ReceiveFileServer receive;
    receive.startServer(GlobalConfig::UpgradePort);

    return a.exec();
}
