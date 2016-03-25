#ifndef WORKTHREAD_H
#define WORKTHREAD_H

#include <QThread>
#include "operatecamera.h"

class WorkThread : public QThread
{
    Q_OBJECT
public:
    explicit WorkThread(QString VideoName, quint32 width, quint32 height, quint32 BPP, quint32 pixelformat, QObject *parent = 0);

    QImage ReadFrame();
    void ProcessFrame(QImage &image);
    bool YUYVToRGB24_FFmpeg(uchar *pYUV,uchar *pRGB24);

protected:
    void run();

signals:
    void signalAlarmImage();
    void signalUSBCameraOffline();

public:
    OperateCamera *operatecamera;
    QString VideoName;
    quint32 width;
    quint32 height;

    volatile bool isStopCapture;//拍照是暂停采集图片
    volatile bool isReInitialize;
    volatile bool isVaild;
    volatile bool isAlarm;

    int factor;
    QRect SelectRect;
    QList<QPoint> LightPoint;

    uchar *yuyv_buff;
    uchar *rgb_buff;

    qint64 ThreadState;

    QImage NormalImage;
    QImage AlarmImage;
};

#endif // WORKTHREAD_H
