#ifndef GETCAMERAINFO_H
#define GETCAMERAINFO_H

#include <QObject>

class GetCameraInfo : public QObject
{
    Q_OBJECT
public:
    explicit GetCameraInfo(QObject *parent = 0);

public:
    QString MainCamera;//主控制杆摄像头设备文件
    QString SubCamera;//辅助控制杆摄像头设备文件
};

#endif // GETCAMERAINFO_H
