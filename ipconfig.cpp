#include "ipconfig.h"
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include "devicecontrol.h"

IPConfig::IPConfig(QObject *parent) :
    QObject(parent)
{
    //获取拨码地址
    uchar mask = DeviceControl::Instance()->GetSwitchAddr();

    QString OldIP;
    QFile file("/etc/eth0-setting");
    if(file.open(QFile::ReadOnly)){
        QTextStream in(&file);
        OldIP = in.readLine().mid(3);
        file.close();
    }

    QString NewIP = QString("192.168.1.") + QString::number(mask);

    if(QString::compare(OldIP,NewIP) == 0){
        //do nothing
    }else{
        //根据拨码开关设置ip
        QStringList NetWorkConfigInfo;
        NetWorkConfigInfo << tr("IP=%1\n").arg(NewIP)
                             << tr("Mask=%1\n").arg("255.255.255.0")
                             << tr("Gateway=%1\n").arg("192.168.1.1")
                             << tr("DNS=%1\n").arg("202.96.209.133")
                             << tr("MAC=%1\n").arg("00:60:6E:E5:68:A0");
        QString ConfigureInfo = NetWorkConfigInfo.join("");
        QFile file("/etc/eth0-setting");
        if(file.open(QFile::WriteOnly | QFile::Truncate)){
            file.write(ConfigureInfo.toAscii());
            file.close();
        }

        //重启生效
        system("reboot");
    }
}
