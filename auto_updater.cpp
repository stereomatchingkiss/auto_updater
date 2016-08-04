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

#include <iostream>
#include <iterator>

auto_updater::auto_updater(QString const &app_to_start, QObject *parent) :
    QObject(parent),
    app_to_start_(app_to_start),
    can_update_remote_contents_(true),
    manager_(new QNetworkAccessManager(this))
{
    QDir dir(QCoreApplication::applicationDirPath());
    if(dir.cdUp()){
        parent_path_ = dir.absolutePath();
    }
}

void auto_updater::check_need_to_update()
{
    can_update_remote_contents_ = false;
    prepare_download_info();
}

void auto_updater::start()
{
    can_update_remote_contents_ = true;
    prepare_download_info();
}

bool auto_updater::need_to_update() const
{
    auto const local_it = update_info_local_.find("update_info");
    if(local_it != std::end(update_info_local_)){
        QLOG_INFO()<<__func__<< " : can find local update info";
        auto const remote_it = update_info_remote_.find("update_info");
        if(remote_it != std::end(update_info_remote_)){
            QLOG_INFO()<<__func__<< " : can find remote update info";
            if(local_it->second.version_ < remote_it->second.version_){
                return true;
            }
        }
    }

    return false;
}

void auto_updater::decompress_update_content(update_info const &info,
                                             QNetworkReply *reply)
{
    QString copy_to = info.copy_to_;
    copy_to.replace("$${PARENT}", parent_path_);
    QString const file_name = info.display_name_ + ".zip";
    QString const zip_file = copy_to + "/" + file_name;
    QLOG_INFO()<<"update content path : "<<zip_file;
    QLOG_INFO()<<"copy update content to : "<<copy_to;
    QFile file(zip_file);
    if(file.open(QIODevice::WriteOnly)){
        file.write(reply->readAll());
        file.close();
        if(info.unzip_ == "qcompress_folder"){
            QLOG_INFO()<<"decompress folder : "<<file_name;
            qte::cp::folder_compressor fc;
            fc.decompress_folder(zip_file, copy_to);
        }else if(info.unzip_ == "qcompress_file"){
            QLOG_INFO()<<"decompress folder : "<<file_name;
            qte::cp::decompress(zip_file, copy_to + "/" + info.display_name_);
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

void auto_updater::prepare_download_info()
{
    update_records_.clear();
    QLOG_INFO()<<"start parsing";
    update_info_parser info_parser;
    update_info_local_ = info_parser.read(QCoreApplication::applicationDirPath() +
                                          "/update_info_local.xml");

    auto update_it = update_info_local_.find("update_info");
    if(update_it != std::end(update_info_local_)){
        QLOG_INFO()<<__func__<<" : can find iter of local update info";
        auto *reply = manager_->get(QNetworkRequest(update_it->second.url_));
        connect(reply, &QNetworkReply::finished, this,
                &auto_updater::update_local_info_finished);
        connect(reply, &QNetworkReply::downloadProgress,
                [this](qint64 bytesReceived, qint64 bytesTotal)
        {
            QLOG_INFO()<<QString("Download update_info : %1/%2").arg(bytesReceived).
                         arg(bytesTotal);
        });
    }else{
        QLOG_ERROR()<<"Cannot find update_info_local.xml, update failed";
        exit_app();
    }
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
            auto it = update_records_.find(reply);
            if(it != std::end(update_records_)){
                auto const &remote_info = it->second.second;
                QLOG_INFO()<<"update_content of : "<<remote_info.display_name_;
                decompress_update_content(remote_info, reply);
                QLOG_INFO()<<"update_content "<<remote_info.display_name_<<" finished";
            }
        }else{
            auto it = update_records_.find(reply);
            QString display_name(" ");
            if(it != std::end(update_records_)){
                auto &local_info = it->second.first;
                auto const &remote_info = it->second.second;
                display_name = remote_info.display_name_ + " ";
                if(!local_info.version_.isEmpty()){
                    local_info.version_ = remote_info.version_;
                }
            }
            QLOG_ERROR()<<QString("download%1fail : %2").
                          arg(display_name).arg(reply->errorString());
        }
    }
    download_update_contents();
}

void auto_updater::download_update_content(update_info local_info,
                                           update_info remote_info)
{
    QNetworkReply *reply = manager_->get(QNetworkRequest(remote_info.url_));
    update_records_.insert({reply, {local_info, remote_info}});
    auto const name = remote_info.display_name_;
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
        auto const remote_info = remote_it->second;
        if(local_it == std::end(update_info_local_)){
            QLOG_INFO()<<"local_it == std::end(update_info_local_)";
            update_info_remote_.erase(remote_it);
            download_update_content({}, remote_info);
        }else{
            if(remote_it->second.version_ > local_it->second.version_){
                QLOG_INFO()<<"remote_it->second.version_ > local_it->second.version_";
                auto const local_info = local_it->second;
                update_info_local_.erase(local_it);
                update_info_remote_.erase(remote_it);
                download_update_content(local_info, remote_info);
            }else{
                QLOG_INFO()<<"do not need to update content : "<<remote_it->second.display_name_;
                update_info_local_.erase(local_it);
                update_info_remote_.erase(remote_it);
                download_update_contents();
            }
        }
        QLOG_INFO()<<remote_info.display_name_<<" : erase update info remote";
    }else{
        update_local_update_file();
        download_erase_list();
    }
}

void auto_updater::update_remote_contents(QNetworkReply *reply)
{
    iter_type local_it = update_info_local_.find("update_info");
    iter_type remote_it = update_info_remote_.find("update_info");
    if(remote_it != std::end(update_info_remote_) &&
            local_it != std::end(update_info_local_)){
        if(remote_it->second.version_ > local_it->second.version_){
            update_records_.insert({reply,
                                    {local_it->second, remote_it->second}});
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

bool auto_updater::download_remote_update_info(QNetworkReply *reply)
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

            update_info_parser parser;
            update_info_remote_ = parser.read(QCoreApplication::applicationDirPath() +
                                              "/update_info_remote.xml");

            return true;
        }
    }else{
        QLOG_ERROR()<<"Cannot write data into update_info_remote.xml, "
                      "fail to update";
        exit_app();
    }

    return false;
}

void auto_updater::update_local_update_file()
{
    std::vector<std::pair<update_info, update_info>> info_vec;
    for(auto const &pair : update_records_){
        info_vec.emplace_back(pair.second);
    }
    update_info_parser::write(info_vec,
                              QCoreApplication::applicationDirPath() +
                              "/update_info_local.xml");
}

void auto_updater::update_local_info_finished()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if(reply){
        QLOG_INFO()<<"update local info has valid reply";
        guard_delete_later<QNetworkReply> guard(reply);
        if(reply->error() == QNetworkReply::NoError){
            QLOG_INFO()<<"start update info remote";
            if(download_remote_update_info(reply)){
                if(can_update_remote_contents_){
                    update_remote_contents(reply);
                }else{
                    if(need_to_update()){
                        std::cout<<"need to update";
                        QLOG_INFO()<<"need to update";
                    }else{
                        std::cout<<"nothing to update";
                        QLOG_INFO()<<"nothing to update";                        
                    }
                    exit_app();
                }
            }
        }else{
            QLOG_ERROR()<<"cannot update update_info.xml : "<<reply->errorString();
            QLOG_ERROR()<<"please fix the network issue before update";
            exit_app();
        }
    }else{
        QLOG_ERROR()<<"update local info has invalid reply";
        exit_app();
    }
}
