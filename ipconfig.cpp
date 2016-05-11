#include "ipconfig.h"
#include "devicecontrol.h"
#include "globalconfig.h"
#include <QFile>
#include <QTextStream>
#include <QStringList>

IPConfig *IPConfig::_instance = 0;

IPConfig::IPConfig(QObject *parent) :
    QObject(parent)
{

}

void IPConfig::Update()
{
    //获取拨码地址
    uchar mask = DeviceControl::Instance()->GetSwitchAddr();
    QString NewIP = GlobalConfig::DeviceIPAddrPrefix + QString(".") + QString::number(mask);

    if((QString::compare(GlobalConfig::LocalHostIP,NewIP) == 0) &&
            (QString::compare(GlobalConfig::DeviceMacAddr,GlobalConfig::MAC) == 0)){
        //do nothing
    }else{
        //根据拨码开关设置ip
        QStringList NetWorkConfigInfo;
        NetWorkConfigInfo << tr("IP=%1\n").arg(NewIP)
                             << tr("Mask=%1\n").arg("255.255.255.0")
                             << tr("Gateway=%1\n").arg(GlobalConfig::DeviceIPAddrPrefix + QString(".") +  QString("1"))
                             << tr("DNS=%1\n").arg("202.96.209.133")
                             << tr("MAC=%1\n").arg(GlobalConfig::DeviceMacAddr);
        QString ConfigureInfo = NetWorkConfigInfo.join("");
        QFile file("/etc/eth0-setting");
        if(file.open(QFile::WriteOnly | QFile::Truncate)){
            file.write(ConfigureInfo.toAscii());
            file.close();
        }

        //重启网卡
        system("/etc/init.d/ifconfig-eth0");

        GlobalConfig::LocalHostIP = CommonSetting::GetLocalHostIP();
        GlobalConfig::Gateway = CommonSetting::GetGateway();
        GlobalConfig::MAC = CommonSetting::ReadMacAddress();
    }
}
