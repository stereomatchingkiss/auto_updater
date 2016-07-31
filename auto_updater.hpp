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

    void decompress_update_content(download_iter_type it,
                                   QNetworkReply *reply);
    void download_erase_list();
    void download_update_contents();
    void download_update_content(update_info info);

    void erase_old_contents();

    void start_updated_app();

    void update_content();
    void update_info_finished();
    void update_info_remote(QNetworkReply *reply);
    void update_local_update_file();

    QString app_to_start_;
    std::map<QNetworkReply*, update_info> download_details_;
    QNetworkAccessManager *manager_;
    QString parent_path_;
    std::map<QString, update_info> update_info_local_;
    std::map<QString, update_info> update_info_remote_;
};

#endif // AUTO_UPDATER_H
