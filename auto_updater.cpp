#include "auto_updater.hpp"
#include "update_info_parser.hpp"

#include <qt_enhance/compressor/file_compressor.hpp>
#include <qt_enhance/compressor/folder_compressor.hpp>

#include <QsLog.h>

#include <QCoreApplication>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QtGlobal>
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
    update_records_.clear();
    QLOG_INFO()<<"start config parse";
    update_info_parser info_parser;
    update_info_local_ = info_parser.read(QCoreApplication::applicationDirPath() +
                                          "/update_info_local.xml");

    auto update_it = update_info_local_.find("update_info");
    if(update_it != std::end(update_info_local_)){
        auto *reply = manager_->get(QNetworkRequest(update_it->second.url_));
        connect(reply, &QNetworkReply::finished, this,
                &auto_updater::update_local_info_finished);
        connect(reply, &QNetworkReply::downloadProgress,
                [this](qint64 bytesReceived, qint64 bytesTotal)
        {
            QLOG_INFO()<<QString("Download update_info : %1/%2").arg(bytesReceived).
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
    QLOG_INFO()<<"update content path : "<<zip_file;
    QLOG_INFO()<<"copy update content to : "<<copy_to;
    QFile file(zip_file);
    if(file.open(QIODevice::WriteOnly)){
        file.write(reply->readAll());
        file.close();
        if(it->second.unzip_ == "qcompress_folder"){
            QLOG_INFO()<<"decompress folder : "<<file_name;
            qte::cp::folder_compressor fc;
            fc.decompress_folder(zip_file, copy_to);
        }else if(it->second.unzip_ == "qcompress_file"){
            QLOG_INFO()<<"decompress folder : "<<file_name;
            qte::cp::decompress(zip_file, copy_to + "/" + it->second.display_name_);
        }
        file.remove();
    }else{
        QLOG_ERROR()<<"Cannot write into file : "<<zip_file;
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
        QLOG_DEBUG()<<"cannot read file_list.txt";
    }

    return file_maps;
}

void auto_updater::erase_old_contents()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if(reply){
        guard_delete_later<QNetworkReply> guard(reply);
        if(reply->error() != QNetworkReply::NoError){
            QLOG_ERROR()<<reply->errorString();
        }else{
            QString const erase_list = reply->readAll();
            QStringList lists = erase_list.split("\n");
            for(auto &list : lists){
                if(!list.isEmpty()){
                    list.replace("$${PARENT}", parent_path_);
                    list = list.simplified();
                    QLOG_DEBUG()<<"list to removed : "<<list;
                    if(QFileInfo(list).isDir()){
                        QDir dir(list);
                        if(!dir.removeRecursively()){
                            QLOG_DEBUG()<<"cannot remove dir : "<<list;
                        }
                    }else{
                        if(!QFile::remove(list)){
                            QLOG_DEBUG()<<"cannot remove file : "<<list;
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
    QLOG_INFO()<<"press any key to exit...";
    exit(0);
}

void auto_updater::start_updated_app()
{
    if(!app_to_start_.isEmpty()){
        QLOG_INFO()<<"app_to_start is "<<app_to_start_;
        app_to_start_.replace("$${PARENT}", parent_path_);
        if(!QProcess::startDetached(app_to_start_)){
            QLOG_DEBUG()<<"cannot start the process : "<<app_to_start_;
            exit_app();
        }
    }else{
        QLOG_INFO()<<"app_to_start is empty";
    }

    exit_app();
}

void auto_updater::download_erase_list()
{
    QLOG_INFO()<<__func__;
    std::map<QString, QString> const file_maps = create_file_maps();
    auto it = file_maps.find("erase_list");
    if(it != std::end(file_maps)){
        QLOG_INFO()<<"erase_list location : "<<it->second;
        auto *reply = manager_->get(QNetworkRequest(it->second));
        connect(reply, &QNetworkReply::finished, this,
                &auto_updater::erase_old_contents);
        connect(reply, &QNetworkReply::downloadProgress,
                [](qint64 bytesReceived, qint64 bytesTotal)
        {
            QLOG_INFO()<<QString("Download erase_list.txt : %1/%2").
                         arg(bytesReceived).arg(bytesTotal);
        });
    }else{
        QLOG_INFO()<<"cannot find erase list";
        start_updated_app();
    }
}

void auto_updater::update_content()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if(reply){
        guard_delete_later<QNetworkReply> guard(reply);
        if(reply->error() == QNetworkReply::NoError){
            download_iter_type it = download_details_.find(reply);
            if(it != std::end(download_details_)){
                QLOG_INFO()<<"update_content of : "<<it->second.display_name_;
                decompress_update_content(it, reply);
                QLOG_INFO()<<"update_content "<<it->second.display_name_<<" finished";
            }
        }else{
            auto it = update_records_.find(reply);
            QString display_name(" ");
            if(it != std::end(update_records_)){
                display_name = it->second.second.display_name_ + " ";
                it->second.second.version_ = it->second.first;
            }
            QLOG_ERROR()<<QString("download%1fail : %2").
                          arg(display_name).arg(reply->errorString());
        }
    }
    download_update_contents();
}

void auto_updater::download_update_content(update_info info, QString local_ver_num)
{
    QNetworkReply *reply = manager_->get(QNetworkRequest(info.url_));
    update_records_.insert({reply, {local_ver_num, info}});
    download_details_.insert({reply, info});
    auto const name = info.display_name_;
    connect(reply, &QNetworkReply::finished, this,
            &auto_updater::update_content);
    connect(reply, &QNetworkReply::downloadProgress,
            [this, name](qint64 bytesReceived, qint64 bytesTotal)
    {
        QLOG_INFO()<<QString("Download %1 : %2/%3").arg(name).
                     arg(bytesReceived).arg(bytesTotal);
    });
}

void auto_updater::download_update_contents()
{        
    if(!update_info_remote_.empty()){
        iter_type remote_it = std::begin(update_info_remote_);
        iter_type local_it = update_info_local_.find(remote_it->first);
        QLOG_INFO()<<"download_update_contents : "<<remote_it->second.display_name_;
        auto const info = remote_it->second;
        if(local_it == std::end(update_info_local_)){
            QLOG_INFO()<<"local_it == std::end(update_info_local_)";
            update_info_remote_.erase(remote_it);
            download_update_content(info, "");
        }else{
            if(remote_it->second.version_ > local_it->second.version_){
                QLOG_INFO()<<"remote_it->second.version_ > local_it->second.version_";
                auto const version = local_it->second.version_;
                update_info_local_.erase(local_it);
                update_info_remote_.erase(remote_it);
                download_update_content(info, version);
            }else{
                QLOG_INFO()<<"do not need to update content : "<<remote_it->second.display_name_;
                update_info_local_.erase(local_it);
                update_info_remote_.erase(remote_it);
                download_update_contents();
            }
        }
        QLOG_INFO()<<info.display_name_<<" : erase update info remote";
    }else{
        update_local_update_file();
        download_erase_list();
    }
}

void auto_updater::update_remote_contents(QNetworkReply *reply,
                                          iter_type local_it)
{
    update_info_parser parser;
    update_info_remote_ = parser.read(QCoreApplication::applicationDirPath() +
                                      "/update_info_remote.xml");
    iter_type remote_it = update_info_remote_.find("update_info");
    if(remote_it != std::end(update_info_remote_)){
        if(remote_it->second.version_ > local_it->second.version_){
            update_records_.insert({reply,
                                    {local_it->second.version_, remote_it->second}});
            update_info_remote_.erase(remote_it);
            update_info_local_.erase(local_it);
            download_update_contents();
        }else{
            QLOG_INFO()<<"nothing to update because version is equal or less";
            start_updated_app();
        }
    }else{
        QLOG_INFO()<<"update_info at remote repository do not contain update_info,"
                     "update failed";
        exit_app();
    }
}

void auto_updater::download_remote_update_info(QNetworkReply *reply)
{
    QLOG_INFO()<<"enter "<<__func__;
    QFile file("update_info_remote.xml");
    if(file.open(QIODevice::WriteOnly)){
        iter_type local_it = update_info_local_.find("update_info");
        if(local_it != std::end(update_info_local_)){
            if(local_it->second.unzip_ == "qcompress_file"){
                file.write(qUncompress(reply->readAll()));
            }else{
                file.write(reply->readAll());
            }
            file.close();

            update_remote_contents(reply, local_it);
        }
    }else{
        QLOG_ERROR()<<"Cannot write data into update_info_remote.xml, "
                      "fail to update";
        exit_app();
    }
}

void auto_updater::update_local_update_file()
{
    if(!QFile::copy(QCoreApplication::applicationDirPath() +
                    "/update_info_remote.xml",
                    QCoreApplication::applicationDirPath() +
                    "/update_info_local.xml")){
        QLOG_ERROR()<<"cannot update update_info_local.xml";
    }
}

void auto_updater::update_local_info_finished()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if(reply){
        guard_delete_later<QNetworkReply> guard(reply);
        if(reply->error() == QNetworkReply::NoError){
            QLOG_INFO()<<"start update info remote";
            download_remote_update_info(reply);
        }else{
            QLOG_ERROR()<<"cannot update update_info.xml : "<<reply->errorString();
            QLOG_ERROR()<<"please fix the network issue before update";
            exit_app();
        }
    }
}
