#ifndef DEVICECONTROL_H
#define DEVICECONTROL_H

#include "CommonSetting.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define	WATCHDOG_IOCTL_BASE	 'W'
#define	WDIOC_KEEPALIVE		 _IOR(WATCHDOG_IOCTL_BASE, 5, int)
#define	WDIOC_SETTIMEOUT     _IOWR(WATCHDOG_IOCTL_BASE, 6, int)


class DeviceControl : public QObject
{
    Q_OBJECT
public:
    explicit DeviceControl(QObject *parent = 0);
    void MainCameraPowerEnable();//打开主控制杆摄像头电源
    void MainCameraPowerDisable();
    void SubCameraPowerEnable();//打开辅助控制杆摄像头电源
    void SubCameraPowerDisable();//关闭辅助控制杆摄像头电源
    uchar GetSwitchAddr();//获取拨码地址
    void UsbHubReset();//usb hub复位

    static DeviceControl *Instance() {
        static QMutex mutex;
        if (!_instance) {
            QMutexLocker locker(&mutex);
            if (!_instance) {
                _instance = new DeviceControl;
            }
        }
        return _instance;
    }

public slots:
    void slotFeedWatchDog();

public:
    int camera_fd;
    int usb_hub_fd;
    int ip_fd;
    int wdt_fd;

    static DeviceControl *_instance;

    QTimer *FeedWatchDogTimer;
};

#endif // DEVICECONTROL_H
