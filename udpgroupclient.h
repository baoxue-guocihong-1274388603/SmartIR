#ifndef UDPGROUPCLIENT_H
#define UDPGROUPCLIENT_H

#include "CommonSetting.h"

class UdpGroupClient : public QObject
{
    Q_OBJECT
public:
    explicit UdpGroupClient(QObject *parent = 0);

signals:

public slots:
    void slotProcessPendingDatagrams();

private:
    QUdpSocket udp_socket;
};

#endif // UDPGROUPCLIENT_H
