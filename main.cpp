#include <QApplication>
#include "ipconfig.h"
#include "getcamerainfo.h"
#include "tcpserver.h"
#include "udpgroupclient.h"
#include "receivefileserver.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //根据拨码开关设置ip地址
    IPConfig ipconfig;

    //获取2个摄像头设备文件
    GetCameraInfo getcamerainfo;

    //tcp初始化
    TcpServer tcpserver(getcamerainfo.MainCamera,getcamerainfo.SubCamera);

    //组播监听
    UdpGroupClient udpgroupclient;

    //升级
    ReceiveFileServer receive;
    receive.startServer(6904);

    return a.exec();
}
