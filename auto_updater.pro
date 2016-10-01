QT += core network
QT -= gui

INCLUDEPATH += ../

include(../pri/qslog.pri)

CONFIG += c++11

TARGET = auto_updater
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

CONFIG(release, debug|release):BUILD_TYPE = release
CONFIG(debug, debug|release):BUILD_TYPE = debug

win32 {
    WINSDK_DIR = C:/Program Files (x86)/Windows Kits/10
    WIN_PWD = $$replace(PWD, /, \\)
    OUT_PWD_WIN = $$replace(OUT_PWD, /, \\)
    message("output : " + $$OUT_PWD_WIN)
    QMAKE_POST_LINK = "$$WINSDK_DIR/bin/x86/mt.exe -manifest $$quote($$WIN_PWD\\$$basename(TARGET).manifest) -outputresource:$$quote($$OUT_PWD_WIN\\${DESTDIR_TARGET};1)"
}

SOURCES += main.cpp \
    auto_updater.cpp \
    update_info_parser.cpp \
    ../qt_enhance/compressor/file_compressor.cpp \
    ../qt_enhance/compressor/folder_compressor.cpp

HEADERS += \
    auto_updater.hpp \
    update_info_parser.hpp \
    utility.hpp \
    ../qt_enhance/compressor/file_compressor.hpp \
    ../qt_enhance/compressor/folder_compressor.hpp
