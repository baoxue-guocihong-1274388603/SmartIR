#ifndef OPERATECAMERA_H
#define OPERATECAMERA_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QDebug>
#include <fcntl.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#define BUFFER_NUMBER 4

typedef struct ImgBuffer
{
    __u8 *start;
    size_t length;
}ImgBuffer;

class OperateCamera : public QObject
{
    Q_OBJECT
public:
    explicit OperateCamera(int input, int width, int height, int BPP, QObject *parent = 0);

    int OpenCamera(QString VideoName);
    int InitCameraDevice(int fd, ImgBuffer **imgBufsPtr, int width, int height,
                         __u32 pixelformat);
    void CleanupCaptureDevice(int fd, ImgBuffer **imgBuffers);
    int ReadFrame(int fd, ImgBuffer *imgBuffers);

public:
    uchar *yuyv_buff;
    int input;
};

#endif // OPERATECAMERA_H
