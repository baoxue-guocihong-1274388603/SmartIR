#include "devicecontrol.h"
#include <stdio.h>

DeviceControl *DeviceControl::_instance = 0;

DeviceControl::DeviceControl(QObject *parent) :
    QObject(parent)
{
    camera_fd = open("/dev/s5pv210_camera",O_RDWR);
    if(camera_fd == -1){
        printf("%s:%s open /dev/s5pv210_camera failed\n",__FILE__,__FUNCTION__);
        return;
    }

    usb_hub_fd = open("/dev/s5pv210_usb_hub",O_RDWR);
    if(usb_hub_fd == -1){
        printf("%s:%s open /dev/s5pv210_usb_hub failed\n",__FILE__,__FUNCTION__);
        return;
    }

    alarm_fd = open("/dev/s5pv210_alarm",O_RDWR);
    if(alarm_fd == -1){
        printf("%s:%s open /dev/s5pv210_alarm failed\n",__FILE__,__FUNCTION__);
        return;
    }

    buzzer_fd = open("/dev/s5pv210_buzzer",O_RDWR);
    if(alarm_fd == -1){
        printf("%s:%s open /dev/s5pv210_buzzer failed\n",__FILE__,__FUNCTION__);
        return;
    }

    ip_fd = open("/dev/s5pv210_ip",O_RDONLY);
    if(ip_fd == -1){
        printf("%s:%s open /dev/s5pv210_ip failed\n",__FILE__,__FUNCTION__);
    }

    wdt_fd = open("/dev/watchdog",O_RDWR);
    if(wdt_fd == -1){
        printf("%s:%s open /dev/watchdog failed\n",__FILE__,__FUNCTION__);
    }

    //开启看门狗
    ioctl(wdt_fd,WDIOC_SETTIMEOUT,15);

    FeedWatchDogTimer = new QTimer(this);
    FeedWatchDogTimer->setInterval(5 * 1000);
    connect(FeedWatchDogTimer,SIGNAL(timeout()),this,SLOT(slotFeedWatchDog()));
    FeedWatchDogTimer->start();
}

void DeviceControl::SubCameraPowerEnable()
{
    ioctl(camera_fd,1);
}

void DeviceControl::SubCameraPowerDisable()
{
    ioctl(camera_fd,0);
}

uchar DeviceControl::GetSwitchAddr()//获取拨码地址
{
    uchar mask;
    if(read(ip_fd,&mask,1) < 0){
        printf("%s:%s read ip addr error\n",__FILE__,__FUNCTION__);
    }

    return mask;
}

void DeviceControl::UsbHubReset()
{
    ioctl(usb_hub_fd,0);
}

void DeviceControl::AlarmEnable()
{
    ioctl(alarm_fd,1);
}

void DeviceControl::AlarmDisable()
{
    ioctl(alarm_fd,0);
}

void DeviceControl::BuzzerEnable()
{
    ioctl(buzzer_fd,1);
}

void DeviceControl::BuzzerDisable()
{
    ioctl(buzzer_fd,0);
}

void DeviceControl::slotFeedWatchDog()
{
    ioctl(wdt_fd,WDIOC_KEEPALIVE);
}
