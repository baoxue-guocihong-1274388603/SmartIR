#ifndef IPCONFIG_H
#define IPCONFIG_H

#include <QObject>

class IPConfig : public QObject
{
    Q_OBJECT
public:
    explicit IPConfig(QObject *parent = 0);
};

#endif // IPCONFIG_H
