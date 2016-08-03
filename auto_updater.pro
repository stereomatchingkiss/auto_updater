QT += core network
QT -= gui

INCLUDEPATH += ../

include(../pri/qslog.pri)

CONFIG += c++11

TARGET = auto_updater
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

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
