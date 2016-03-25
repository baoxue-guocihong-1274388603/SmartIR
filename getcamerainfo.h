#ifndef GETCAMERAINFO_H
#define GETCAMERAINFO_H

#include <QObject>
#include <QMutex>
#include <QMutexLocker>

class GetCameraInfo : public QObject
{
    Q_OBJECT
public:
    explicit GetCameraInfo(QObject *parent = 0);

    static GetCameraInfo *Instance() {
        static QMutex mutex;
        if (!_instance) {
            QMutexLocker locker(&mutex);
            if (!_instance) {
                _instance = new GetCameraInfo;
            }
        }
        return _instance;
    }
public:
    static GetCameraInfo *_instance;

    static QString MainCamera;//主控制杆摄像头设备文件
    static QString SubCamera;//辅助控制杆摄像头设备文件

    void GetMainCameraDevice();//主控制杆摄像头掉线时，获取主控制杆摄像头设备文件
    void GetSubCameraDevice();//辅助控制杆摄像头掉线时，获取辅助控制杆摄像头设备文件
};

#endif // GETCAMERAINFO_H
