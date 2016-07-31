#ifndef UPDATE_INFO_PARSER_HPP
#define UPDATE_INFO_PARSER_HPP

#include "utility.hpp"

#include <QString>
#include <QXmlStreamReader>

#include <map>
#include <vector>

class update_info_parser
{
public:    
    update_info_parser();

    std::map<QString, update_info> read(QString const &file_name);

private:
    void process_repository();

    std::map<QString, update_info> info_;
    QXmlStreamReader reader_;
};

#endif // UPDATE_INFO_PARSER_HPP
