#include "jpegencode.h"
#include <QDebug>

JpegEncode *JpegEncode::_instance = 0;

JpegEncode::JpegEncode(QObject *parent) :
    QObject(parent)
{
    jpeg_init();
}

void JpegEncode::jpeg_init()
{
    fd = open("/dev/s3c-jpg",O_RDWR);
    if(fd < 0){
        qDebug() << "open /dev/s3c-jpg failed";
        fd = -1;
    }

    //必须为640*480*4或者640*480*5以上,不能为640*480*1、640*480*2 、640*480*3
    arg.mapped_addr = (char *)mmap(NULL,640*480*4,PROT_READ | PROT_WRITE,MAP_SHARED, fd, 0);
    if(arg.mapped_addr == MAP_FAILED){
        qDebug() << "s3c-jpg Error in mmap";
        fd = -1;
    }

    arg.enc_param = (struct jpg_enc_proc_param *)malloc(sizeof(struct jpg_enc_proc_param));
    if(arg.enc_param == NULL){
        qDebug() << "struct jpg_enc_proc_param malloc failed";
        fd = -1;
    }
    memset(arg.enc_param,0,sizeof(struct jpg_enc_proc_param));

    arg.enc_param->sample_mode = JPG_422;
    arg.enc_param->enc_type = JPG_MAIN;
    arg.enc_param->in_format = JPG_MODESEL_YCBCR;
    arg.enc_param->quality = JPG_QUALITY_LEVEL_1;
    arg.enc_param->width = 640;
    arg.enc_param->height = 480;

    JPEGInputBuffer = (uchar *)ioctl(fd, IOCTL_JPG_GET_FRMBUF, arg.mapped_addr);
    if(JPEGInputBuffer == NULL){
        qDebug() << "s3c-jpg input buffer is NULL";
        fd = -1;
    }
}

void JpegEncode::jpeg_uninit()
{
    munmap(arg.mapped_addr,640*480*4);
    free(arg.enc_param);
    close(fd);
	fd = -1;
}

QImage JpegEncode::jpeg_encode(const char *ptr, int len)
{
    if(fd != -1){
        enum jpg_return_status ret = JPG_FAIL;

        memcpy(JPEGInputBuffer,ptr,len);

        ret = (enum jpg_return_status)ioctl(fd,IOCTL_JPG_ENCODE,&arg);
        if(ret != JPG_SUCCESS){
            qDebug() << "IOCTL_JPG_ENCODE failed";
            return QImage();
        }

        arg.out_buf = (char *)ioctl(fd,IOCTL_JPG_GET_STRBUF,arg.mapped_addr);
        if(arg.out_buf == NULL){
            qDebug() << "s3c-jpg out buffer is NULL";
            return QImage();
        }

        QImage image;
        image.loadFromData((uchar *)arg.out_buf,arg.enc_param->file_size);

        return image;
    }

    return QImage();
}
