#include "update_info_parser.hpp"

#include <QDebug>
#include <QFile>

#include <iterator>

update_info_parser::update_info_parser()
{

}

std::map<QString, update_info>
update_info_parser::read(QString const &file_name)
{
    QFile file(file_name);
    if(file.open(QIODevice::ReadOnly)){
        reader_.setDevice(&file);
        reader_.readNextStartElement(); //read tag Installer
        reader_.readNextStartElement(); //read tag RemoteRepositories
        if(reader_.readNextStartElement()){
            info_.clear();
            process_repository();
            return info_;
        }else{
            qDebug()<<"start element tag should be \"Repository\"";
        }
    }else{
        qDebug()<<"cannot open config.xml";
    }

    qDebug()<<"parse end";

    return {};
}

void update_info_parser::process_repository()
{
    if(reader_.isEndDocument()){
        qDebug()<<"this is end document";
        return;
    }

    if(reader_.tokenType() == QXmlStreamReader::Invalid ||
             reader_.name() != "Repository"){
        qDebug()<<"this is invalid token or the "
                  "start element != \"Repository\"";
        return;
    }    

    update_info info;
    QString display_name;
    std::map<QString, QString*> const mapper
    {
        {"Url", &info.url_},
        {"Version", &info.version_},        
        {"Unzip", &info.unzip_},
        {"CopyTo", &info.copy_to_},
        {"DisplayName", &display_name}
    };
    while(reader_.readNextStartElement()){
        QString const name = reader_.name().toString();
        auto it = mapper.find(name);
        QString *const target = it != std::end(mapper) ? it->second : nullptr;
        reader_.readNext();
        if(target != nullptr){
            *target = reader_.text().toString();
        }
        if(!reader_.text().isEmpty()){
            reader_.skipCurrentElement();
        }
    }
    info.display_name_ = display_name;
    info_.insert(std::make_pair(display_name, info));


    if(reader_.isEndElement() &&
            reader_.name() == "Repository"){        
        reader_.readNext();
        reader_.readNextStartElement();
        process_repository();
    }
}
