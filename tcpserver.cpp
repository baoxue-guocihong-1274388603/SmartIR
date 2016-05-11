#include "tcpserver.h"

#define TIMEMS QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")

TcpServer::TcpServer(QString MainVideoName, QString SubVideoName, QObject *parent) :
    QObject(parent)
{
    MainStreamVideoName = MainVideoName;
    SubStreamVideoName = SubVideoName;

    isMainThreadExceptionQuit = false;
    isSubThreadExceptionQuit = false;

    //监听报警主机连接
    AlarmHostServer = new QTcpServer(this);
    connect(AlarmHostServer,SIGNAL(newConnection()),this,SLOT(slotProcessAlarmHostConfigConnection()));
    AlarmHostServer->listen(QHostAddress::Any,GlobalConfig::AlarmHostServerPort);

    //监听主控制杆连接
    MainControlSever = new QTcpServer(this);
    connect(MainControlSever,SIGNAL(newConnection()),this,SLOT(slotProcessMainControlConnection()));
    MainControlSever->listen(QHostAddress::Any,GlobalConfig::MainControlServerPort);

    MainControlSocket = NULL;

    //用来与右边的辅助控制杆通信-->使用tcp长连接
    if(!MainVideoName.isEmpty()){//最后一个点位没有主控制杆
        SubControlSocket = new QTcpSocket(this);
        connect(SubControlSocket,SIGNAL(readyRead()),this,SLOT(slotRecvSubControlMsg()));
        connect(SubControlSocket,SIGNAL(connected()),this,SLOT(slotSubControlConnected()));
        connect(SubControlSocket,SIGNAL(disconnected()),this,SLOT(slotSubControlDisConnected()));
        connect(SubControlSocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(slotSubControlDisplayError(QAbstractSocket::SocketError)));
        SubControlConnectStateFlag = TcpServer::DisConnectedState;
        SubControlSocket->connectToHost(GlobalConfig::SubIP,GlobalConfig::MainControlServerPort);

        //开启探测主控制杆与辅助控制杆tcp长连接的状态
        enableKeepAlive(SubControlSocket->socketDescriptor(),1,1,1);
    }

    //主控制杆采集线程
    if(!MainVideoName.isEmpty()){
        MainWorkThread = new WorkThread(MainVideoName,640,480,16,V4L2_PIX_FMT_YUYV,this);
        MainWorkThread->factor = GlobalConfig::MainStreamFactor;
        MainWorkThread->SelectRect = GlobalConfig::MainStreamSelectRect;
        MainWorkThread->LightPoint = GlobalConfig::MainStreamLightPoint;
        connect(MainWorkThread,SIGNAL(signalAlarmImage()),this, SLOT(slotMainAlarmImage()),Qt::BlockingQueuedConnection);
        connect(MainWorkThread,SIGNAL(signalUSBCameraOffline()),this,SLOT(slotMainUSBCameraOffline()),Qt::BlockingQueuedConnection);
        connect(MainWorkThread,SIGNAL(signalUSBCameraOnline()),this,SLOT(slotMainUSBCameraOnline()));
        MainWorkThread->start();
    }

    //辅助控制杆采集线程
    if(!SubVideoName.isEmpty()){
        SubWorkThread = new WorkThread(SubVideoName,640,480,16,V4L2_PIX_FMT_YUYV,this);
        SubWorkThread->factor = GlobalConfig::SubStreamFactor;
        SubWorkThread->SelectRect = GlobalConfig::SubStreamSelectRect;
        SubWorkThread->LightPoint = GlobalConfig::SubStreamLightPoint;
        connect(SubWorkThread,SIGNAL(signalAlarmImage()),this, SLOT(slotSubAlarmImage()),Qt::BlockingQueuedConnection);
        connect(SubWorkThread,SIGNAL(signalUSBCameraOffline()),this,SLOT(slotSubUSBCameraOffline()),Qt::BlockingQueuedConnection);
        connect(SubWorkThread,SIGNAL(signalUSBCameraOnline()),this,SLOT(slotSubUSBCameraOnline()));
        SubWorkThread->start();
    }

    //专门用来处理报警主机下发的配置信息
    ProcessAlarmHostConfigMsgTimer = new QTimer(this);
    ProcessAlarmHostConfigMsgTimer->setInterval(300);
    connect(ProcessAlarmHostConfigMsgTimer,SIGNAL(timeout()),this,SLOT(slotProcessAlarmHostConfigMsg()));
    ProcessAlarmHostConfigMsgTimer->start();

    //用来发送所有报警信息，包括主控制杆和辅助控制杆以及其他报警信息
    SendAlarmMsgTimer = new QTimer(this);
    SendAlarmMsgTimer->setInterval(500);
    connect(SendAlarmMsgTimer,SIGNAL(timeout()),this,SLOT(slotSendAlarmMsg()));
    SendAlarmMsgTimer->start();

    //用来设置系统时间
    SetSystemTimeTimer = new QTimer(this);
    SetSystemTimeTimer->setInterval(30 * 60 * 1000);
    connect(SetSystemTimeTimer,SIGNAL(timeout()),this,SLOT(slotSetSystemTime()));
    SetSystemTimeTimer->start();

    //用来重连右边的辅助控制杆
    if(!MainVideoName.isEmpty()){//最后一个点位没有主控制杆
        ReConnectSubControlTimer = new QTimer(this);
        ReConnectSubControlTimer->setInterval(3000);
        connect(ReConnectSubControlTimer,SIGNAL(timeout()),this,SLOT(slotReConnectSubControl()));
    }

    //主要用来检测工作线程的状态
    CheckWorkThreadStateTimer = new QTimer(this);
    CheckWorkThreadStateTimer->setInterval(5000);
    connect(CheckWorkThreadStateTimer,SIGNAL(timeout()),this,SLOT(slotCheckWorkThreadState()));
    CheckWorkThreadStateTimer->start();

    //串口1用来与51单片机通信
    operate_serial = new OperateSerial(this);
    connect(operate_serial,SIGNAL(signalAlarmMsg(quint8)),this,SLOT(slotOtherAlarmMsg(quint8)));
}

void TcpServer::slotProcessAlarmHostConfigConnection()
{
    QTcpSocket *AlarmHostConfigSocket = AlarmHostServer->nextPendingConnection();

    connect(AlarmHostConfigSocket, SIGNAL(readyRead()), this, SLOT(slotRecvAlarmHostConfigMsg()));
    connect(AlarmHostConfigSocket, SIGNAL(disconnected()), this, SLOT(slotAlarmHostConfigDisconnect()));
    qDebug() << TIMEMS << "AlarmHostConfig Connect" << "\tIP =" << AlarmHostConfigSocket->peerAddress().toString() << "\tPort =" << AlarmHostConfigSocket->peerPort();

    enableKeepAlive(AlarmHostConfigSocket->socketDescriptor(),1,1,1);

    tcpClientsUnique.append(AlarmHostConfigSocket);
}

void TcpServer::slotRecvAlarmHostConfigMsg()
{
    QTcpSocket *AlarmHostConfigSocket = (QTcpSocket *)sender();
    QByteArray data = AlarmHostConfigSocket->readAll();

    if(data.contains("StopServices")){
        AlarmHostConfigBuffer.clear();
        tcpClients.clear();
    }

    if(data.contains("Reboot")){
        AlarmHostConfigBuffer.clear();
        tcpClients.clear();
    }

    AlarmHostConfigBuffer.append(data);
    tcpClients.append(AlarmHostConfigSocket);
}

void TcpServer::slotAlarmHostConfigDisconnect()
{
    QTcpSocket *AlarmHostConfigSocket = (QTcpSocket *)sender();

    qDebug() << TIMEMS << "AlarmHost Disconnect" << "\tIP =" << AlarmHostConfigSocket->peerAddress().toString() << "\tPort =" << AlarmHostConfigSocket->peerPort();

    int size = tcpClientsUnique.size();
    for(int i = 0; i < size; i++){
        if(tcpClientsUnique.at(i) == AlarmHostConfigSocket){
            tcpClientsUnique.removeAt(i);
            break;
        }
    }
}

void TcpServer::slotProcessMainControlConnection()
{
    MainControlSocket = MainControlSever->nextPendingConnection();
    connect(MainControlSocket, SIGNAL(readyRead()), this, SLOT(slotRecvMainControlMsg()));
    connect(MainControlSocket, SIGNAL(disconnected()), this, SLOT(slotMainControlDisconnect()));
    qDebug() << TIMEMS << "MainControl Connect" << "\tIP =" << MainControlSocket->peerAddress().toString() << "\tPort =" << MainControlSocket->peerPort();
}

void TcpServer::slotRecvMainControlMsg()
{
    if(MainControlSocket != NULL){
        QDomDocument dom;
        QString errorMsg;
        int errorLine, errorColumn;
        bool isSetPWM = false;//添加根据pwm来设置辅助控制杆灯珠的亮度
        bool isGetSubStream = false;//本设备返回辅助控制杆基准图片给报警主机
        quint8 PWM;

        if(MainControlSocket->isOpen() && MainControlSocket->isReadable()) {
            MainControlBuffer.append(MainControlSocket->readAll());
            QByteArray FullPackage;

            if(MainControlBuffer.size() <= 97){
                qDebug() << TIMEMS << "MainControlBuffer.size() <= 97";
                MainControlBuffer.clear();
                return;
            }

            int index = MainControlBuffer.indexOf("IALARM:");
            if(index == -1){
                return;
            }

            int MsgLength = MainControlBuffer.mid(index + 7, 13).toUInt();
            if(MainControlBuffer.size() - 20 >= MsgLength) {
                FullPackage = MainControlBuffer.mid(20, MsgLength);
                MainControlBuffer = MainControlBuffer.mid(20 + MsgLength);
            }else{
                return;
            }

            if(!dom.setContent(FullPackage, &errorMsg, &errorLine, &errorColumn)) {
                qDebug() << TIMEMS << "Parse error at line " << errorLine <<","<< "column " << errorColumn << ","<< errorMsg;
                return;
            }

            qDebug() << "slotRecvMainControlMsg" << FullPackage;

            QDomElement RootElement = dom.documentElement();//获取根元素
            if(RootElement.tagName() == "MainControlDevice"){ //根元素名称
                QDomNode firstChildNode = RootElement.firstChild();//第一个子节点
                while(!firstChildNode.isNull()){
                    if(firstChildNode.nodeName() == "SetPWM"){
                        isSetPWM = true;
                        QDomElement firstChildElement = firstChildNode.toElement();
                        PWM = firstChildElement.text().toUInt();
                    }

                    if(firstChildNode.nodeName() == "GetSubStream"){
                        isGetSubStream = true;
                    }

                    if(firstChildNode.nodeName() == "SubStream"){
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString SubStreamBasicPoint = firstChildElement.text();
                        SubStreamBasicPoint = SubStreamBasicPoint.split(",").join(".");
                        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/SubStreamBasicPoint",SubStreamBasicPoint);

                        GlobalConfig::SubStreamFactor =
                                CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/SubStreamBasicPoint").split("#").at(0).toInt();
                        GlobalConfig::SubStreamSelectRect =
                                GlobalConfig::GetSubStreamSelectRect();
                        GlobalConfig::SubStreamLightPoint =
                                GlobalConfig::GetSubStreamLightPoint();

                        if(!SubStreamVideoName.isEmpty()){
                            SubWorkThread->factor = GlobalConfig::SubStreamFactor;
                            SubWorkThread->SelectRect = GlobalConfig::SubStreamSelectRect;
                            SubWorkThread->LightPoint = GlobalConfig::SubStreamLightPoint;
                        }
                    }

                    if(firstChildNode.nodeName() == "ServerIP"){
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString ServerIP = firstChildElement.text();
                        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/ServerIP",ServerIP);
                        GlobalConfig::ServerIP =
                                CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/ServerIP");
                    }

                    if(firstChildNode.nodeName() == "MainIP"){
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString MainIP = firstChildElement.text();
                        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/MainIP",MainIP);
                        GlobalConfig::MainIP =
                                CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/MainIP");
                    }

                    if(firstChildNode.nodeName() == "SubDefenceID"){
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString SubDefenceID = firstChildElement.text();
                        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/SubDefenceID",SubDefenceID);
                        GlobalConfig::SubDefenceID =
                                CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/SubDefenceID");
                    }

                    firstChildNode = firstChildNode.nextSibling();//下一个节点
                }

                if(isSetPWM){
                    //添加根据pwm来设置辅助控制杆灯珠的亮度
                    operate_serial->SetPWM(PWM,0x01);
                }

                //本设备返回辅助控制杆基准图片给报警主机
                if(isGetSubStream){
                    QString MessageMerge;
                    QDomDocument AckDom;

                    //xml声明
                    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                    AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                    //创建根元素
                    QDomElement RootElement = AckDom.createElement("Device");
                    RootElement.setAttribute("DeviceIP",GlobalConfig::MainIP);
                    RootElement.setAttribute("DefenceID",GlobalConfig::SubDefenceID);
                    AckDom.appendChild(RootElement);

                    //创建MainStream元素
                    QDomElement MainStreamElement = AckDom.createElement("MainStream");
                    QDomText MainStreamElementText = AckDom.createTextNode("");
                    MainStreamElement.appendChild(MainStreamElementText);
                    RootElement.appendChild(MainStreamElement);

                    //创建SubStream元素
                    QDomElement SubStreamElement = AckDom.createElement("SubStream");
                    QString SubStreamBase64 = QString("");

                    if(!SubStreamVideoName.isEmpty()){
                        QImage image_rgb888;

                        if (SubWorkThread->operatecamera->ReadFrame()){
                            if(SubWorkThread->YUYVToRGB24_FFmpeg(SubWorkThread->operatecamera->yuyv_buff, SubWorkThread->rgb_buff)){
                                image_rgb888 = QImage((quint8 *)SubWorkThread->rgb_buff, SubWorkThread->width, SubWorkThread->height, QImage::Format_RGB888);
                            }
                        }else{
                            image_rgb888 = QImage();
                        }

                        if(!image_rgb888.isNull()){
                            QByteArray data;
                            QBuffer buffer(&data);
                            image_rgb888.save(&buffer,"JPG");
                            SubStreamBase64 = data.toBase64();//base64的图片数据
                        }else{
                            SubStreamBase64 = QString("辅助控制杆摄像头获取图片失败");
                        }
                    }else{
                        SubStreamBase64 = QString("本设备没有辅助控制杆，不能获取子码流");
                    }

                    QDomText SubStreamElementText = AckDom.createTextNode(SubStreamBase64);
                    SubStreamElement.appendChild(SubStreamElementText);
                    RootElement.appendChild(SubStreamElement);

                    QTextStream Out(&MessageMerge);
                    AckDom.save(Out,4);

                    int length = MessageMerge.size();
                    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                    foreach (QTcpSocket *AlarmHostConfigSocket, tcpClientsUnique) {
                        SendAlarmHostConfigData(MessageMerge,AlarmHostConfigSocket);
                    }
                    qDebug() << TIMEMS << "Return SubStream Basic Pic";
                }
            }
        }
    }
}

void TcpServer::slotMainControlDisconnect()
{
    qDebug() << TIMEMS << "MainControl Disconnect" << "\tIP =" << MainControlSocket->peerAddress().toString() << "\tPort =" << MainControlSocket->peerPort();
    MainControlSocket = NULL;
}

void TcpServer::slotMainAlarmImage()
{
    qDebug() << TIMEMS << "MainStream Alarm";

    QString MessageMerge;
    QDomDocument AckDom;

    //xml声明
    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
    AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

    //创建根元素
    QDomElement RootElement = AckDom.createElement("Device");
    RootElement.setAttribute("DeviceIP",GlobalConfig::LocalHostIP);
    RootElement.setAttribute("DefenceID",GlobalConfig::MainDefenceID);
    AckDom.appendChild(RootElement);

    //创建DeviceAlarmImage元素
    QDomElement DeviceAlarmImageElement = AckDom.createElement("DeviceAlarmImage");
    DeviceAlarmImageElement.setAttribute("id","0");

    QByteArray data;
    QBuffer buffer(&data);
    MainWorkThread->AlarmImage.save(&buffer,"JPG");
    QString AlarmImageBase64 = data.toBase64();//base64的图片数据

    QDomText DeviceAlarmImageElementText = AckDom.createTextNode(AlarmImageBase64);
    DeviceAlarmImageElement.appendChild(DeviceAlarmImageElementText);
    RootElement.appendChild(DeviceAlarmImageElement);

    QTextStream Out(&MessageMerge);
    AckDom.save(Out,4);

    int length = MessageMerge.size();
    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

    AlarmMsgBuffer.append(MessageMerge);
}

void TcpServer::slotSubAlarmImage()
{
    qDebug() << TIMEMS << "SubStream Alarm";

    QString MessageMerge;
    QDomDocument AckDom;

    //xml声明
    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
    AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

    //创建根元素
    QDomElement RootElement = AckDom.createElement("Device");
    RootElement.setAttribute("DeviceIP",GlobalConfig::MainIP);
    RootElement.setAttribute("DefenceID",GlobalConfig::SubDefenceID);
    AckDom.appendChild(RootElement);

    //创建DeviceAlarmImage元素
    QDomElement DeviceAlarmImageElement = AckDom.createElement("DeviceAlarmImage");
    DeviceAlarmImageElement.setAttribute("id","1");

    QByteArray data;
    QBuffer buffer(&data);
    SubWorkThread->AlarmImage.save(&buffer,"JPG");
    QString AlarmImageBase64 = data.toBase64();//base64的图片数据

    QDomText DeviceAlarmImageElementText = AckDom.createTextNode(AlarmImageBase64);
    DeviceAlarmImageElement.appendChild(DeviceAlarmImageElementText);
    RootElement.appendChild(DeviceAlarmImageElement);

    QTextStream Out(&MessageMerge);
    AckDom.save(Out,4);

    int length = MessageMerge.size();
    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

    AlarmMsgBuffer.append(MessageMerge);
}

void TcpServer::slotOtherAlarmMsg(quint8 id)
{
    qDebug() << TIMEMS << "Other Alarm";

    QString MessageMerge;
    QDomDocument AckDom;

    //xml声明
    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
    AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

    //创建根元素
    QDomElement RootElement = AckDom.createElement("Device");
    RootElement.setAttribute("DeviceIP",GlobalConfig::LocalHostIP);
    RootElement.setAttribute("DefenceID",GlobalConfig::MainDefenceID);
    AckDom.appendChild(RootElement);

    //创建Alarm元素
    QDomElement AlarmElement = AckDom.createElement("DeviceAlarm");
    AlarmElement.setAttribute("id",QString::number(id));
    RootElement.appendChild(AlarmElement);

    QTextStream Out(&MessageMerge);
    AckDom.save(Out,4);

    int length = MessageMerge.size();
    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

    AlarmMsgBuffer.append(MessageMerge);
}

void TcpServer::slotSendAlarmMsg()
{
    if(AlarmMsgBuffer.count() > 0){
        QString XmlData = AlarmMsgBuffer.takeFirst();

        QStringList listServerIP   = GlobalConfig::ServerIP.split("|");
        QStringList listServerPort = GlobalConfig::ServerPort.split("|");

        int count = listServerIP.count();
        for (int i = 0; i < count; i++) {
            QString serverIP = listServerIP.at(i);
            int serverPort = listServerPort.at(i).toInt();

            QTcpSocket *tcpSocket = new QTcpSocket(this);
            connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(slotAlarmHostAlarmDisplayError(QAbstractSocket::SocketError)));
            connect(tcpSocket, SIGNAL(disconnected()), this, SLOT(slotAlarmHostAlarmDisConnected()));

            tcpSocket->connectToHost(serverIP, serverPort);
            bool isConnect = tcpSocket->waitForConnected(GlobalConfig::TcpConnectTimeout);
            if (isConnect) {
                tcpSocket->write(XmlData.toLatin1());
                tcpServers.append(tcpSocket);
                qDebug() << TIMEMS << "Connect Serve succeed" << "IP = " << serverIP << "Port = " << serverPort;
            } else {
                tcpSocket->close();
                tcpSocket->deleteLater();
                qDebug() << TIMEMS << "Connect Server Error" << "IP = " << serverIP << "Port = " << serverPort;
            }
        }
    }
}

void TcpServer::slotAlarmHostAlarmDisConnected()
{
    QTcpSocket *tcpSocket = (QTcpSocket *)sender();
    QString ip = tcpSocket->peerAddress().toString();
    int port = tcpSocket->peerPort();
    qDebug() << TIMEMS << "Server DisConnect" << "IP = " << ip << "Port = " << port;

    int count = tcpServers.count();
    for (int i = 0; i < count; i++) {
        if (tcpServers.at(i) == tcpSocket) {
            tcpSocket->deleteLater();
            tcpServers.removeAt(i);
            break;
        }
    }
}

void TcpServer::slotAlarmHostAlarmDisplayError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket *tcpSocket = (QTcpSocket *)sender();
    QString ip = tcpSocket->peerAddress().toString();
    int port = tcpSocket->peerPort();

    switch(socketError){
    case QAbstractSocket::ConnectionRefusedError:
        qDebug() << TIMEMS << "Server ConnectionRefusedError" << "IP = " << ip << "Port = " << port;
        break;
    case QAbstractSocket::RemoteHostClosedError:
        qDebug() << TIMEMS  << "Server RemoteHostClosedError" << "IP = " << ip << "Port = " << port;
        break;
    case QAbstractSocket::HostNotFoundError:
        qDebug() << TIMEMS  << "Server HostNotFoundError" << "IP = " << ip << "Port = " << port;
        break;
    default:
        qDebug() << TIMEMS  << "Server " + tcpSocket->errorString() << "IP = " << ip << "Port = " << port;
        break;
    }

    tcpSocket->close();
    tcpSocket->disconnectFromHost();
}

void TcpServer::slotRecvSubControlMsg()
{
    QDomDocument dom;
    QString errorMsg;
    int errorLine, errorColumn;
    bool isSetPWM = false;//添加根据pwm来设置左边主控制杆灯珠的亮度
    quint8 PWM;

    if(SubControlSocket->isOpen() && SubControlSocket->isReadable()) {
        SubControlBuffer.append(SubControlSocket->readAll());
        QByteArray FullPackage;

        if(SubControlBuffer.size() <= 57){
            qDebug() << TIMEMS << "SubControlBuffer.size() <= 57";
            SubControlBuffer.clear();
            return;
        }

        int index = SubControlBuffer.indexOf("IALARM:");
        if(index == -1){
            return;
        }

        int MsgLength = SubControlBuffer.mid(index + 7, 13).toUInt();
        if(SubControlBuffer.size() - 20 >= MsgLength) {
            FullPackage = SubControlBuffer.mid(20, MsgLength);
            SubControlBuffer = SubControlBuffer.mid(20 + MsgLength);
        }else{
            return;
        }

        if(!dom.setContent(FullPackage, &errorMsg, &errorLine, &errorColumn)) {
            qDebug() << TIMEMS << "Parse error at line " << errorLine <<","<< "column " << errorColumn << ","<< errorMsg;
            return;
        }

        qDebug() << "slotRecvSubControlMsg" << FullPackage;

        QDomElement RootElement = dom.documentElement();//获取根元素
        if(RootElement.tagName() == "SubControlDevice"){ //根元素名称
            QDomNode firstChildNode = RootElement.firstChild();//第一个子节点
            while(!firstChildNode.isNull()){
                if(firstChildNode.nodeName() == "SetPWM"){
                    isSetPWM = true;
                    QDomElement firstChildElement = firstChildNode.toElement();
                    PWM = firstChildElement.text().toUInt();
                }

                firstChildNode = firstChildNode.nextSibling();//下一个节点
            }

            if(isSetPWM){
                //添加根据pwm来设置左边主控制杆灯珠的亮度
                operate_serial->SetPWM(PWM,0x00);
            }
        }
    }
}

void TcpServer::slotSubControlConnected()
{
    qDebug() << TIMEMS << "connect to sub control succeed";
    SubControlConnectStateFlag = TcpServer::ConnectedState;

    //停止重连右边的辅助控制杆定时器
    ReConnectSubControlTimer->stop();

    //开启探测主控制杆与辅助控制杆tcp长连接的状态
    enableKeepAlive(SubControlSocket->socketDescriptor(),1,1,1);
}

void TcpServer::slotSubControlDisConnected()
{
    qDebug() << TIMEMS << "sub control close connection";
    SubControlConnectStateFlag = TcpServer::DisConnectedState;
    SubControlSocket->abort();
}

void TcpServer::slotSubControlDisplayError(QAbstractSocket::SocketError socketError)
{
    switch(socketError){
    case QAbstractSocket::ConnectionRefusedError:
        qDebug() << TIMEMS << "SubControl:QAbstractSocket::ConnectionRefusedError";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        qDebug() << TIMEMS << "SubControl:QAbstractSocket::RemoteHostClosedError";
        break;
    case QAbstractSocket::HostNotFoundError:
        qDebug() << TIMEMS << "SubControl:QAbstractSocket::HostNotFoundError";
        break;
    default:
        qDebug() << TIMEMS << "SubControl:" + SubControlSocket->errorString();
        break;
    }

    SubControlConnectStateFlag = TcpServer::DisConnectedState;
    SubControlSocket->abort();

    //启动重连右边的辅助控制杆定时器
    ReConnectSubControlTimer->start();
}

void TcpServer::slotProcessAlarmHostConfigMsg()
{
    if(AlarmHostConfigBuffer.size() == 0){
        return;
    }

    QDomDocument dom;
    QString errorMsg;
    int errorLine, errorColumn;
    bool isBasicConfig = false;
    bool isSystemReboot = false;
    bool isReturnOK = false;//本设备返回OK消息给报警主机
    bool isGetBasicPic = false;//本设备返回主控制杆图片和辅助控制杆图片给报警主机
    bool isGetMainStream = false;//本设备返回主控制杆图片给报警主机
    bool isSubStream = false;//本设备将辅助控制杆基准点配置信息发送给右边的辅助控制杆
    bool isSetSubStreamPWM = false;//本设备将报警主机调节辅助控制杆灯柱亮度的PWM发送给辅助控制杆
    bool isGetDeviceConfig = false;//本设备返回网络配置信息给报警主机

    QString SubStreamBasicPoint;//辅助控制杆基准点配置信息
    quint8 PWM;

    QString FullPackage = AlarmHostConfigBuffer.takeFirst();
    QTcpSocket *AlarmHostConfigSocket = tcpClients.takeFirst();

    FullPackage = FullPackage.mid(20);

    if(!dom.setContent(FullPackage, &errorMsg, &errorLine, &errorColumn)) {
        qDebug() << TIMEMS << "Parse error at line " << errorLine <<","<< "column " << errorColumn << ","<< errorMsg;
        return;
    }

    qDebug() << TIMEMS << FullPackage;

    QDomElement RootElement = dom.documentElement();//获取根元素
    if(RootElement.tagName() == "Server"){ //根元素名称
        //判断根元素是否有这个属性
        if(RootElement.hasAttribute("TargetIP")){
            //获得这个属性对应的值
            QString TargetIP = RootElement.attributeNode("TargetIP").value();

            if(TargetIP == GlobalConfig::LocalHostIP){
                //判断根元素是否有这个属性
                if(RootElement.hasAttribute("NowTime")){
                    //获得这个属性对应的值
                    QString NowTime = RootElement.attributeNode("NowTime").value();
                    SystemTime = NowTime;
                }

                QDomNode firstChildNode = RootElement.firstChild();//第一个子节点
                while(!firstChildNode.isNull()){
                    if(firstChildNode.nodeName() == "StartServices"){
                        isReturnOK = true;
                        if(!MainStreamVideoName.isEmpty()){
                            MainWorkThread->isStopCapture = false;
                        }

                        if(!SubStreamVideoName.isEmpty()){
                            SubWorkThread->isStopCapture = false;
                        }
                    }

                    if(firstChildNode.nodeName() == "StopServices"){
                        isReturnOK = true;
                        if(!MainStreamVideoName.isEmpty()){
                            MainWorkThread->isStopCapture = true;
                        }

                        if(!SubStreamVideoName.isEmpty()){
                            SubWorkThread->isStopCapture = true;
                        }
                    }

                    if(firstChildNode.nodeName() == "ServerIP"){
                        isBasicConfig = true;
                        isReturnOK = true;
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString ServerIP = firstChildElement.text();
                        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/ServerIP",ServerIP);
                        GlobalConfig::ServerIP = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/ServerIP");
                    }

                    if(firstChildNode.nodeName() == "ServerPort"){
                        isBasicConfig = true;
                        isReturnOK = true;
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString ServerPort = firstChildElement.text();
                        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/ServerPort",ServerPort);
                        GlobalConfig::ServerPort = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/ServerPort");
                    }

                    if(firstChildNode.nodeName() == "SubIP"){
                        isBasicConfig = true;
                        isReturnOK = true;
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString SubIP = firstChildElement.text();
                        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/SubIP",SubIP);
                        GlobalConfig::SubIP = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/SubIP");
                    }

                    if(firstChildNode.nodeName() == "DefenceID"){
                        isBasicConfig = true;
                        isReturnOK = true;
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString DefenceID = firstChildElement.text();
                        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/MainDefenceID",DefenceID);
                        GlobalConfig::MainDefenceID = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/MainDefenceID");
                    }


                    if(firstChildNode.nodeName() == "CameraSleepTime"){
                        isBasicConfig = true;
                        isReturnOK = true;
                        //采集间隔（单位毫秒）
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString CameraSleepTime = firstChildElement.text();
                        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/CameraSleepTime",CameraSleepTime);
                        GlobalConfig::CameraSleepTime = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/CameraSleepTime").toInt();
                    }

                    if(firstChildNode.nodeName() == "DeviceMacAddr"){
                        isBasicConfig = true;
                        isReturnOK = true;
                        //设备MAC地址（同一局域网内设备不能有重复的MAC地址）
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString DeviceMacAddr = firstChildElement.text().toUpper();
                        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/DeviceMacAddr",DeviceMacAddr);
                        GlobalConfig::DeviceMacAddr = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/DeviceMacAddr");
                    }

                    if(firstChildNode.nodeName() == "DeviceIPAddrPrefix"){
                        isBasicConfig = true;
                        isReturnOK = true;
                        //设备网段 默认192.168.8
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString DeviceIPAddrPrefix = firstChildElement.text();
                        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/DeviceIPAddrPrefix",DeviceIPAddrPrefix);
                        GlobalConfig::DeviceIPAddrPrefix = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/DeviceIPAddrPrefix");
                    }

                    if(firstChildNode.nodeName() == "GetBasicPic"){
                        isGetBasicPic = true;
                    }

                    if(firstChildNode.nodeName() == "MainStream"){
                        //主控制杆基准点保存到本地
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString MainStreamBasicPoint = firstChildElement.text();
                        MainStreamBasicPoint = MainStreamBasicPoint.split(",").join(".");
                        CommonSetting::WriteSettings(GlobalConfig::ConfigFileName,"AppConfig/MainStreamBasicPoint",MainStreamBasicPoint);

                        GlobalConfig::MainStreamFactor = CommonSetting::ReadSettings(GlobalConfig::ConfigFileName,"AppConfig/MainStreamBasicPoint").split("#").at(0).toInt();
                        GlobalConfig::MainStreamSelectRect = GlobalConfig::GetMainStreamSelectRect();
                        GlobalConfig::MainStreamLightPoint = GlobalConfig::GetMainStreamLightPoint();

                        if(!MainStreamVideoName.isEmpty()){
                            MainWorkThread->factor = GlobalConfig::MainStreamFactor;
                            MainWorkThread->SelectRect = GlobalConfig::MainStreamSelectRect;
                            MainWorkThread->LightPoint = GlobalConfig::MainStreamLightPoint;
                        }
                    }

                    if(firstChildNode.nodeName() == "SubStream"){
                        //辅助基准点配置信息发送给右边的辅助控制杆
                        isSubStream = true;
                        QDomElement firstChildElement = firstChildNode.toElement();
                        SubStreamBasicPoint = firstChildElement.text();
                    }

                    if(firstChildNode.nodeName() == "GetMainStream"){
                        isGetMainStream = true;
                    }

                    if(firstChildNode.nodeName() == "SetPWM"){
                        isReturnOK = true;
                        //判断元素是否有这个属性
                        QDomElement firstChildElement = firstChildNode.toElement();
                        if(firstChildElement.hasAttribute("id")){
                            //获得这个属性对应的值
                            quint8 id = firstChildElement.attributeNode("id").value().toUInt();
                            PWM = firstChildElement.text().toUInt();
                            //添加根据id和pwm来设置控制杆灯珠的亮度
                            if(id == 0){
                                operate_serial->SetPWM(PWM,id);
                            }else if(id == 1){
                                isSetSubStreamPWM = true;
                            }
                        }
                    }

                    if(firstChildNode.nodeName() == "GetDeviceConfig") {
                        isGetDeviceConfig = true;
                    }

                    if(firstChildNode.nodeName() == "SystemReboot"){
                        isReturnOK = true;
                        isSystemReboot = true;
                    }

                    firstChildNode = firstChildNode.nextSibling();//下一个节点
                }

                if(isBasicConfig){
                    QString MessageMerge;
                    QDomDocument AckDom;

                    //xml声明
                    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                    AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                    //创建根元素
                    QDomElement RootElement = AckDom.createElement("MainControlDevice");
                    AckDom.appendChild(RootElement);

                    //创建ServerIP元素
                    QDomElement ServerIPElement = AckDom.createElement("ServerIP");
                    QDomText ServerIPElementText =
                            AckDom.createTextNode(GlobalConfig::ServerIP);
                    ServerIPElement.appendChild(ServerIPElementText);
                    RootElement.appendChild(ServerIPElement);

                    //创建MainIP元素
                    QDomElement MainIPElement = AckDom.createElement("MainIP");
                    QDomText MainIPElementText =
                            AckDom.createTextNode(GlobalConfig::LocalHostIP);
                    MainIPElement.appendChild(MainIPElementText);
                    RootElement.appendChild(MainIPElement);

                    //创建SubDefenceID元素
                    QDomElement SubDefenceIDElement = AckDom.createElement("SubDefenceID");
                    QDomText SubDefenceIDElementText =
                            AckDom.createTextNode(GlobalConfig::MainDefenceID);
                    SubDefenceIDElement.appendChild(SubDefenceIDElementText);
                    RootElement.appendChild(SubDefenceIDElement);

                    QTextStream Out(&MessageMerge);
                    AckDom.save(Out,4);

                    int length = MessageMerge.size();
                    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                    WriteData(MessageMerge);

                    CommonSetting::Sleep(500);
                }

                if(isGetBasicPic){
                    if(MainStreamVideoName.isEmpty()){
                        //没有主控制杆,那就没有对应的辅助控制杆，所以不能获取基准图片
                        QString MessageMerge;
                        QDomDocument AckDom;

                        //xml声明
                        QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                        AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                        //创建根元素
                        QDomElement RootElement = AckDom.createElement("Device");

                        RootElement.setAttribute("DeviceIP",GlobalConfig::LocalHostIP);
                        RootElement.setAttribute("DefenceID",GlobalConfig::MainDefenceID);
                        AckDom.appendChild(RootElement);

                        //创建DeviceError
                        QDomElement DeviceErrorElement = AckDom.createElement("DeviceError");
                        QDomText DeviceErrorText = AckDom.createTextNode("非法配置,本设备只有辅助控制杆,没有主控制杆，不能获取基准图片");
                        DeviceErrorElement.appendChild(DeviceErrorText);
                        RootElement.appendChild(DeviceErrorElement);


                        QTextStream Out(&MessageMerge);
                        AckDom.save(Out,4);

                        int length = MessageMerge.size();
                        MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                        SendAlarmHostConfigData(MessageMerge,AlarmHostConfigSocket);
                        qDebug() << TIMEMS << "DeviceError";
                    }else{
                        //有主控制杆和相应的辅助控制杆，能获取基准图片
                        //1.发送GetSubStream包给辅助控制杆
                        {
                            QString MessageMerge;
                            QDomDocument AckDom;

                            //xml声明
                            QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                            AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                            //创建根元素
                            QDomElement RootElement =
                                    AckDom.createElement("MainControlDevice");
                            AckDom.appendChild(RootElement);

                            //创建GetSubStream元素
                            QDomElement GetSubStreamElement =
                                    AckDom.createElement("GetSubStream");
                            RootElement.appendChild(GetSubStreamElement);

                            QTextStream Out(&MessageMerge);
                            AckDom.save(Out,4);

                            int length = MessageMerge.size();
                            MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                            WriteData(MessageMerge);
                        }

                        //2.获取主控制杆图片，并且发送给报警主机
                        {
                            QString MessageMerge;
                            QDomDocument AckDom;

                            //xml声明
                            QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                            AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                            //创建根元素
                            QDomElement RootElement = AckDom.createElement("Device");
                            RootElement.setAttribute("DeviceIP",GlobalConfig::LocalHostIP);
                            RootElement.setAttribute("DefenceID",GlobalConfig::MainDefenceID);
                            AckDom.appendChild(RootElement);

                            //创建MainStream元素
                            QDomElement MainStreamElement =
                                    AckDom.createElement("MainStream");
                            QString MainStreamBase64 = QString("");

                            if(!MainStreamVideoName.isEmpty()){
                                QImage image_rgb888;

                                if (MainWorkThread->operatecamera->ReadFrame()){
                                    if(MainWorkThread->YUYVToRGB24_FFmpeg(MainWorkThread->operatecamera->yuyv_buff, MainWorkThread->rgb_buff)){
                                        image_rgb888 = QImage((quint8 *)MainWorkThread->rgb_buff, MainWorkThread->width, MainWorkThread->height, QImage::Format_RGB888);
                                    }
                                }else{
                                    image_rgb888 = QImage();
                                }

                                if(!image_rgb888.isNull()){
                                    QByteArray data;
                                    QBuffer buffer(&data);
                                    image_rgb888.save(&buffer,"JPG");
                                    MainStreamBase64 = data.toBase64();//base64的图片数据
                                }else{
                                    MainStreamBase64 = QString("主控制杆摄像头获取图片失败");
                                }
                            }else{
                                MainStreamBase64 = QString("本设备没有主控制杆，不能获取主码流");
                            }

                            QDomText MainStreamElementText =
                                    AckDom.createTextNode(MainStreamBase64);
                            MainStreamElement.appendChild(MainStreamElementText);
                            RootElement.appendChild(MainStreamElement);

                            //创建SubStream元素
                            QDomElement SubStreamElement =
                                    AckDom.createElement("SubStream");
                            QDomText SubStreamElementText = AckDom.createTextNode("");
                            SubStreamElement.appendChild(SubStreamElementText);
                            RootElement.appendChild(SubStreamElement);

                            QTextStream Out(&MessageMerge);
                            AckDom.save(Out,4);

                            int length = MessageMerge.size();
                            MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                            SendAlarmHostConfigData(MessageMerge,AlarmHostConfigSocket);
                            qDebug() << TIMEMS << "Return MainStream Basic Pic";
                        }
                    }
                }

                if(isGetMainStream){
                    if(!MainStreamVideoName.isEmpty()){
                        QString MessageMerge;
                        QDomDocument AckDom;

                        //xml声明
                        QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                        AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                        //创建根元素
                        QDomElement RootElement = AckDom.createElement("Device");
                        RootElement.setAttribute("DeviceIP",GlobalConfig::LocalHostIP);
                        RootElement.setAttribute("DefenceID",GlobalConfig::MainDefenceID);
                        AckDom.appendChild(RootElement);

                        //创建MainStream元素
                        QDomElement MainStreamElement = AckDom.createElement("MainStream");
                        QString MainStreamBase64;

                        QByteArray data;
                        QBuffer buffer(&data);

                        if (!MainStreamVideoName.isEmpty()) {
                            MainWorkThread->NormalImage.save(&buffer,"JPG");
                            MainStreamBase64 = data.toBase64();//base64的图片数据
                        } else {
                            MainStreamBase64 = QString("");
                        }


                        QDomText MainStreamElementText = AckDom.createTextNode(MainStreamBase64);
                        MainStreamElement.appendChild(MainStreamElementText);
                        RootElement.appendChild(MainStreamElement);

                        QTextStream Out(&MessageMerge);
                        AckDom.save(Out,4);

                        int length = MessageMerge.size();
                        MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                        SendAlarmHostConfigData(MessageMerge,AlarmHostConfigSocket);
                        qDebug() << TIMEMS << "GetMainStream";

//                        MainWorkThread->NormalImage = QImage();
                    }
                }

                //1、返回OK给报警主机
                //2、将辅助控制杆基准点配置信息发送给右边的辅助控制杆
                if(isSubStream){
                    //1、返回OK给报警主机
                    {
                        QString MessageMerge;
                        QDomDocument AckDom;

                        //xml声明
                        QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                        AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                        //创建根元素
                        QDomElement RootElement = AckDom.createElement("Device");

                        RootElement.setAttribute("DeviceIP",GlobalConfig::LocalHostIP);
                        RootElement.setAttribute("DefenceID",GlobalConfig::MainDefenceID);

                        QDomText RootElementText = AckDom.createTextNode("OK");
                        RootElement.appendChild(RootElementText);

                        AckDom.appendChild(RootElement);

                        QTextStream Out(&MessageMerge);
                        AckDom.save(Out,4);

                        int length = MessageMerge.size();
                        MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;


                        SendAlarmHostConfigData(MessageMerge,AlarmHostConfigSocket);
                        qDebug() << TIMEMS << MessageMerge;
                    }

                    //2、将辅助控制杆基准点配置信息发送给右边的辅助控制杆
                    {
                        QString MessageMerge;
                        QDomDocument AckDom;

                        //xml声明
                        QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                        AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                        //创建根元素
                        QDomElement RootElement = AckDom.createElement("MainControlDevice");
                        AckDom.appendChild(RootElement);

                        //创建SubStream元素
                        QDomElement SubStreamElement = AckDom.createElement("SubStream");
                        QDomText SubStreamElementText =
                                AckDom.createTextNode(SubStreamBasicPoint);
                        SubStreamElement.appendChild(SubStreamElementText);
                        RootElement.appendChild(SubStreamElement);

                        QTextStream Out(&MessageMerge);
                        AckDom.save(Out,4);

                        int length = MessageMerge.size();
                        MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                        WriteData(MessageMerge);
                    }
                }

                if(isSetSubStreamPWM){
                    QString MessageMerge;
                    QDomDocument AckDom;

                    //xml声明
                    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                    AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                    //创建根元素
                    QDomElement RootElement = AckDom.createElement("MainControlDevice");
                    AckDom.appendChild(RootElement);

                    //创建SetPWM元素
                    QDomElement SetPWMElement = AckDom.createElement("SetPWM");
                    QDomText SetPWMElementText = AckDom.createTextNode(QString::number(PWM));
                    SetPWMElement.appendChild(SetPWMElementText);
                    RootElement.appendChild(SetPWMElement);

                    QTextStream Out(&MessageMerge);
                    AckDom.save(Out,4);

                    int length = MessageMerge.size();
                    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                    WriteData(MessageMerge);
                }

                if(isGetDeviceConfig){
                    QString MessageMerge;
                    QDomDocument AckDom;

                    //xml声明
                    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                    AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                    //创建根元素
                    QDomElement RootElement = AckDom.createElement("Device");

                    RootElement.setAttribute("DeviceIP",GlobalConfig::LocalHostIP);
                    RootElement.setAttribute("DefenceID",GlobalConfig::MainDefenceID);
                    AckDom.appendChild(RootElement);

                    //创建ServerIP元素
                    QDomElement ServerIP = AckDom.createElement("ServerIP");
                    QDomText ServerIPText = AckDom.createTextNode(GlobalConfig::ServerIP);
                    ServerIP.appendChild(ServerIPText);
                    RootElement.appendChild(ServerIP);

                    //创建ServerPort元素
                    QDomElement ServerPort = AckDom.createElement("ServerPort");
                    QDomText ServerPortText =
                            AckDom.createTextNode(GlobalConfig::ServerPort);
                    ServerPort.appendChild(ServerPortText);
                    RootElement.appendChild(ServerPort);

                    //创建SubIP元素
                    QDomElement SubIP = AckDom.createElement("SubIP");
                    QDomText SubIPText = AckDom.createTextNode(GlobalConfig::SubIP);
                    SubIP.appendChild(SubIPText);
                    RootElement.appendChild(SubIP);

                    //创建DefenceID元素
                    QDomElement DefenceID = AckDom.createElement("DefenceID");
                    QDomText DefenceIDText = AckDom.createTextNode(GlobalConfig::MainDefenceID);
                    DefenceID.appendChild(DefenceIDText);
                    RootElement.appendChild(DefenceID);

                    //创建CameraSleepTime元素
                    QDomElement CameraSleepTime = AckDom.createElement("CameraSleepTime");
                    QDomText CameraSleepTimeText =
                           AckDom.createTextNode(QString::number(GlobalConfig::CameraSleepTime));
                    CameraSleepTime.appendChild(CameraSleepTimeText);
                    RootElement.appendChild(CameraSleepTime);

                    //创建DeviceMacAddr元素
                    QDomElement DeviceMacAddr = AckDom.createElement("DeviceMacAddr");
                    QDomText DeviceMacAddrText =
                            AckDom.createTextNode(GlobalConfig::DeviceMacAddr);
                    DeviceMacAddr.appendChild(DeviceMacAddrText);
                    RootElement.appendChild(DeviceMacAddr);

                    //创建DeviceIPAddrPrefix元素
                    QDomElement DeviceIPAddrPrefix = AckDom.createElement("DeviceIPAddrPrefix");
                    QDomText DeviceIPAddrPrefixText =
                            AckDom.createTextNode(GlobalConfig::DeviceIPAddrPrefix);
                    DeviceIPAddrPrefix.appendChild(DeviceIPAddrPrefixText);
                    RootElement.appendChild(DeviceIPAddrPrefix);

                    QTextStream Out(&MessageMerge);
                    AckDom.save(Out,4);

                    int length = MessageMerge.size();
                    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                    SendAlarmHostConfigData(MessageMerge,AlarmHostConfigSocket);
                    qDebug() << TIMEMS << "GetDeviceConfig";
                }

                if(isReturnOK){
                    QString MessageMerge;
                    QDomDocument AckDom;

                    //xml声明
                    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                    AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                    //创建根元素
                    QDomElement RootElement = AckDom.createElement("Device");

                    RootElement.setAttribute("DeviceIP",GlobalConfig::LocalHostIP);
                    RootElement.setAttribute("DefenceID",GlobalConfig::MainDefenceID);

                    QDomText RootElementText = AckDom.createTextNode("OK");
                    RootElement.appendChild(RootElementText);

                    AckDom.appendChild(RootElement);

                    QTextStream Out(&MessageMerge);
                    AckDom.save(Out,4);

                    int length = MessageMerge.size();
                    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                    SendAlarmHostConfigData(MessageMerge,AlarmHostConfigSocket);
                    qDebug() << TIMEMS << MessageMerge;
                    CommonSetting::Sleep(500);
                }

                if(isSystemReboot){
                    system("reboot");
                }
            }
        }
    }
}

void TcpServer::slotMainUSBCameraOffline()
{
    SendMainCameraStateInfo("主杆摄像头掉线");

    GetCameraInfo::Instance()->GetMainCameraDevice();

    MainWorkThread->operatecamera->VideoName = GetCameraInfo::MainCamera;
    MainWorkThread->VideoName = GetCameraInfo::MainCamera;
    MainWorkThread->isReInitialize = true;
    MainWorkThread->isStopCapture = false;
}

void TcpServer::slotMainUSBCameraOnline()
{
    SendMainCameraStateInfo("主杆摄像头恢复");
}

void TcpServer::slotSubUSBCameraOffline()
{
    SendSubCameraStateInfo("辅杆摄像头掉线");

    GetCameraInfo::Instance()->GetMainCameraDevice();

    SubWorkThread->operatecamera->VideoName = GetCameraInfo::SubCamera;
    SubWorkThread->VideoName = GetCameraInfo::SubCamera;
    SubWorkThread->isReInitialize = true;
    SubWorkThread->isStopCapture = false;
}

void TcpServer::slotSubUSBCameraOnline()
{
    SendSubCameraStateInfo("辅杆摄像头恢复");
}

void TcpServer::SendMainCameraStateInfo(QString info)
{
    qDebug() << TIMEMS << info;

    QString MessageMerge;
    QDomDocument AckDom;

    //xml声明
    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
    AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

    //创建根元素
    QDomElement RootElement = AckDom.createElement("Device");

    RootElement.setAttribute("DeviceIP",GlobalConfig::LocalHostIP);
    RootElement.setAttribute("DefenceID",GlobalConfig::MainDefenceID);
    AckDom.appendChild(RootElement);

    //创建DeviceError
    QDomElement DeviceErrorElement = AckDom.createElement("DeviceError");
    QDomText DeviceErrorText = AckDom.createTextNode(info);
    DeviceErrorElement.appendChild(DeviceErrorText);
    RootElement.appendChild(DeviceErrorElement);


    QTextStream Out(&MessageMerge);
    AckDom.save(Out,4);

    int length = MessageMerge.size();
    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

    AlarmMsgBuffer.append(MessageMerge);
}

void TcpServer::SendSubCameraStateInfo(QString info)
{
    qDebug() << TIMEMS << info;

    QString MessageMerge;
    QDomDocument AckDom;

    //xml声明
    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
    AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

    //创建根元素
    QDomElement RootElement = AckDom.createElement("Device");

    RootElement.setAttribute("DeviceIP",GlobalConfig::MainIP);
    RootElement.setAttribute("DefenceID",GlobalConfig::SubDefenceID);
    AckDom.appendChild(RootElement);

    //创建DeviceError
    QDomElement DeviceErrorElement = AckDom.createElement("DeviceError");
    QDomText DeviceErrorText = AckDom.createTextNode(info);
    DeviceErrorElement.appendChild(DeviceErrorText);
    RootElement.appendChild(DeviceErrorElement);


    QTextStream Out(&MessageMerge);
    AckDom.save(Out,4);

    int length = MessageMerge.size();
    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

    AlarmMsgBuffer.append(MessageMerge);
}

void TcpServer::slotSetSystemTime()
{
    CommonSetting::SettingSystemDateTime(SystemTime);
}

void TcpServer::enableKeepAlive(int fd, int max_idle, int keep_count, int keep_interval)
{
    int enableKeepAlive = 1;

    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enableKeepAlive, sizeof(enableKeepAlive));
    setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &max_idle, sizeof(max_idle));
    setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &keep_count, sizeof(keep_count));
    setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &keep_interval, sizeof(keep_interval));
}

void TcpServer::SendAlarmHostConfigData(QString Msg,QTcpSocket *AlarmHostConfigSocket)
{
    AlarmHostConfigSocket->write(Msg.toAscii());
    AlarmHostConfigSocket->waitForBytesWritten(100);
}

void TcpServer::WriteData(QString Msg)
{
    if(SubControlConnectStateFlag == TcpServer::ConnectedState){
        SubControlSocket->write(Msg.toAscii());
        SubControlSocket->waitForBytesWritten(100);
        qDebug() << TIMEMS << Msg;
    }else if(SubControlConnectStateFlag == TcpServer::DisConnectedState){
        SubControlSocket->connectToHost(GlobalConfig::SubIP,GlobalConfig::MainControlServerPort);
        bool state = SubControlSocket->waitForConnected(100);
        if(state){
            SubControlSocket->write(Msg.toAscii());
            SubControlSocket->waitForBytesWritten(100);
            qDebug() << TIMEMS << Msg;
        }
    }
}

void TcpServer::slotReConnectSubControl()
{
    SubControlSocket->abort();
    SubControlSocket->disconnectFromHost();
    SubControlSocket->connectToHost(GlobalConfig::SubIP,GlobalConfig::MainControlServerPort);
}

void TcpServer::slotCheckWorkThreadState()
{
    if (!MainStreamVideoName.isEmpty()) {
        if (!MainWorkThread->isRunning()){
            MainWorkThread->Clear();//释放资源
            delete MainWorkThread;

            isMainThreadExceptionQuit = true;
            SendMainCameraStateInfo("主线程异常退出");

            GetCameraInfo::Instance()->GetMainCameraDevice();//重新获取辅助控制杆摄像头设备文件

            MainWorkThread =
                    new WorkThread(GetCameraInfo::MainCamera,640,480,16,V4L2_PIX_FMT_YUYV,this);
            MainWorkThread->factor = GlobalConfig::MainStreamFactor;
            MainWorkThread->SelectRect = GlobalConfig::MainStreamSelectRect;
            MainWorkThread->LightPoint = GlobalConfig::MainStreamLightPoint;
            connect(MainWorkThread,SIGNAL(signalAlarmImage()),this, SLOT(slotMainAlarmImage()),Qt::BlockingQueuedConnection);
            connect(MainWorkThread,SIGNAL(signalUSBCameraOffline()),this,SLOT(slotMainUSBCameraOffline()),Qt::BlockingQueuedConnection);
            connect(MainWorkThread,SIGNAL(signalUSBCameraOnline()),this,SLOT(slotMainUSBCameraOnline()));
            MainWorkThread->start();

            CommonSetting::WriteCommonFile("/bin/SmartIR_Log.txt",TIMEMS + QString("MainWorkThread Restart"));
        }else{
            if(isMainThreadExceptionQuit){
                SendMainCameraStateInfo("主线程恢复正常");
                isMainThreadExceptionQuit = false;
            }
        }
    }

    if (!SubStreamVideoName.isEmpty()) {
        if (!SubWorkThread->isRunning()){
            SubWorkThread->Clear();//释放资源
            delete SubWorkThread;

            isSubThreadExceptionQuit = true;
            SendSubCameraStateInfo("辅线程异常退出");

            GetCameraInfo::Instance()->GetSubCameraDevice();//重新获取辅助控制杆摄像头设备文件

            SubWorkThread =
                    new WorkThread(GetCameraInfo::SubCamera,640,480,16,V4L2_PIX_FMT_YUYV,this);
            SubWorkThread->factor = GlobalConfig::SubStreamFactor;
            SubWorkThread->SelectRect = GlobalConfig::SubStreamSelectRect;
            SubWorkThread->LightPoint = GlobalConfig::SubStreamLightPoint;
            connect(SubWorkThread,SIGNAL(signalAlarmImage()),this, SLOT(slotSubAlarmImage()),Qt::BlockingQueuedConnection);
            connect(SubWorkThread,SIGNAL(signalUSBCameraOffline()),this,SLOT(slotSubUSBCameraOffline()),Qt::BlockingQueuedConnection);
            connect(SubWorkThread,SIGNAL(signalUSBCameraOnline()),this,SLOT(slotSubUSBCameraOnline()));
            SubWorkThread->start();

            CommonSetting::WriteCommonFile("/bin/SmartIR_Log.txt",TIMEMS + QString("SubWorkThread Restart"));
        }else{
            if(isSubThreadExceptionQuit){
                SendSubCameraStateInfo("辅线程恢复正常");
                isSubThreadExceptionQuit = false;
            }
        }
    }
}
