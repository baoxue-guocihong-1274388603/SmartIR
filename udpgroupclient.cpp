#include "udpgroupclient.h"
#include "globalconfig.h"

#define TIMEMS QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")

UdpGroupClient::UdpGroupClient(QObject *parent) :
    QObject(parent)
{
    system("route add -net 224.0.0.0 netmask 224.0.0.0 eth0");
    udp_socket.bind(QHostAddress::Any, GlobalConfig::GroupPort);
    udp_socket.setSocketOption(QAbstractSocket::MulticastLoopbackOption, 0);//禁止回环许可
    udp_socket.joinMulticastGroup(QHostAddress(GlobalConfig::GroupAddr));//加入组播地址
    connect(&udp_socket,SIGNAL(readyRead()),this,SLOT(slotProcessPendingDatagrams()));
}

void UdpGroupClient::slotProcessPendingDatagrams()
{
    while(udp_socket.hasPendingDatagrams()){
        QByteArray datagram;
        datagram.resize(udp_socket.pendingDatagramSize());
        udp_socket.readDatagram(datagram.data(), datagram.size());

        qDebug() << TIMEMS << "slotProcessPendingDatagrams" << datagram;

        datagram = datagram.mid(20);

        QDomDocument dom;
        QString errorMsg;
        int errorLine, errorColumn;
        bool isSearchDevice = false;

        if(!dom.setContent(datagram, &errorMsg, &errorLine, &errorColumn)) {
            qDebug() << TIMEMS << "Parse error at line " << errorLine <<","<< "column " << errorColumn << ","<< errorMsg;
            continue;
        }

        QDomElement RootElement = dom.documentElement();//获取根元素
        if(RootElement.tagName() == "Server"){ //根元素名称
            QDomNode firstChildNode = RootElement.firstChild();//第一个子节点
            while(!firstChildNode.isNull()){
                if(firstChildNode.nodeName() == "SearchDevice"){
                    isSearchDevice = true;
                }

                firstChildNode = firstChildNode.nextSibling();//下一个节点
            }

            if(isSearchDevice){
                QString MessageMerge;
                QDomDocument AckDom;

                //xml声明
                QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                //创建根元素
                QDomElement RootElement = AckDom.createElement("Device");
                RootElement.setAttribute("DeviceIP",GlobalConfig::LocalHostIP);
                RootElement.setAttribute("Mask",GlobalConfig::Mask);
                RootElement.setAttribute("Gateway",GlobalConfig::Gateway);
                RootElement.setAttribute("MAC",GlobalConfig::MAC);
                if(GlobalConfig::MainDefenceID.isEmpty()){
                    RootElement.setAttribute("DefenceID","000");
                }else{
                    RootElement.setAttribute("DefenceID",GlobalConfig::MainDefenceID);
                }
                AckDom.appendChild(RootElement);

                QTextStream Out(&MessageMerge);
                AckDom.save(Out,4);

                int length = MessageMerge.size();
                MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;
                udp_socket.writeDatagram(MessageMerge.toAscii(),QHostAddress(GlobalConfig::GroupAddr),GlobalConfig::GroupPort);

            }
        }
    }
}
