#ifndef IPCONFIG_H
#define IPCONFIG_H

#include <QObject>
#include <QMutex>
#include <QMutexLocker>

class IPConfig : public QObject
{
    Q_OBJECT
public:
    explicit IPConfig(QObject *parent = 0);
    void Update();

    static IPConfig *Instance() {
        static QMutex mutex;
        if (!_instance) {
            QMutexLocker locker(&mutex);
            if (!_instance) {
                _instance = new IPConfig;
            }
        }
        return _instance;
    }

public:
    static IPConfig *_instance;
};

#endif // IPCONFIG_H
