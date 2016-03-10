#include "getcamerainfo.h"
#include "devicecontrol.h"
#include <QFile>
#include <QDebug>
#include <QDateTime>

GetCameraInfo::GetCameraInfo(QObject *parent) :
    QObject(parent)
{
    DeviceControl::Instance()->BuzzerDisable();

    //获取主控制摄像头设备文件
    system("rm -rf path.txt device.txt");
    system("find /sys/devices/platform -name video4linux > path.txt");//arm

    QString MainCameraPath;
    QFile MainCameraFile("path.txt");
    if(MainCameraFile.open(QFile::ReadOnly)){
        MainCameraPath =  QString(MainCameraFile.readAll()).trimmed();
        MainCameraFile.close();
    }
    if(MainCameraPath.isEmpty()){
        printf("not find main usb camera\n");
        MainCamera = QString("");
    }else{
        QString cmd = QString("ls %1 > device.txt").arg(MainCameraPath);
        system(cmd.toAscii().data());
        QFile MainCameraDeviceFile("device.txt");
        if(MainCameraDeviceFile.open(QFile::ReadOnly)){
            MainCamera = QString("/dev/") + QString(MainCameraDeviceFile.readAll()).trimmed();
            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "MainCamera = " << MainCamera;
            MainCameraDeviceFile.close();
        }
    }

    /************使能辅助控制杆摄像头电源**************/
    DeviceControl::Instance()->SubCameraPowerEnable();

    //获取辅助控制杆摄像头设备文件
    system("rm -rf path.txt device.txt");
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
        printf("not find sub usb camera\n");
        SubCamera = QString("");
    }else{
        QString cmd = QString("ls %1 > device.txt").arg(SubCameraPath);
        system(cmd.toAscii().data());
        QFile SubCameraDeviceFile("device.txt");
        if(SubCameraDeviceFile.open(QFile::ReadOnly)){
            SubCamera = QString("/dev/") + QString(SubCameraDeviceFile.readAll()).trimmed();
            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "SubCamera = " << SubCamera;
            SubCameraDeviceFile.close();
        }
    }
}
