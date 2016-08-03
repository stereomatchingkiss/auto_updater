#include "auto_updater.hpp"

#include <QsLog.h>
#include <QsLogDest.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>

void setup_logger(QString const &app_dir_path);

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("auto_update");
    QCoreApplication::setApplicationVersion("1.0");

    setup_logger(a.applicationDirPath());

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
        QLOG_WARN()<<"Has not specify app to start, auto_update will "
                     "not start the app after update finished";
    }

    auto_updater au(parser.value(app_to_start));
    au.start();

    return a.exec();
}

void logFunction(const QString &message, QsLogging::Level level)
{
    qDebug() << "From log function: " << qPrintable(message)
             << " " << static_cast<int>(level);
}

void setup_logger(QString const &app_dir_path)
{
    using namespace QsLogging;

    Logger& logger = Logger::instance();
    logger.setLoggingLevel(QsLogging::TraceLevel);
    const QString sLogPath(QDir(app_dir_path).filePath("log.txt"));

    // 2. add two destinations
    DestinationPtr fileDestination(DestinationFactory::MakeFileDestination(
                                       sLogPath, EnableLogRotation,
                                       MaxSizeBytes(1024*1024*10), MaxOldLogCount(2)));
    DestinationPtr debugDestination(DestinationFactory::MakeDebugOutputDestination());
    DestinationPtr functorDestination(DestinationFactory::MakeFunctorDestination(&logFunction));
    logger.addDestination(debugDestination);
    logger.addDestination(fileDestination);
    logger.addDestination(functorDestination);
}
