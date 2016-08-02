#include "auto_updater.hpp"
#include "update_info_parser.hpp"

#include <qt_enhance/compressor/file_compressor.hpp>
#include <qt_enhance/compressor/folder_compressor.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QUrl>

#include <iterator>

auto_updater::auto_updater(QString const &app_to_start, QObject *parent) :
    QObject(parent),
    app_to_start_(app_to_start),
    manager_(new QNetworkAccessManager(this))
{
    QDir dir(QCoreApplication::applicationDirPath());
    if(dir.cdUp()){
        parent_path_ = dir.absolutePath();
    }
}

void auto_updater::start()
{    
    download_details_.clear();
    qDebug()<<"start config parse";
    update_info_parser info_parser;
    update_info_local_ = info_parser.read(QCoreApplication::applicationDirPath() +
                                          "/update_info_local.xml");

    auto update_it = update_info_local_.find("update_info");
    if(update_it != std::end(update_info_local_)){
        auto *reply = manager_->get(QNetworkRequest(update_it->second.url_));
        connect(reply, &QNetworkReply::finished, this,
                &auto_updater::update_info_finished);
        connect(reply, &QNetworkReply::downloadProgress,
                [this](qint64 bytesReceived, qint64 bytesTotal)
        {
            qDebug()<<QString("Download update_info : %1/%2").arg(bytesReceived).
                      arg(bytesTotal);
        });
    }
}

void auto_updater::decompress_update_content(download_iter_type it,
                                             QNetworkReply *reply)
{
    QString copy_to = it->second.copy_to_;
    copy_to.replace("$${PARENT}", parent_path_);
    QString file_name = it->second.display_name_ + ".zip";
    QString const zip_file = copy_to + "/" + file_name;
    qDebug()<<"update content path : "<<zip_file;
    qDebug()<<"copy update content to : "<<copy_to;
    QFile file(zip_file);
    if(file.open(QIODevice::WriteOnly)){
        file.write(reply->readAll());
        file.close();
        if(it->second.unzip_ == "qcompress_folder"){
            qDebug()<<"decompress folder : "<<file_name;
            qte::cp::folder_compressor fc;
            fc.decompress_folder(zip_file, copy_to);
        }else if(it->second.unzip_ == "qcompress_file"){
            qDebug()<<"decompress file : "<<file_name;
            qte::cp::decompress(zip_file, copy_to + "/" + it->second.display_name_);
        }
        file.remove();
    }else{
        qDebug()<<"Cannot write into file : "
               <<zip_file;
    }
}

std::map<QString, QString> auto_updater::create_file_maps() const
{
    QFile file("file_list.txt");
    std::map<QString, QString> file_maps;
    if(file.open(QIODevice::ReadOnly)){
        QTextStream stream(&file);
        QString line = stream.readLine();
        while(!line.isEmpty()){
            QStringList strs = line.split(" ");
            if(strs.size() == 2){
                strs[1].replace(QString("$${PARENT}"), parent_path_);
                file_maps.insert(std::make_pair(strs[0], strs[1]));
            }
            line = stream.readLine();
        }
    }else{
        qDebug()<<"cannot read file_list.txt";
    }

    return file_maps;
}

void auto_updater::erase_old_contents()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if(reply){
        guard_delete_later<QNetworkReply> guard(reply);
        if(reply->error() != QNetworkReply::NoError){
            qDebug()<<reply->errorString();
        }else{
            QString const erase_list = reply->readAll();
            QStringList lists = erase_list.split("\n");
            for(auto &list : lists){
                if(!list.isEmpty()){
                    list.replace("$${PARENT}", parent_path_);
                    list = list.simplified();
                    qDebug()<<list;
                    if(QFileInfo(list).isDir()){
                        QDir dir(list);
                        if(!dir.removeRecursively()){
                            qDebug()<<"cannot remove dir : "<<list;
                        }
                    }else{
                        if(!QFile::remove(list)){
                            qDebug()<<"cannot remove file : "<<list;
                        }
                    }
                }
            }
        }
    }
    start_updated_app();
}

void auto_updater::exit_app()
{
    qDebug()<<"press any key to exit...";
    exit(0);
}

void auto_updater::start_updated_app()
{
    if(!app_to_start_.isEmpty()){
        QProcess *process = new QProcess(this);
        connect(process, &QProcess::errorOccurred, [&](QProcess::ProcessError)
        {
            qDebug()<<process->errorString();
            exit_app();
        });
        app_to_start_.replace("$${PARENT}", parent_path_);
        process->startDetached(app_to_start_);
        process->waitForStarted(-1);
    }

     exit_app();
}

void auto_updater::download_erase_list()
{
    qDebug()<<__func__;
    std::map<QString, QString> const file_maps = create_file_maps();
    auto it = file_maps.find("erase_list");
    if(it != std::end(file_maps)){
        qDebug()<<"erase_list location : "<<it->second;
        auto *reply = manager_->get(QNetworkRequest(it->second));
        connect(reply, &QNetworkReply::finished, this,
                &auto_updater::erase_old_contents);
        connect(reply, &QNetworkReply::downloadProgress,
                [](qint64 bytesReceived, qint64 bytesTotal)
        {
            qDebug()<<QString("Download erase_list.txt : %1/%2").
                      arg(bytesReceived).arg(bytesTotal);
        });
    }else{
        qDebug()<<"cannot find erase list";
    }
}

void auto_updater::update_content()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if(reply){
        guard_delete_later<QNetworkReply> guard(reply);
        download_iter_type it = download_details_.find(reply);
        if(it != std::end(download_details_)){
            qDebug()<<"update_content of : "<<it->second.display_name_;
            decompress_update_content(it, reply);
            qDebug()<<"update_content "<<it->second.display_name_<<" finished";
        }
    }
    download_update_contents();
}

void auto_updater::download_update_content(update_info info)
{
    QNetworkReply *reply = manager_->get(QNetworkRequest(info.url_));
    download_details_.insert(std::make_pair(reply, info));
    auto const name = info.display_name_;
    connect(reply, &QNetworkReply::finished, this,
            &auto_updater::update_content);
    connect(reply, &QNetworkReply::downloadProgress,
            [this, name](qint64 bytesReceived, qint64 bytesTotal)
    {
        qDebug()<<QString("Download %1 : %2/%3").arg(name).
                  arg(bytesReceived).arg(bytesTotal);
    });
}

void auto_updater::download_update_contents()
{        
    if(!update_info_local_.empty() && !update_info_remote_.empty()){
        iter_type remote_it = std::begin(update_info_remote_);
        iter_type local_it = update_info_local_.find(remote_it->first);
        qDebug()<<"download_update_contents : "<<remote_it->second.display_name_;        
        QString remote_name = remote_it->second.display_name_;
        if(local_it == std::end(update_info_local_)){
            qDebug()<<"local_it == std::end(update_info_local_)";
            auto const info = remote_it->second;
            update_info_remote_.erase(remote_it);
            download_update_content(info);
        }else{
            if(remote_it->second.version_ > local_it->second.version_){
                qDebug()<<"remote_it->second.version_ > local_it->second.version_";
                auto const info = remote_it->second;
                update_info_local_.erase(local_it);
                update_info_remote_.erase(remote_it);
                download_update_content(info);
            }else{
                qDebug()<<"do not need to update content : "<<remote_it->second.display_name_;
                update_info_local_.erase(local_it);
                update_info_remote_.erase(remote_it);
                download_update_contents();
            }
        }
        qDebug()<<remote_name<<" : erase update info remote";
    }else{
        update_local_update_file();
        download_erase_list();
    }
}

void auto_updater::update_info_remote(QNetworkReply *reply)
{
    qDebug()<<__func__;
    QFile file("update_info_remote.xml");
    if(file.open(QIODevice::WriteOnly)){
        if(update_info_local_["update_info"].unzip_ == "qcompress_file"){
            file.write(qUncompress(reply->readAll()));
        }else{
            file.write(reply->readAll());
        }
        file.close();

        update_info_parser parser;
        update_info_remote_ = parser.read(QCoreApplication::applicationDirPath() +
                                          "/update_info_remote.xml");
        auto it = update_info_remote_.find("update_info");
        if(it != std::end(update_info_remote_)){
            if(it->second.version_ > update_info_local_["update_info"].version_){
                update_info_remote_.erase(it);
                update_info_local_.erase("update_info");
                download_update_contents();
            }else{
                qDebug()<<"nothing to update because version is equal or less";
            }
        }else{
            qDebug()<<"update_info at remote repository do not contain update_info,"
                      "update failed";
            exit_app();
        }
    }else{
        qDebug()<<"Cannot write data into update_info_remote.xml, "
                  "fail to update";
        exit_app();
    }
}

void auto_updater::update_local_update_file()
{
    bool const can_rename = QFile::rename(QCoreApplication::applicationDirPath() +
                                          "/update_info_local.xml",
                                          QCoreApplication::applicationDirPath() +
                                          "/update_info_local_temp.xml");
    if(!QFile::copy(QCoreApplication::applicationDirPath() +
                    "update_info_remote.xml",
                    QCoreApplication::applicationDirPath() +
                    "update_info_local.xml")){
        qDebug()<<"cannot update update_info_local.xml";
    }
    if(can_rename){
        QFile::remove("update_info_local_temp.xml");
    }
}

void auto_updater::update_info_finished()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if(reply){
        guard_delete_later<QNetworkReply> guard(reply);
        if(reply->error() == QNetworkReply::NoError){
            qDebug()<<"start update info remote";
            update_info_remote(reply);
        }else{
            qDebug()<<"cannot update update_info.xml : "<<reply->errorString();
            qDebug()<<"please fix the network issue before update";
            exit_app();
        }
    }
}
