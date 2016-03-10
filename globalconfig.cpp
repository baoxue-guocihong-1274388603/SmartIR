#include "globalconfig.h"

QString GlobalConfig::ServerIP = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/ServerIP");
QString GlobalConfig::MainIP = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/MainIP");
QString GlobalConfig::SubIP = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/SubIP");
QString GlobalConfig::MainDefenceID = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/MainDefenceID");
QString GlobalConfig::SubDefenceID = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/SubDefenceID");


QString GlobalConfig::LocalHostIP = CommonSetting::GetLocalHostIP();
QString GlobalConfig::Mask = CommonSetting::GetMask();
QString GlobalConfig::Gateway = CommonSetting::GetGateway();
QString GlobalConfig::MAC = CommonSetting::ReadMacAddress();

int GlobalConfig::AlarmHostAlarmPort = 6901;
int GlobalConfig::AlarmHostServerPort = 6902;
int GlobalConfig::MainControlServerPort = 6903;

QString GlobalConfig::GroupAddr = "224.0.0.17";
int GlobalConfig::GroupPort = 6905;


int GlobalConfig::MainStreamFactor = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/MainStreamBasicPoint").split("#").at(0).toInt();
QRect GlobalConfig::MainStreamSelectRect = GlobalConfig::GetMainStreamSelectRect();
QList<QPoint> GlobalConfig::MainStreamLightPoint = GlobalConfig::GetMainStreamLightPoint();

int GlobalConfig::SubStreamFactor = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/SubStreamBasicPoint").split("#").at(0).toInt();
QRect GlobalConfig::SubStreamSelectRect = GlobalConfig::GetSubStreamSelectRect();
QList<QPoint> GlobalConfig::SubStreamLightPoint = GlobalConfig::GetSubStreamLightPoint();

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

