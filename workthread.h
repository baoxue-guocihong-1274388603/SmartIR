#ifndef WORKTHREAD_H
#define WORKTHREAD_H

#include <math.h>
#include <sys/time.h>
#include "opencv2/opencv.hpp"
#include "operatecamera.h"
#include "jpegencode.h"
#include "devicecontrol.h"

//必须加以下内容,否则编译不能通过,为了兼容C和C99标准
#ifndef INT64_C
#define INT64_C
#define UINT64_C
#endif

//引入ffmpeg头文件
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
}

class WorkThread : public QThread
{
    Q_OBJECT
public:
    enum ThreadType{
        MainStream,
        SubStream
    };

    explicit WorkThread(QString VideoName, int input, int width, int height, int BPP, enum ThreadType tt, QObject *parent = 0);
    void CleanupCaptureDevice();

    QImage ReadFrame();
    void ProcessFrame(QImage &image);
    bool YUYVToRGB24_FFmpeg(uchar *pYUV,uchar *pRGB24);

protected:
    void run();

signals:
    void signalCaptureNormalFrame(QImage &img);
    void signalCaputreAlarmFrame(QImage &img);
    void signalUSBCameraOffline();

public:
    QString VideoName;
    unsigned int width;
    unsigned int height;
    unsigned int BPP;
    OperateCamera *operatecamera;
    int camera_fd;
    ImgBuffer *img_buffers;
    volatile bool isStopCapture;//拍照是暂停采集图片
    volatile bool isReInitialize;
    volatile bool isVaild;
    volatile bool isAlarm;
    int factor;
    QRect SelectRect;
    QList<QPoint> LightPoint;

    uchar *yuyv_buff;
    uchar *rgb_buff;

    enum ThreadType type;//主要是用来区分区分是主控制杆工作线程，还是辅助控制杆工作线程
    //主控制杆工作线程采用jpeg编码，辅助工作线程采用ffmpeg转换

    qint64 ThreadState;

    JpegEncode *jpeg;
};

#endif // WORKTHREAD_H
