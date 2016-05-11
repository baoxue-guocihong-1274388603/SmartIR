#include "globalconfig.h"

QString GlobalConfig::ServerIP = QString("192.168.1.239");
QString GlobalConfig::ServerPort = QString("6901");
QString GlobalConfig::MainIP = QString("");
QString GlobalConfig::SubIP = QString("");
QString GlobalConfig::MainDefenceID = QString("000");
QString GlobalConfig::SubDefenceID = QString("000");
int     GlobalConfig::CameraSleepTime = 300;
QString GlobalConfig::DeviceMacAddr = GlobalConfig::GenerateMAC();
QString GlobalConfig::DeviceIPAddrPrefix = QString("192.168.1");
int     GlobalConfig::TcpConnectTimeout = 100;

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

QString GlobalConfig::AppPath = "";
QString GlobalConfig::AppName = "";
QString GlobalConfig::ConfigFileName = "";

void GlobalConfig::init()
{
    GlobalConfig::AppPath = QApplication::applicationDirPath();
    GlobalConfig::AppName = QApplication::applicationName();
    GlobalConfig::ConfigFileName = QString("%1/%2_Config.ini").arg(GlobalConfig::AppPath).arg(GlobalConfig::AppName);

    bool retval = CommonSetting::FileExists(GlobalConfig::ConfigFileName);
    if (retval) {//配置文件存在
        QString str = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/ServerIP");
        if(!str.isEmpty()){
            GlobalConfig::ServerIP = str;
        }

        str = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/ServerPort");
        if(!str.isEmpty()){
            GlobalConfig::ServerPort = str;
        }

        GlobalConfig::MainIP = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/MainIP");
        GlobalConfig::SubIP = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/SubIP");

        str = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/MainDefenceID");
        if(!str.isEmpty()){
            GlobalConfig::MainDefenceID = str;
        }

        str = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/SubDefenceID");
        if(!str.isEmpty()){
            GlobalConfig::SubDefenceID = str;
        }

        str = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/CameraSleepTime");
        if(!str.isEmpty()){
            GlobalConfig::CameraSleepTime = str.toInt();
        }

        str = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/DeviceMacAddr");
        if(!str.isEmpty()){
            GlobalConfig::DeviceMacAddr = str;
        }

        str = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/DeviceIPAddrPrefix");
        if(!str.isEmpty()){
            GlobalConfig::DeviceIPAddrPrefix = str;
        }

        str = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/TcpConnectTimeout");
        if(!str.isEmpty()){
            GlobalConfig::TcpConnectTimeout = str.toInt();
        }

        GlobalConfig::MainStreamFactor =
                CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/MainStreamBasicPoint").split("#").at(0).toInt();
        GlobalConfig::MainStreamSelectRect = GlobalConfig::GetMainStreamSelectRect();
        GlobalConfig::MainStreamLightPoint = GlobalConfig::GetMainStreamLightPoint();

        GlobalConfig::SubStreamFactor =
                CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/SubStreamBasicPoint").split("#").at(0).toInt();
        GlobalConfig::SubStreamSelectRect = GlobalConfig::GetSubStreamSelectRect();
        GlobalConfig::SubStreamLightPoint = GlobalConfig::GetSubStreamLightPoint();
    }else{//配置文件不存在,使用默认值生成配置文件
        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/ServerIP",GlobalConfig::ServerIP);
        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/ServerPort",GlobalConfig::ServerPort);
        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/MainIP",GlobalConfig::MainIP);
        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/SubIP",GlobalConfig::SubIP);
        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/MainDefenceID",GlobalConfig::MainDefenceID);
        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/SubDefenceID",GlobalConfig::SubDefenceID);
        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/CameraSleepTime",QString::number(GlobalConfig::CameraSleepTime));
        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/DeviceMacAddr",GlobalConfig::DeviceMacAddr);
        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/DeviceIPAddrPrefix",GlobalConfig::DeviceIPAddrPrefix);
        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/TcpConnectTimeout",QString::number(GlobalConfig::TcpConnectTimeout));
        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/MainStreamBasicPoint",QString(""));
        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/SubStreamBasicPoint",QString(""));
    }
}

QRect GlobalConfig::GetMainStreamSelectRect()
{
   QStringList list = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/MainStreamBasicPoint").split("#");

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
    QStringList list = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/SubStreamBasicPoint").split("#");

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
    QString MainStreamBasicPoint = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/MainStreamBasicPoint");
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
    QString SubStreamBasicPoint = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/SubStreamBasicPoint");
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


QString GlobalConfig::GenerateMAC()
{
    system("echo $RANDOM | md5sum | sed 's/\\(..\\)/&:/g' | cut -c1-8 > /bin/mac.txt");
    return QString("00:60:6E:") + CommonSetting::ReadFile("/bin/mac.txt").simplified().trimmed().toUpper();
}
