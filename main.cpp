#include "auto_updater.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("auto_update");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("auto update the contents of specify application");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption app_to_start(QStringList() << "a" << "app_to_start",
                                    QCoreApplication::translate("main", "App to start update update finished"),
                                    "app");
    parser.addOption(app_to_start);
    parser.process(a);

    if(!parser.isSet(app_to_start)){
        qDebug()<<"Has not specify app to start, auto_update will "
                  "not start the app after update finished";
    }

    auto_updater au(parser.value(app_to_start));
    au.start();

    return a.exec();
}
