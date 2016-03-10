#ifndef JPEGENCODE_H
#define JPEGENCODE_H

#include <QObject>
#include <QMutex>
#include <QImage>
#include "s3c-jpeg.h"
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

class JpegEncode : public QObject
{
    Q_OBJECT
public:
    explicit JpegEncode(QObject *parent = 0);
    void jpeg_init();
    void jpeg_uninit();
    QImage jpeg_encode(const char *ptr,int len);

    static JpegEncode *Instance() {
        static QMutex mutex;
        if (!_instance) {
            QMutexLocker locker(&mutex);
            if (!_instance) {
                _instance = new JpegEncode;
            }
        }
        return _instance;
    }


public:
    static JpegEncode *_instance;

    struct jpg_args arg;
    uchar *JPEGInputBuffer;

    int fd;
};

#endif // JPEGENCODE_H
