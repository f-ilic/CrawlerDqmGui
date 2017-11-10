TEMPLATE = app
CONFIG += console c++14
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp

INCLUDEPATH += $$PWD/dependencies/curl/include/

LIBS += $$PWD/dependencies/curl/lib/libcurl.a  -lssl  -lcrypto -pthread -lz
