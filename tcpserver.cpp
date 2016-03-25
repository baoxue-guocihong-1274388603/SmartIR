#include "tcpserver.h"

TcpServer::TcpServer(QString MainVideoName, QString SubVideoName, QObject *parent) :
    QObject(parent)
{
    MainStreamVideoName = MainVideoName;
    SubStreamVideoName = SubVideoName;

    //监听报警主机
    AlarmHostServer = new QTcpServer(this);
    connect(AlarmHostServer,SIGNAL(newConnection()),this,SLOT(slotProcessAlarmHostConfigConnection()));
    AlarmHostServer->listen(QHostAddress::Any,GlobalConfig::AlarmHostServerPort);

    //监听主控制杆
    if(!SubVideoName.isEmpty()){//第一个点位没有辅助控制杆
        MainControlSever = new QTcpServer(this);
        connect(MainControlSever,SIGNAL(newConnection()),this,SLOT(slotProcessMainControlConnection()));
        MainControlSever->listen(QHostAddress::Any,GlobalConfig::MainControlServerPort);
    }

    //用来主动上报报警信息-->使用tcp短连接
    AlarmHostAlarmSocket = new QTcpSocket(this);
    connect(AlarmHostAlarmSocket,SIGNAL(connected()),this,SLOT(slotAlarmHostAlarmConnected()));
    connect(AlarmHostAlarmSocket,SIGNAL(disconnected()),this,SLOT(slotAlarmHostAlarmDisConnected()));
    connect(AlarmHostAlarmSocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(slotAlarmHostAlarmDisplayError(QAbstractSocket::SocketError)));
    AlarmHostAlarmConnectStateFlag = TcpServer::DisConnectedState;

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
        PreMainThreadState = MainWorkThread->ThreadState;
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
        PreSubThreadState = SubWorkThread->ThreadState;
        SubWorkThread->start();
    }

    //专门用来处理报警主机的下发的配置信息
    ProcessAlarmHostConfigMsgTimer = new QTimer(this);
    ProcessAlarmHostConfigMsgTimer->setInterval(50);
    connect(ProcessAlarmHostConfigMsgTimer,SIGNAL(timeout()),this,SLOT(slotProcessAlarmHostConfigMsg()));
    ProcessAlarmHostConfigMsgTimer->start();

    //用来发送所有报警信息，包括主控制杆和辅助控制杆以及其他报警信息
    SendAlarmMsgTimer = new QTimer(this);
    SendAlarmMsgTimer->setInterval(50);
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
    AlarmHostConfigSocket = AlarmHostServer->nextPendingConnection();
    connect(AlarmHostConfigSocket, SIGNAL(readyRead()), this, SLOT(slotRecvAlarmHostConfigMsg()));
    connect(AlarmHostConfigSocket, SIGNAL(disconnected()), this, SLOT(slotAlarmHostConfigDisconnect()));
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "AlarmHost Connect" << "\tIP =" << AlarmHostConfigSocket->peerAddress().toString() << "\tPort =" << AlarmHostConfigSocket->peerPort();

    //enableKeepAlive(AlarmHostConfigSocket->socketDescriptor(),30,1,1);
}

void TcpServer::slotRecvAlarmHostConfigMsg()
{
    if(AlarmHostConfigSocket->isOpen() && AlarmHostConfigSocket->isReadable()) {
        QByteArray data = AlarmHostConfigSocket->readAll();

        if(data.contains("StopServices")){
            AlarmHostConfigBuffer.clear();
        }

        if(data.contains("Reboot")){
            AlarmHostConfigBuffer.clear();
        }

        AlarmHostConfigBuffer.append(data);
    }
}

void TcpServer::slotAlarmHostConfigDisconnect()
{
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "AlarmHost Disconnect" << "\tIP =" << AlarmHostConfigSocket->peerAddress().toString() << "\tPort =" << AlarmHostConfigSocket->peerPort();
}

void TcpServer::slotProcessMainControlConnection()
{
    MainControlSocket = MainControlSever->nextPendingConnection();
    connect(MainControlSocket, SIGNAL(readyRead()), this, SLOT(slotRecvMainControlMsg()));
    connect(MainControlSocket, SIGNAL(disconnected()), this, SLOT(slotMainControlDisconnect()));
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "MainControl Connect" << "\tIP =" << MainControlSocket->peerAddress().toString() << "\tPort =" << MainControlSocket->peerPort();
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
                qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "MainControlBuffer.size() <= 97";
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
                qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "Parse error at line " << errorLine <<","<< "column " << errorColumn << ","<< errorMsg;
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
                        CommonSetting::WriteSettings("/bin/config.ini","AppConfig/SubStreamBasicPoint",SubStreamBasicPoint);

                        GlobalConfig::SubStreamFactor = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/SubStreamBasicPoint").split("#").at(0).toInt();
                        GlobalConfig::SubStreamSelectRect = GlobalConfig::GetSubStreamSelectRect();
                        GlobalConfig::SubStreamLightPoint = GlobalConfig::GetSubStreamLightPoint();

                        if(!SubStreamVideoName.isEmpty()){
                            SubWorkThread->factor = GlobalConfig::SubStreamFactor;
                            SubWorkThread->SelectRect = GlobalConfig::SubStreamSelectRect;
                            SubWorkThread->LightPoint = GlobalConfig::SubStreamLightPoint;
                        }
                    }

                    if(firstChildNode.nodeName() == "ServerIP"){
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString ServerIP = firstChildElement.text();
                        CommonSetting::WriteSettings("/bin/config.ini","AppConfig/ServerIP",ServerIP);
                        GlobalConfig::ServerIP = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/ServerIP");
                    }

                    if(firstChildNode.nodeName() == "MainIP"){
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString MainIP = firstChildElement.text();
                        CommonSetting::WriteSettings("/bin/config.ini","AppConfig/MainIP",MainIP);
                        GlobalConfig::MainIP = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/MainIP");
                    }

                    if(firstChildNode.nodeName() == "SubDefenceID"){
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString SubDefenceID = firstChildElement.text();
                        CommonSetting::WriteSettings("/bin/config.ini","AppConfig/SubDefenceID",SubDefenceID);
                        GlobalConfig::SubDefenceID = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/SubDefenceID");
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

                    QDomText SubStreamElementText =
                            AckDom.createTextNode(SubStreamBase64);
                    SubStreamElement.appendChild(SubStreamElementText);
                    RootElement.appendChild(SubStreamElement);

                    QTextStream Out(&MessageMerge);
                    AckDom.save(Out,4);

                    int length = MessageMerge.size();
                    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                    if(AlarmHostConfigSocket->isOpen() && AlarmHostConfigSocket->isWritable())
                    {
                        AlarmHostConfigSocket->write(MessageMerge.toAscii());
                        AlarmHostConfigSocket->waitForBytesWritten(300);
                        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "Return SubStream Pic";
                    }
                }
            }
        }
    }
}

void TcpServer::slotMainControlDisconnect()
{
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "MainControl Disconnect" << "\tIP =" << MainControlSocket->peerAddress().toString() << "\tPort =" << MainControlSocket->peerPort();
    MainControlSocket = NULL;
}

void TcpServer::slotMainAlarmImage()
{
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "MainStream Alarm";

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

    //创建AlarmImage元素
    QDomElement AlarmImageElement = AckDom.createElement("AlarmImage");
    AlarmImageElement.setAttribute("id","0");

    QByteArray data;
    QBuffer buffer(&data);
    MainWorkThread->AlarmImage.save(&buffer,"JPG");
    QString AlarmImageBase64 = data.toBase64();//base64的图片数据

    QDomText AlarmImageElementText = AckDom.createTextNode(AlarmImageBase64);
    AlarmImageElement.appendChild(AlarmImageElementText);
    RootElement.appendChild(AlarmImageElement);

    QTextStream Out(&MessageMerge);
    AckDom.save(Out,4);

    int length = MessageMerge.size();
    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

    AlarmMsgBuffer.append(MessageMerge);
}

void TcpServer::slotSubAlarmImage()
{
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "SubStream Alarm";

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

    //创建AlarmImage元素
    QDomElement AlarmImageElement = AckDom.createElement("AlarmImage");
    AlarmImageElement.setAttribute("id","1");

    QByteArray data;
    QBuffer buffer(&data);
    SubWorkThread->AlarmImage.save(&buffer,"JPG");
    QString AlarmImageBase64 = data.toBase64();//base64的图片数据

    QDomText AlarmImageElementText = AckDom.createTextNode(AlarmImageBase64);
    AlarmImageElement.appendChild(AlarmImageElementText);
    RootElement.appendChild(AlarmImageElement);

    QTextStream Out(&MessageMerge);
    AckDom.save(Out,4);

    int length = MessageMerge.size();
    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

    AlarmMsgBuffer.append(MessageMerge);
}

void TcpServer::slotOtherAlarmMsg(quint8 id)
{
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "Other Alarm";

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
    QDomElement AlarmElement = AckDom.createElement("Alarm");
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

        AlarmHostAlarmSocket->abort();
        AlarmHostAlarmSocket->connectToHost(GlobalConfig::ServerIP,GlobalConfig::ServerPort);
        bool state = AlarmHostAlarmSocket->waitForConnected(100);
        if(state){
            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "AlarmHostAlarmConnectStateFlag:ConnectedState";
            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "SendBytes = " << AlarmHostAlarmSocket->write(XmlData.toAscii());
            AlarmHostAlarmSocket->waitForBytesWritten(100);
        }else{
            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "AlarmHostAlarmConnectStateFlag:DisConnectedState";
            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "slotSendAlarmMsg:" <<  AlarmHostAlarmSocket->errorString();
        }
    }
}

void TcpServer::slotAlarmHostAlarmConnected()
{
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "connect to alarm host alarm succeed";
    AlarmHostAlarmConnectStateFlag = TcpServer::ConnectedState;
}

void TcpServer::slotAlarmHostAlarmDisConnected()
{
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "alarm host alarm close connection";
    AlarmHostAlarmConnectStateFlag = TcpServer::DisConnectedState;
    AlarmHostAlarmSocket->abort();
}

void TcpServer::slotAlarmHostAlarmDisplayError(QAbstractSocket::SocketError socketError)
{
    switch(socketError){
    case QAbstractSocket::ConnectionRefusedError:
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "AlarmHostAlarm:QAbstractSocket::ConnectionRefusedError";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "AlarmHostAlarm:QAbstractSocket::RemoteHostClosedError";
        break;
    case QAbstractSocket::HostNotFoundError:
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "AlarmHostAlarm:QAbstractSocket::HostNotFoundError";
        break;
    default:
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "AlarmHostAlarm:" + AlarmHostAlarmSocket->errorString();
        break;
    }

    AlarmHostAlarmConnectStateFlag = TcpServer::DisConnectedState;
    AlarmHostAlarmSocket->abort();
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
            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "SubControlBuffer.size() <= 57";
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
            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "Parse error at line " << errorLine <<","<< "column " << errorColumn << ","<< errorMsg;
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
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "connect to sub control succeed";
    SubControlConnectStateFlag = TcpServer::ConnectedState;

    //停止重连右边的辅助控制杆定时器
    ReConnectSubControlTimer->stop();

    //开启探测主控制杆与辅助控制杆tcp长连接的状态
    enableKeepAlive(SubControlSocket->socketDescriptor(),1,1,1);
}

void TcpServer::slotSubControlDisConnected()
{
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "sub control close connection";
    SubControlConnectStateFlag = TcpServer::DisConnectedState;
    SubControlSocket->abort();
}


void TcpServer::slotSubControlDisplayError(QAbstractSocket::SocketError socketError)
{
    switch(socketError){
    case QAbstractSocket::ConnectionRefusedError:
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "SubControl:QAbstractSocket::ConnectionRefusedError";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "SubControl:QAbstractSocket::RemoteHostClosedError";
        break;
    case QAbstractSocket::HostNotFoundError:
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "SubControl:QAbstractSocket::HostNotFoundError";
        break;
    default:
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "SubControl:" + SubControlSocket->errorString();
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
    bool isReturnDeviceStatus = false;//本设备返回设备的状态
    bool isReturnOK = false;//本设备返回OK消息给报警主机
    bool isGetBasicPic = false;//本设备返回主控制杆图片和辅助控制杆图片给报警主机
    bool isGetMainStream = false;//本设备返回主控制杆图片给报警主机
    bool isSubStream = false;//本设备将辅助控制杆基准点配置信息发送给右边的辅助控制杆
    bool isSetSubStreamPWM = false;//本设备将报警主机调节辅助控制杆灯柱亮度的PWM发送给辅助控制杆
    bool isValidConfig = true;//最后一个点位没有主控制杆，如果配置进行配置就是进行非法操作

    QString SubStreamBasicPoint;//辅助控制杆基准点配置信息
    quint8 PWM;
    QByteArray FullPackage;

    if(AlarmHostConfigBuffer.size() <= 37){
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "AlarmHostConfigBuffer.size() <= 37";
        AlarmHostConfigBuffer.clear();
        return;
    }

    int index = AlarmHostConfigBuffer.indexOf("IALARM:");
    if(index == -1){
        return;
    }

    int MsgLength = AlarmHostConfigBuffer.mid(index + 7, 13).toUInt();
    if(AlarmHostConfigBuffer.size() - 20 >= MsgLength) {
        FullPackage = AlarmHostConfigBuffer.mid(20, MsgLength);
        AlarmHostConfigBuffer = AlarmHostConfigBuffer.mid(20 + MsgLength);
    }else{
        return;
    }

    if(!dom.setContent(FullPackage, &errorMsg, &errorLine, &errorColumn)) {
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "Parse error at line " << errorLine <<","<< "column " << errorColumn << ","<< errorMsg;
        return;
    }

    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << FullPackage;

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
                        isReturnDeviceStatus = true;
                        if(!MainStreamVideoName.isEmpty()){
                            MainWorkThread->isStopCapture = false;
                        }

                        if(!SubStreamVideoName.isEmpty()){
                            SubWorkThread->isStopCapture = false;
                        }
                    }

                    if(firstChildNode.nodeName() == "StopServices"){
                        isReturnDeviceStatus = true;
                        if(!MainStreamVideoName.isEmpty()){
                            MainWorkThread->isStopCapture = true;
                        }

                        if(!SubStreamVideoName.isEmpty()){
                            SubWorkThread->isStopCapture = true;
                        }
                    }

                    if(firstChildNode.nodeName() == "ServerIP"){
                        if(!MainStreamVideoName.isEmpty()){
                            isReturnOK = true;
                            isBasicConfig = true;
                            isValidConfig = true;
                            QDomElement firstChildElement = firstChildNode.toElement();
                            QString ServerIP = firstChildElement.text();
                            CommonSetting::WriteSettings("/bin/config.ini","AppConfig/ServerIP",ServerIP);
                            GlobalConfig::ServerIP = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/ServerIP");
                        }else{
                            isValidConfig = false;
                        }
                    }

                    if(firstChildNode.nodeName() == "SubIP"){
                        if(!MainStreamVideoName.isEmpty()){
                            isReturnOK = true;
                            isBasicConfig = true;
                            isValidConfig = true;
                            QDomElement firstChildElement = firstChildNode.toElement();
                            QString SubIP = firstChildElement.text();
                            CommonSetting::WriteSettings("/bin/config.ini","AppConfig/SubIP",SubIP);
                            GlobalConfig::SubIP = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/SubIP");
                        }else{
                            isValidConfig = false;
                        }
                    }

                    if(firstChildNode.nodeName() == "DefenceID"){
                        if(!MainStreamVideoName.isEmpty()){
                            isReturnOK = true;
                            isBasicConfig = true;
                            isValidConfig = true;
                            QDomElement firstChildElement = firstChildNode.toElement();
                            QString DefenceID = firstChildElement.text();
                            CommonSetting::WriteSettings("/bin/config.ini","AppConfig/MainDefenceID",DefenceID);
                            GlobalConfig::MainDefenceID = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/MainDefenceID");
                        }else{
                            isValidConfig = false;
                        }
                    }


                    if(firstChildNode.nodeName() == "CameraSleepTime"){
                        //采集间隔（单位毫秒）
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString CameraSleepTime = firstChildElement.text();
                        CommonSetting::WriteSettings("/bin/config.ini","AppConfig/CameraSleepTime",CameraSleepTime);

                        GlobalConfig::CameraSleepTime = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/CameraSleepTime").toInt();
                    }

                    if(firstChildNode.nodeName() == "DeviceMacAddr"){
                        //设备MAC地址（同一局域网内设备不能有重复的MAC地址）
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString DeviceMacAddr = firstChildElement.text();
                        CommonSetting::WriteSettings("/bin/config.ini","AppConfig/DeviceMacAddr",DeviceMacAddr);

                        GlobalConfig::DeviceMacAddr = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/DeviceMacAddr");

                        //重启网卡
                        system("/etc/init.d/ifconfig-eth0");

                        GlobalConfig::LocalHostIP = CommonSetting::GetLocalHostIP();
                        GlobalConfig::Gateway = CommonSetting::GetGateway();
                        GlobalConfig::MAC = CommonSetting::ReadMacAddress();
                    }

                    if(firstChildNode.nodeName() == "DeviceIPAddrPrefix"){
                        //设备网段 默认192.168.8.
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString DeviceIPAddrPrefix = firstChildElement.text();
                        CommonSetting::WriteSettings("/bin/config.ini","AppConfig/DeviceIPAddrPrefix",DeviceIPAddrPrefix);

                        GlobalConfig::DeviceIPAddrPrefix = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/DeviceIPAddrPrefix");

                        //重启网卡
                        system("/etc/init.d/ifconfig-eth0");

                        GlobalConfig::LocalHostIP = CommonSetting::GetLocalHostIP();
                        GlobalConfig::Gateway = CommonSetting::GetGateway();
                        GlobalConfig::MAC = CommonSetting::ReadMacAddress();
                    }

                    if(firstChildNode.nodeName() == "GetBasicPic"){
                            isGetBasicPic = true;
                    }

                    if(firstChildNode.nodeName() == "MainStream"){
                        //主控制杆基准点保存到本地
                        QDomElement firstChildElement = firstChildNode.toElement();
                        QString MainStreamBasicPoint = firstChildElement.text();
                        MainStreamBasicPoint = MainStreamBasicPoint.split(",").join(".");
                        CommonSetting::WriteSettings("/bin/config.ini","AppConfig/MainStreamBasicPoint",MainStreamBasicPoint);

                        GlobalConfig::MainStreamFactor = CommonSetting::ReadSettings("/bin/config.ini","AppConfig/MainStreamBasicPoint").split("#").at(0).toInt();
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

                    if(firstChildNode.nodeName() == "SystemReboot"){
                        isReturnOK = true;
                        isSystemReboot = true;
                    }

                    firstChildNode = firstChildNode.nextSibling();//下一个节点
                }


                if(isReturnDeviceStatus){
                    QString MessageMerge;
                    QDomDocument AckDom;

                    //xml声明
                    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                    AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                    //创建根元素
                    QDomElement RootElement = AckDom.createElement("Device");
                    RootElement.setAttribute("DeviceIP",GlobalConfig::LocalHostIP);
                    if(GlobalConfig::MainDefenceID.isEmpty()){
                        RootElement.setAttribute("DefenceID",GlobalConfig::SubDefenceID);
                    }else{
                        RootElement.setAttribute("DefenceID",GlobalConfig::MainDefenceID);
                    }
                    AckDom.appendChild(RootElement);

                    //创建DeviceStatus元素
                    QDomElement DeviceStatusElement = AckDom.createElement("DeviceStatus");
                    QString DeviceStatus;
                    if(!MainStreamVideoName.isEmpty()){
                        if(MainWorkThread->isStopCapture){
                            DeviceStatus = QString("0");
                        }else{
                            DeviceStatus = QString("1");
                        }
                    }

                    if(!SubStreamVideoName.isEmpty()){
                        if(SubWorkThread->isStopCapture){
                            DeviceStatus = QString("0");
                        }else{
                            DeviceStatus = QString("1");
                        }
                    }

                    if(MainStreamVideoName.isEmpty() && SubStreamVideoName.isEmpty()) {
                        DeviceStatus = QString("0");
                    }

                    QDomText DeviceStatusElementText = AckDom.createTextNode(DeviceStatus);
                    DeviceStatusElement.appendChild(DeviceStatusElementText);
                    RootElement.appendChild(DeviceStatusElement);

                    QTextStream Out(&MessageMerge);
                    AckDom.save(Out,4);

                    int length = MessageMerge.size();
                    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                    if(AlarmHostConfigSocket->isOpen() && AlarmHostConfigSocket->isWritable()) {
                        AlarmHostConfigSocket->write(MessageMerge.toAscii());
                        AlarmHostConfigSocket->waitForBytesWritten(300);

                        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << MessageMerge;
                    }
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

                    if(AlarmHostConfigSocket->isOpen() && AlarmHostConfigSocket->isWritable()) {
                        AlarmHostConfigSocket->write(MessageMerge.toAscii());
                        AlarmHostConfigSocket->waitForBytesWritten(300);
                        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << MessageMerge;

                    }
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

                    if(SubControlConnectStateFlag == TcpServer::ConnectedState){
                        if(SubControlSocket->isOpen() && SubControlSocket->isWritable()) {
                            SubControlSocket->write(MessageMerge.toAscii());
                            SubControlSocket->waitForBytesWritten(300);
                            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << MessageMerge;
                        }
                    }else if(SubControlConnectStateFlag == TcpServer::DisConnectedState){
                        SubControlSocket->connectToHost(GlobalConfig::SubIP,GlobalConfig::MainControlServerPort);
                        bool state = SubControlSocket->waitForConnected(100);
                        if(state){
                            if(SubControlSocket->isOpen() && SubControlSocket->isWritable()) {
                                SubControlSocket->write(MessageMerge.toAscii());
                                SubControlSocket->waitForBytesWritten(300);
                                qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << MessageMerge;
                            }
                        }
                    }
                }

                if(!isValidConfig){//非法配置
                    QString MessageMerge;
                    QDomDocument AckDom;

                    //xml声明
                    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
                    AckDom.appendChild(AckDom.createProcessingInstruction("xml", XmlHeader));

                    //创建根元素
                    QDomElement RootElement = AckDom.createElement("Device");

                    RootElement.setAttribute("DeviceIP",GlobalConfig::LocalHostIP);
                    RootElement.setAttribute("DefenceID",GlobalConfig::SubDefenceID);

                    QDomText RootElementText = AckDom.createTextNode("非法配置,本设备只有辅助控制杆,没有主控制杆，配置无效");
                    RootElement.appendChild(RootElementText);

                    AckDom.appendChild(RootElement);

                    QTextStream Out(&MessageMerge);
                    AckDom.save(Out,4);

                    int length = MessageMerge.size();
                    MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                    if(AlarmHostConfigSocket->isOpen() && AlarmHostConfigSocket->isWritable()) {
                        AlarmHostConfigSocket->write(MessageMerge.toAscii());
                        AlarmHostConfigSocket->waitForBytesWritten(300);
                        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << MessageMerge;

                    }
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
                        RootElement.setAttribute("DefenceID",GlobalConfig::SubDefenceID);

                        QDomText RootElementText = AckDom.createTextNode("非法配置,本设备只有辅助控制杆,没有主控制杆，不能获取基准图片");
                        RootElement.appendChild(RootElementText);

                        AckDom.appendChild(RootElement);

                        QTextStream Out(&MessageMerge);
                        AckDom.save(Out,4);

                        int length = MessageMerge.size();
                        MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                        if(AlarmHostConfigSocket->isOpen() && AlarmHostConfigSocket->isWritable())
                        {
                            AlarmHostConfigSocket->write(MessageMerge.toAscii());
                            AlarmHostConfigSocket->waitForBytesWritten(300);
                            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << MessageMerge;

                        }
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
                            QDomElement RootElement = AckDom.createElement("MainControlDevice");
                            AckDom.appendChild(RootElement);

                            //创建GetSubStream元素
                            QDomElement GetSubStreamElement =
                                    AckDom.createElement("GetSubStream");
                            RootElement.appendChild(GetSubStreamElement);

                            QTextStream Out(&MessageMerge);
                            AckDom.save(Out,4);

                            int length = MessageMerge.size();
                            MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                            if(SubControlConnectStateFlag == TcpServer::ConnectedState){
                                if(SubControlSocket->isOpen() && SubControlSocket->isWritable())
                                {
                                    SubControlSocket->write(MessageMerge.toAscii());
                                    SubControlSocket->waitForBytesWritten(300);
                                    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << MessageMerge;
                                }
                            }else if(SubControlConnectStateFlag == TcpServer::DisConnectedState){
                                SubControlSocket->connectToHost(GlobalConfig::SubIP,GlobalConfig::MainControlServerPort);
                                bool state = SubControlSocket->waitForConnected(100);
                                if(state){
                                    if(SubControlSocket->isOpen() && SubControlSocket->isWritable())
                                    {
                                        SubControlSocket->write(MessageMerge.toAscii());
                                        SubControlSocket->waitForBytesWritten(300);
                                        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << MessageMerge;
                                    }
                                }
                            }
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
                            QDomElement MainStreamElement = AckDom.createElement("MainStream");
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

                            QTextStream Out(&MessageMerge);
                            AckDom.save(Out,4);

                            int length = MessageMerge.size();
                            MessageMerge = QString("IALARM:") + QString("%1").arg(length,13,10,QLatin1Char('0')) + MessageMerge;

                            if(AlarmHostConfigSocket->isOpen() && AlarmHostConfigSocket->isWritable())
                            {
                                AlarmHostConfigSocket->write(MessageMerge.toAscii());
                                AlarmHostConfigSocket->waitForBytesWritten(300);
                                qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "Return MainStream Pic";
                            }
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

                        if(AlarmHostConfigSocket->isOpen() && AlarmHostConfigSocket->isWritable()){
                            AlarmHostConfigSocket->write(MessageMerge.toAscii());
                            AlarmHostConfigSocket->waitForBytesWritten(300);
                            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "GetMainStream";
                        }

                        MainWorkThread->NormalImage = QImage();
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


                        if(AlarmHostConfigSocket->isOpen() && AlarmHostConfigSocket->isWritable()) {
                            AlarmHostConfigSocket->write(MessageMerge.toAscii());
                            AlarmHostConfigSocket->waitForBytesWritten(300);
                            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << MessageMerge;
                        }

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

                        if(SubControlConnectStateFlag == TcpServer::ConnectedState){
                            if(SubControlSocket->isOpen() && SubControlSocket->isWritable()) {
                                SubControlSocket->write(MessageMerge.toAscii());
                                SubControlSocket->waitForBytesWritten(300);
                                qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << MessageMerge;
                            }
                        }else if(SubControlConnectStateFlag == TcpServer::DisConnectedState){
                            SubControlSocket->connectToHost(GlobalConfig::SubIP,GlobalConfig::MainControlServerPort);
                            bool state = SubControlSocket->waitForConnected(100);
                            if(state){
                                if(SubControlSocket->isOpen() && SubControlSocket->isWritable())
                                {
                                    SubControlSocket->write(MessageMerge.toAscii());
                                    SubControlSocket->waitForBytesWritten(300);
                                    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << MessageMerge;
                                }
                            }
                        }
                    }
                }

                if(isSystemReboot){
                    system("reboot");
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

                    if(SubControlConnectStateFlag == TcpServer::ConnectedState){
                        if(SubControlSocket->isOpen() && SubControlSocket->isWritable()) {
                            SubControlSocket->write(MessageMerge.toAscii());
                            SubControlSocket->waitForBytesWritten(300);
                            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << MessageMerge;
                        }
                    }else if(SubControlConnectStateFlag == TcpServer::DisConnectedState){
                        SubControlSocket->connectToHost(GlobalConfig::SubIP,GlobalConfig::MainControlServerPort);
                        bool state = SubControlSocket->waitForConnected(100);
                        if(state){
                            if(SubControlSocket->isOpen() && SubControlSocket->isWritable()) {
                                SubControlSocket->write(MessageMerge.toAscii());
                                SubControlSocket->waitForBytesWritten(300);
                                qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << MessageMerge;
                            }
                        }
                    }
                }
            }
        }
    }
}

void TcpServer::slotMainUSBCameraOffline()
{
    GetCameraInfo::Instance()->GetMainCameraDevice();

    MainWorkThread->operatecamera->VideoName = GetCameraInfo::MainCamera;
    MainWorkThread->VideoName = GetCameraInfo::MainCamera;
    MainWorkThread->isReInitialize = true;
    MainWorkThread->isStopCapture = false;
}

void TcpServer::slotSubUSBCameraOffline()
{
    GetCameraInfo::Instance()->GetMainCameraDevice();

    SubWorkThread->operatecamera->VideoName = GetCameraInfo::SubCamera;
    SubWorkThread->VideoName = GetCameraInfo::SubCamera;
    SubWorkThread->isReInitialize = true;
    SubWorkThread->isStopCapture = false;
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

void TcpServer::slotReConnectSubControl()
{
    SubControlSocket->abort();
    SubControlSocket->connectToHost(GlobalConfig::SubIP,GlobalConfig::MainControlServerPort);
}

void TcpServer::slotCheckWorkThreadState()
{

}
