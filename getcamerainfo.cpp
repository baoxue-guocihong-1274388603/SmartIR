#include "getcamerainfo.h"
#include "devicecontrol.h"
#include <QFile>
#include <QDebug>
#include <QDateTime>

GetCameraInfo *GetCameraInfo::_instance = 0;
QString GetCameraInfo::MainCamera = QString("");
QString GetCameraInfo::SubCamera = QString("");

#define TIMEMS QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")

GetCameraInfo::GetCameraInfo(QObject *parent) :
    QObject(parent)
{
    /************使能主控制杆摄像头电源**************/
    DeviceControl::Instance()->MainCameraPowerEnable();
    CommonSetting::Sleep(2000);

    //获取主控制摄像头设备文件
    system("find /sys/devices/platform -name video4linux > path.txt");//arm

    QString MainCameraPath;
    QFile MainCameraFile("path.txt");
    if(MainCameraFile.open(QFile::ReadOnly)){
        MainCameraPath =  QString(MainCameraFile.readAll()).trimmed();
        MainCameraFile.close();
    }
    if(MainCameraPath.isEmpty()){
        qDebug() << "not find main usb camera";
        GetCameraInfo::MainCamera = QString("");
    }else{
        QString cmd = QString("ls %1 > device.txt").arg(MainCameraPath);
        system(cmd.toAscii().data());
        QFile MainCameraDeviceFile("device.txt");
        if(MainCameraDeviceFile.open(QFile::ReadOnly)){
            GetCameraInfo::MainCamera = QString("/dev/") + QString(MainCameraDeviceFile.readAll()).trimmed();
            qDebug() << TIMEMS << "MainCamera = " << GetCameraInfo::MainCamera;
            MainCameraDeviceFile.close();
        }
    }

    /************使能辅助控制杆摄像头电源**************/
    DeviceControl::Instance()->SubCameraPowerEnable();
    CommonSetting::Sleep(2000);

    //获取辅助控制杆摄像头设备文件
    if(MainCameraPath.isEmpty()){
        system("find /sys/devices/platform -name video4linux > path.txt");//arm
    }else{
        system(tr("find /sys/devices/platform -name video4linux | grep -v \"%1\" > path.txt").arg(MainCameraPath).toAscii().data());
    }

    QString SubCameraPath;
    QFile SubCameraFile("path.txt");
    if(SubCameraFile.open(QFile::ReadOnly)){
        SubCameraPath =  QString(SubCameraFile.readAll()).trimmed();
        SubCameraFile.close();
    }
    if(SubCameraPath.isEmpty()){
        qDebug() << "not find sub usb camera";
        GetCameraInfo::SubCamera = QString("");
    }else{
        QString cmd = QString("ls %1 > device.txt").arg(SubCameraPath);
        system(cmd.toAscii().data());
        QFile SubCameraDeviceFile("device.txt");
        if(SubCameraDeviceFile.open(QFile::ReadOnly)){
            GetCameraInfo::SubCamera = QString("/dev/") + QString(SubCameraDeviceFile.readAll()).trimmed();
            qDebug() << TIMEMS << "SubCamera = " << GetCameraInfo::SubCamera;
            SubCameraDeviceFile.close();
        }
    }
}

void GetCameraInfo::GetMainCameraDevice()
{
    //切断主控制摄像头电源
    DeviceControl::Instance()->MainCameraPowerDisable();
    CommonSetting::Sleep(1000);

    system("find /sys/devices/platform -name video4linux > path.txt");//arm

    QString SubCameraPath;
    QFile SubCameraFile("path.txt");
    if(SubCameraFile.open(QFile::ReadOnly)){
        SubCameraPath =  QString(SubCameraFile.readAll()).trimmed();
        SubCameraFile.close();
    }

    /************使能主控制杆摄像头电源**************/
    DeviceControl::Instance()->MainCameraPowerEnable();
    CommonSetting::Sleep(3000);

    system(tr("find /sys/devices/platform -name video4linux | grep -v \"%1\" > path.txt").arg(SubCameraPath).toAscii().data());

    QString MainCameraPath;
    QFile MainCameraFile("path.txt");
    if(MainCameraFile.open(QFile::ReadOnly)){
        MainCameraPath =  QString(MainCameraFile.readAll()).trimmed();
        MainCameraFile.close();
    }
    if(MainCameraPath.isEmpty()){
        printf("not find main usb camera\n");
        GetCameraInfo::MainCamera = QString("");
    }else{
        QString cmd = QString("ls %1 > device.txt").arg(MainCameraPath);
        system(cmd.toAscii().data());
        QFile MainCameraDeviceFile("device.txt");
        if(MainCameraDeviceFile.open(QFile::ReadOnly)){
            GetCameraInfo::MainCamera = QString("/dev/") + QString(MainCameraDeviceFile.readAll()).trimmed();
            qDebug() << TIMEMS << "MainCamera = " << GetCameraInfo::MainCamera;
            MainCameraDeviceFile.close();
        }
    }
}

void GetCameraInfo::GetSubCameraDevice()
{
    //切断辅助控制摄像头电源
    DeviceControl::Instance()->SubCameraPowerDisable();
    CommonSetting::Sleep(1000);

    system("find /sys/devices/platform -name video4linux > path.txt");//arm

    QString MainCameraPath;
    QFile MainCameraFile("path.txt");
    if(MainCameraFile.open(QFile::ReadOnly)){
        MainCameraPath =  QString(MainCameraFile.readAll()).trimmed();
        MainCameraFile.close();
    }

    /************使能辅助控制杆摄像头电源**************/
    DeviceControl::Instance()->SubCameraPowerEnable();
    CommonSetting::Sleep(3000);

    system(tr("find /sys/devices/platform -name video4linux | grep -v \"%1\" > path.txt").arg(MainCameraPath).toAscii().data());

    QString SubCameraPath;
    QFile SubCameraFile("path.txt");
    if(SubCameraFile.open(QFile::ReadOnly)){
        SubCameraPath =  QString(SubCameraFile.readAll()).trimmed();
        SubCameraFile.close();
    }
    if(SubCameraPath.isEmpty()){
        printf("not find sub usb camera\n");
        GetCameraInfo::SubCamera = QString("");
    }else{
        QString cmd = QString("ls %1 > device.txt").arg(SubCameraPath);
        system(cmd.toAscii().data());
        QFile SubCameraDeviceFile("device.txt");
        if(SubCameraDeviceFile.open(QFile::ReadOnly)){
            GetCameraInfo::SubCamera = QString("/dev/") + QString(SubCameraDeviceFile.readAll()).trimmed();
            qDebug() << TIMEMS << "SubCamera = " << GetCameraInfo::SubCamera;
            SubCameraDeviceFile.close();
        }
    }
}
