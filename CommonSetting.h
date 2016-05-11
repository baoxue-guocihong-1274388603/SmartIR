#ifndef COMMONSETTING_H
#define COMMONSETTING_H

#include <QObject>
#include <QTextCodec>
#include <QFile>
#include <QSettings>
#include <QDateTime>
#include <QDomDocument>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <QBuffer>
#include <QProcess>
#include <QList>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QRect>
#include <QThread>
#include <QMutex>
#include <QElapsedTimer>

class CommonSetting : public QObject
{
public:
    CommonSetting();
    ~CommonSetting();

    //设置编码为UTF8
//    static void SetUTF8Code()
//    {
//        QTextCodec *codec=
//                QTextCodec::codecForName("UTF-8");
//        QTextCodec::setCodecForLocale(codec);
//        QTextCodec::setCodecForCStrings(codec);
//        QTextCodec::setCodecForTr(codec);
//    }

    //获取当前日期时间星期
    static QString GetCurrentDateTime()
    {
        QDateTime time = QDateTime::currentDateTime();
        return time.toString("yyyy-MM-dd_hh-mm-ss");
    }

    static QString GetCurrentDateTimeNoSpace()
    {
        QDateTime time = QDateTime::currentDateTime();
        return time.toString("yyyy-MM-dd_hh:mm:ss");
    }

    //读取文件内容
    static QString ReadFile(QString fileName)
    {
        QFile file(fileName);
        QByteArray fileContent;
        if (file.open(QIODevice::ReadOnly)){
            fileContent = file.readAll();
        }
        file.close();
        return QString(fileContent);
    }

    //写数据到文件
    static void WriteCommonFile(QString fileName,QString data)
    {
        QFile file(fileName);
        if(file.open(QFile::ReadWrite | QFile::Append)){
            file.write(data.toLocal8Bit().data());
            file.close();
        }
    }

    static void WriteCommonFileTruncate(QString fileName,QString data)
    {
        QFile file(fileName);
        if(file.open(QFile::ReadWrite | QFile::Truncate)){
            file.write(data.toLocal8Bit().data());
            file.close();
        }
    }

    static void WriteXmlFile(QString fileName,QString data)
    {
        QFile file(fileName);
        if(file.open(QFile::ReadWrite | QFile::Append)){
            QDomDocument dom;
            QTextStream out(&file);
            out.setCodec("UTF-8");
            dom.setContent(data);
            dom.save(out,4,QDomNode::EncodingFromTextStream);
            file.close();
        }
    }

    //创建文件夹
    static void CreateFolder(QString path,QString strFolder)
    {
        QDir dir(path);
        dir.mkdir(strFolder);
    }

    //删除文件夹
    static bool deleteFolder(const QString &dirName)
    {
        QDir directory(dirName);
        if (!directory.exists()){
            return true;
        }

        QString srcPath =
                QDir::toNativeSeparators(dirName);
        if (!srcPath.endsWith(QDir::separator()))
            srcPath += QDir::separator();

        QStringList fileNames =
                directory.entryList(QDir::AllEntries |
                                    QDir::NoDotAndDotDot | QDir::Hidden);
        bool error = false;
        for (QStringList::size_type i=0; i != fileNames.size(); ++i){
            QString filePath = srcPath +
                    fileNames.at(i);
            QFileInfo fileInfo(filePath);
            if(fileInfo.isFile() ||
                    fileInfo.isSymLink()){
                QFile::setPermissions(filePath, QFile::WriteOwner);
                if (!QFile::remove(filePath)){
                    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "remove file" << filePath << " faild!";
                    error = true;
                }
            }else if (fileInfo.isDir()){
                if (!deleteFolder(filePath)){
                    error = true;
                }
            }
        }

        if (!directory.rmdir(
                    QDir::toNativeSeparators(
                        directory.path()))){
            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "remove dir" << directory.path() << " faild!";
            error = true;
        }

        return !error;
    }

    //拷贝文件：
    static bool copyFileToPath(QString sourceDir ,QString toDir, bool coverFileIfExist)
    {
        if (sourceDir == toDir){
            return true;
        }
        if (!QFile::exists(sourceDir)){
            return false;
        }
        QDir *createfile     = new QDir;
        bool exist = createfile->exists(toDir);
        if (exist){
            if(coverFileIfExist){
                createfile->remove(toDir);
            }
        }//end if

        if(!QFile::copy(sourceDir, toDir))
        {
            return false;
        }
        return true;
    }

    //拷贝文件夹
    static bool copyDirectoryFiles(const QString &fromDir, const QString &toDir, bool coverFileIfExist)
    {
        QDir sourceDir(fromDir);
        QDir targetDir(toDir);
        if(!targetDir.exists()){//如果目标目录不存在,则进行创建
            if(!targetDir.mkdir(
                        targetDir.absolutePath()))
                return false;
        }

        QFileInfoList fileInfoList =
                sourceDir.entryInfoList();
        foreach(QFileInfo fileInfo, fileInfoList){
            if(fileInfo.fileName() == "." ||
                    fileInfo.fileName() == "..")
                continue;

            if(fileInfo.isDir()){//当为目录时，递归的进行copy
                if(!copyDirectoryFiles(
                            fileInfo.filePath(),
                            targetDir.filePath(
                                fileInfo.fileName()),
                            coverFileIfExist))
                    return false;
            }else{//当允许覆盖操作时，将旧文件进行删除操作
                if(coverFileIfExist && targetDir.exists(
                            fileInfo.fileName())){
                    targetDir.remove(
                                fileInfo.fileName());
                }

                //进行文件copy
                if(!QFile::copy(fileInfo.filePath(),
                                targetDir.filePath(
                                    fileInfo.fileName()))){
                    return false;
                }
            }
        }
        return true;
    }

    //返回指定路径下符合筛选条件的文件，注意只返回文件名，不返回绝对路径
    static QStringList GetFileNames(QString path,QString filter)
    {
        QDir dir;
        dir.setPath(path);
        QStringList fileFormat(filter);
        dir.setNameFilters(fileFormat);
        dir.setFilter(QDir::Files);
        dir.setSorting(QDir::Time | QDir::Reversed);

        return dir.entryList();
    }
    //返回指定路径下文件夹的集合,注意只返回文件夹名，不返回绝对路径
    static QStringList GetDirNames(QString path)
    {
        QDir dir(path);
        dir.setFilter(QDir::Dirs);
        dir.setSorting(QDir::Time);
        QStringList dirlist = dir.entryList();
        QStringList dirNames;
        foreach(const QString &dirName,dirlist){
            if(dirName == "." || dirName == "..")
                continue;
            dirNames << dirName;
        }
        return dirNames;
    }

    //QSetting应用
    static void WriteSettings(const QString &ConfigFile,
                              const QString &key,
                              const QString value)
    {

        QSettings settings(ConfigFile,QSettings::IniFormat);
        settings.setValue(key,value);
    }

    static QString ReadSettings(const QString &ConfigFile,
                                const QString &key)
    {
        QSettings settings(ConfigFile,QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        return settings.value(key).toString();
    }

    //将指定路径下指定格式的文件返回(只返回文件名，不返回绝对路径)
    static QStringList fileFilter(const QString &path,
                                  const QString &filter)
    {
        QDir dir(path);
        QStringList fileFormat(filter);
        dir.setNameFilters(fileFormat);
        dir.setSorting(QDir::Time);
        dir.setFilter(QDir::Files);
        QStringList fileList = dir.entryList();
        return fileList;
    }

    //文件是否存在
    static bool FileExists(QString strFile)
    {
        QFile tempFile(strFile);
        if (tempFile.exists()){
            return true;
        }
        return false;
    }

    static QString ReadMacAddress()
    {
        QList<QNetworkInterface> list =
                QNetworkInterface::allInterfaces();
        foreach(const QNetworkInterface &interface,list){
            if(interface.name() == "eth0"){
                QString MacAddress = interface.hardwareAddress();
                return MacAddress;
            }
        }
    }

    static QString GetLocalHostIP()
    {
        QList<QNetworkInterface> list =
                QNetworkInterface::allInterfaces();
        foreach(const QNetworkInterface &interface,list){
            if(interface.name() == "eth0"){
                QList<QNetworkAddressEntry> entrylist = interface.addressEntries();
                foreach (QNetworkAddressEntry entry, entrylist) {
                    return entry.ip().toString();
                }
            }
        }
    }

    static QString GetMask()
    {
        QList<QNetworkInterface> list =
                QNetworkInterface::allInterfaces();
        foreach(const QNetworkInterface &interface,list){
            if(interface.name() == "eth0"){
                QList<QNetworkAddressEntry> entrylist = interface.addressEntries();
                foreach (QNetworkAddressEntry entry, entrylist) {
                    return entry.netmask().toString();
                }
            }
        }
    }

    static QString GetGateway()
    {
        system("rm -rf gw.txt");
        system("route -n | grep 'UG' | awk '{print $2}' > gw.txt");

        QString gw;

        QFile file("gw.txt");
        if(file.open(QFile::ReadOnly)){
            gw = QString(file.readAll()).trimmed().simplified();
            file.close();
        }

        return gw;
    }

    static void SettingSystemDateTime(QString SystemDate)
    {
        //设置系统时间
        QString year = SystemDate.mid(0,4);
        QString month = SystemDate.mid(5,2);
        QString day = SystemDate.mid(8,2);
        QString hour = SystemDate.mid(11,2);
        QString minute = SystemDate.mid(14,2);
        QString second = SystemDate.mid(17,2);
        QProcess *process = new QProcess;
        process->start(tr("date %1%2%3%4%5.%6").arg(month).arg(day)
                       .arg(hour).arg(minute)
                       .arg(year).arg(second));
        process->waitForFinished(200);
        process->start("hwclock -w");
        process->waitForFinished(200);
        delete process;
    }

    static void Sleep(quint16 msec)
    {
        QTime dieTime = QTime::currentTime().addMSecs(msec);
        while(QTime::currentTime() < dieTime)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }

    static void outputMessage(QtMsgType type, const char *msg)
    {
        QString current_date_time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ddd");
        QString message = QString("%1 %2").arg(msg).arg(current_date_time);

        QFile file("/mnt/log.txt");
        file.open(QIODevice::WriteOnly | QIODevice::Append);
        QTextStream text_stream(&file);
        text_stream << message << "\r\n";
        file.flush();
        file.close();
    }
};

#endif // COMMONSETTING_H
