#include "operatecamera.h"

OperateCamera::OperateCamera(int input,int width,int height,int BPP,QObject *parent) :
    QObject(parent)
{
    yuyv_buff = (uchar*)malloc(width * height * BPP / 8);
    this->input = input;
}

int OperateCamera::OpenCamera(QString VideoName)
{
    int fd = open(VideoName.toAscii().data(), O_RDWR  | O_NONBLOCK, 0);
    if(fd == -1){
        fprintf(stderr, "Cannot open '%s',errno = %d,error info = %s\n",VideoName.toAscii().data(), errno, strerror(errno));
    }

    return fd;
}


int OperateCamera::InitCameraDevice(int fd,ImgBuffer **imgBufsPtr,int width,int height,
                     __u32 pixelformat)
{
    //查询设备属性
    struct v4l2_capability cap;
    if(ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0){
        qDebug() << "Error in VIDIOC_QUERYCAP";
        close(fd);
        return -1;
    }

    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
        qDebug() << "it is not a video capture device";
        close(fd);
        return -1;
    }

    if(!(cap.capabilities & V4L2_CAP_STREAMING)){
        qDebug() << "It can not streaming";
        close(fd);
        return -1;
    }

    qDebug() << "it is a video capture device";
    qDebug() << "It can streaming";
    qDebug() << "driver:" << cap.driver;
    qDebug() << "card:" << cap.card;
    qDebug() << "bus_info:" << cap.bus_info;
    qDebug() << "version:" << cap.version;

    if(cap.capabilities == 0x4000001){
        qDebug() << "capabilities:" << "V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING";
    }

    //设置视频输入源
    int input = this->input;
    if(ioctl(fd, VIDIOC_S_INPUT, &input) < 0){
        qDebug() << "Error in VIDIOC_S_INPUT";
        close(fd);
        return -1;
    }

    //查询并显示所有支持的格式
//    struct v4l2_fmtdesc fmtdesc;
//    fmtdesc.index = 0;
//    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
//    printf("Support format:\n");
//    while(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != -1){
//        printf("\tpixelformat = %c%c%c%c, description = %s\n",
//               fmtdesc.pixelformat & 0xFF,
//               (fmtdesc.pixelformat >> 8) & 0xFF,
//               (fmtdesc.pixelformat >> 16) & 0xFF,
//               (fmtdesc.pixelformat >> 24) & 0xFF,
//               fmtdesc.description);
//        fmtdesc.index++;
//    }

    //设置图片格式和分辨率
    struct v4l2_format fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.pixelformat = pixelformat;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;

    if(ioctl(fd, VIDIOC_S_FMT, &fmt) < 0){
        printf("Error in VIDIOC_S_FMT,not support format pixelformat = %c%c%c%c",
               pixelformat & 0xFF,
               (pixelformat >> 8) & 0xFF,
               (pixelformat >> 16) & 0xFF,
               (pixelformat >> 24) & 0xFF);
        close(fd);
        return -1;
    }

    //查看图片格式和分辨率,判断是否设置成功
    if(ioctl(fd, VIDIOC_G_FMT, &fmt) < 0){
        printf("Error in VIDIOC_G_FMT\n");
        close(fd);
        return -1;
    }
    qDebug() << "width = " << fmt.fmt.pix.width;
    qDebug() << "height = " << fmt.fmt.pix.height;
    printf("pixelformat = %c%c%c%c\n",
           fmt.fmt.pix.pixelformat & 0xFF,
           (fmt.fmt.pix.pixelformat >> 8) & 0xFF,
           (fmt.fmt.pix.pixelformat >> 16) & 0xFF,
           (fmt.fmt.pix.pixelformat >> 24) & 0xFF);

    //设置帧格式
    struct v4l2_streamparm parm;
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1; // 分子。 例：1
    parm.parm.capture.timeperframe.denominator = 25; //分母。 例：30
    parm.parm.capture.capturemode = 0;

    if(ioctl(fd, VIDIOC_S_PARM, &parm) < 0){
        qDebug() << "Error in VIDIOC_S_PARM";
        close(fd);
        return -1;
    }

    if(ioctl(fd, VIDIOC_G_PARM, &parm) < 0){
        qDebug() << "Error in VIDIOC_G_PARM";
        close(fd);
        return -1;
    }
    qDebug() << "streamparm:\n\tnumerator =" << parm.parm.capture.timeperframe.numerator << "denominator =" << parm.parm.capture.timeperframe.denominator << "capturemode =" <<           parm.parm.capture.capturemode;

    //申请和管理缓冲区
    struct v4l2_requestbuffers reqbuf;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = BUFFER_NUMBER;

    if(ioctl(fd, VIDIOC_REQBUFS, &reqbuf) < 0){
        qDebug() << "Error in VIDIOC_REQBUFS";
        close(fd);
        return -1;
    }

    ImgBuffer *imgBuffers = (ImgBuffer *)calloc(BUFFER_NUMBER,sizeof(ImgBuffer));
    if(imgBuffers == NULL){
        qDebug() << "Error in calloc";
        close(fd);
        return -1;
    }

    struct v4l2_buffer buffer;
    for(int numBufs = 0; numBufs < BUFFER_NUMBER; numBufs++){
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = numBufs;

        if(ioctl(fd, VIDIOC_QUERYBUF, &buffer) < 0){

            qDebug() << "Error in VIDIOC_QUERYBUF";
            free(imgBuffers);
            close(fd);
            return -1;
        }
        qDebug() << "buffer.length =" << buffer.length;
        qDebug() << "buffer.bytesused =" << buffer.bytesused;
        imgBuffers[numBufs].length = buffer.length;
        imgBuffers[numBufs].start = (__u8 *)mmap (NULL,buffer.length,
PROT_READ | PROT_WRITE,MAP_SHARED,fd,buffer.m.offset);
        if(MAP_FAILED == imgBuffers[numBufs].start){
            qDebug() << "Error in mmap";
            free(imgBuffers);
            close(fd);
            return -1;
        }

        //把缓冲帧放入队列
        if(ioctl(fd, VIDIOC_QBUF, &buffer) < 0){
            qDebug() << "Error in VIDIOC_QBUF";
            for(int i = 0; i <= numBufs; i++){
                munmap(imgBuffers[i].start, imgBuffers[i].length);
            }
            free(imgBuffers);
            close(fd);
            return -1;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_STREAMON, &type) < 0){
        qDebug() << "Error in VIDIOC_STREAMON";
        for(int i = 0; i < BUFFER_NUMBER; i++){
            munmap(imgBuffers[i].start, imgBuffers[i].length);
        }
        free(imgBuffers);
        close(fd);
        return -1;
    }

    *imgBufsPtr = imgBuffers;

    return fd;
}

void OperateCamera::CleanupCaptureDevice(int fd, ImgBuffer **imgBuffers)
{
    if(*imgBuffers != NULL){
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(ioctl(fd, VIDIOC_STREAMOFF, &type) < 0){
            qDebug() << "Error in VIDIOC_STREAMOFF";
        }

        for(int i = 0; i < BUFFER_NUMBER; i++){
            munmap((*imgBuffers)[i].start, (*imgBuffers)[i].length);
        }
        close(fd);
        free(*imgBuffers);
        *imgBuffers = NULL;
        qDebug() << "CleanupCaptureDevice";
    }
}

int OperateCamera::ReadFrame(int fd,ImgBuffer *imgBuffers)
{
    //等待摄像头采集到一桢数据
    for(;;){
        fd_set fds;
        struct timeval tv;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        int r = select(fd + 1,&fds,NULL,NULL,&tv);
        if(-1 == r){
            if(EINTR == errno)
                continue;
            qDebug() << "select error";
            return -1;
        }else if(0 == r) {
            qDebug() << "select timeout";
            return -1;
        }else{//采集到一张图片
            qDebug() << "capture frame";
            break;
        }
    }

    //从缓冲区取出一个缓冲帧
    struct v4l2_buffer buffer;
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    if(ioctl(fd, VIDIOC_DQBUF, &buffer) < 0){
        qDebug() << "Error in VIDIOC_DQBUF";
        return -1;
    }

    memcpy(yuyv_buff,(uchar *)imgBuffers[buffer.index].start,imgBuffers[buffer.index].length);

    //将取出的缓冲帧放回缓冲区
    if(ioctl(fd, VIDIOC_QBUF, &buffer) < 0){
        qDebug() << "Error in VIDIOC_QBUF";
        return -1;
    }

    return buffer.index;
}
