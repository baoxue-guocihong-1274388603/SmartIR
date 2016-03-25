#include "globalconfig.h"

QString GlobalConfig::ServerIP = QString("192.168.1.239");
int     GlobalConfig::ServerPort = 6901;
QString GlobalConfig::MainIP = QString("");
QString GlobalConfig::SubIP = QString("");
QString GlobalConfig::MainDefenceID = QString("000");
QString GlobalConfig::SubDefenceID = QString("000");
int     GlobalConfig::CameraSleepTime = 300;
QString GlobalConfig::DeviceMacAddr = QString("00:01:02:03:04:05");
QString GlobalConfig::DeviceIPAddrPrefix = QString("192.168.1.");

int GlobalConfig::MainStreamFactor = 0;
QRect GlobalConfig::MainStreamSelectRect = QRect();
QList<QPoint> GlobalConfig::MainStreamLightPoint = QList<QPoint>();

int GlobalConfig::SubStreamFactor = 0;
QRect GlobalConfig::SubStreamSelectRect = QRect();
QList<QPoint> GlobalConfig::SubStreamLightPoint = QList<QPoint>();

int GlobalConfig::AlarmHostServerPort = 6902;
int GlobalConfig::MainControlServerPort = 6903;
int GlobalConfig::UpgradePort = 6904;

QString GlobalConfig::GroupAddr = "224.0.0.17";
int GlobalConfig::GroupPort = 6905;

QString GlobalConfig::LocalHostIP = CommonSetting::GetLocalHostIP();
QString GlobalConfig::Mask = CommonSetting::GetMask();
QString GlobalConfig::Gateway = CommonSetting::GetGateway();
QString GlobalConfig::MAC = CommonSetting::ReadMacAddress();

void GlobalConfig::init()
{
    bool retval = CommonSetting::FileExists("/bin/config.ini");
    if (retval) {
        //配置文件存在
        GlobalConfig::ServerIP = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/ServerIP");
        GlobalConfig::ServerPort = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/ServerPort").toInt();
        GlobalConfig::MainIP = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/MainIP");
        GlobalConfig::SubIP = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/SubIP");
        GlobalConfig::MainDefenceID = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/MainDefenceID");
        GlobalConfig::SubDefenceID = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/SubDefenceID");
        GlobalConfig::CameraSleepTime = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/CameraSleepTime").toInt();

        GlobalConfig::MainStreamFactor = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/MainStreamBasicPoint").split("#").at(0).toInt();
        GlobalConfig::MainStreamSelectRect = GlobalConfig::GetMainStreamSelectRect();
        GlobalConfig::MainStreamLightPoint = GlobalConfig::GetMainStreamLightPoint();

        GlobalConfig::SubStreamFactor = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/SubStreamBasicPoint").split("#").at(0).toInt();
        GlobalConfig::SubStreamSelectRect = GlobalConfig::GetSubStreamSelectRect();
        GlobalConfig::SubStreamLightPoint = GlobalConfig::GetSubStreamLightPoint();
    }
}

QRect GlobalConfig::GetMainStreamSelectRect()
{
   QStringList list = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/MainStreamBasicPoint").split("#");

   if(list.size() >= 2){
       QString SelectRect = list.at(1);
       int left = SelectRect.split(".").at(0).toInt();
       int top = SelectRect.split(".").at(1).toInt();
       int width = SelectRect.split(".").at(2).toInt();
       int height = SelectRect.split(".").at(3).toInt();

       QRect rect(left,top,width,height);
       return rect;
   }

   return QRect();
}

QRect GlobalConfig::GetSubStreamSelectRect()
{
    QStringList list = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/SubStreamBasicPoint").split("#");

    if(list.size() >= 2){
        QString SelectRect = list.at(1);
        int left = SelectRect.split(".").at(0).toInt();
        int top = SelectRect.split(".").at(1).toInt();
        int width = SelectRect.split(".").at(2).toInt();
        int height = SelectRect.split(".").at(3).toInt();

        QRect rect(left,top,width,height);
        return rect;
    }

    return QRect();
}

QList<QPoint> GlobalConfig::GetMainStreamLightPoint()
{
    QString MainStreamBasicPoint = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/MainStreamBasicPoint");
    QStringList list = MainStreamBasicPoint.split("#");

    QList<QPoint> list_point;
    for(int i = 2; i < list.size(); i++){
        int x = list.at(i).split(".").at(0).toInt();
        int y = list.at(i).split(".").at(1).toInt();

        QPoint point(x,y);
        list_point.append(point);
    }

    return list_point;
}

QList<QPoint> GlobalConfig::GetSubStreamLightPoint()
{
    QString SubStreamBasicPoint = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/SubStreamBasicPoint");
    QStringList list = SubStreamBasicPoint.split("#");

    QList<QPoint> list_point;
    for(int i = 2; i < list.size(); i++){
        int x = list.at(i).split(".").at(0).toInt();
        int y = list.at(i).split(".").at(1).toInt();

        QPoint point(x,y);
        list_point.append(point);
    }

    return list_point;
}
