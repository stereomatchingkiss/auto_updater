#ifndef AUTO_UPDATER_H
#define AUTO_UPDATER_H

#include "utility.hpp"

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

class auto_updater : public QObject
{
    Q_OBJECT

public:
    explicit auto_updater(QString const &app_to_start, QObject *parent = nullptr);

    void start();    

private:
    using iter_type = std::map<QString, update_info>::iterator;
    using download_iter_type = std::map<QNetworkReply*, update_info>::iterator;

    std::map<QString, QString> create_file_maps() const;

    void decompress_update_content(update_info const &info,
                                   QNetworkReply *reply);
    void download_erase_list();
    bool download_remote_update_info(QNetworkReply *reply);
    void download_update_contents();
    void download_update_content(update_info local_info,
                                 update_info remote_info);

    void erase_old_contents();
    void exit_app();

    void start_updated_app();

    void update_content();
    void update_local_info_finished();
    void update_local_update_file();
    void update_remote_contents(QNetworkReply *reply);

    QString app_to_start_;        
    QNetworkAccessManager *manager_;
    QString parent_path_;
    std::map<QString, update_info> update_info_local_;
    std::map<QString, update_info> update_info_remote_;    
    //save the info need to write into update_info_local.xml
    //first info is the info of update_info_local.xml,
    //second info is the info of update_info_remote.xml
    //if the content info do not exist in the update_info_local.xml,
    //the version_ of local info will be empty
    std::map<QNetworkReply*, std::pair<update_info, update_info>> update_records_;
};

#endif // AUTO_UPDATER_H
