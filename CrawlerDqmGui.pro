TEMPLATE = app
CONFIG += console c++14
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    dqmguicrawler.cpp

#INCLUDEPATH += /cvmfs/cms.cern.ch/slc6_amd64_gcc630/external/curl/7.52.1/include/
#INCLUDEPATH += /cvmfs/cms.cern.ch/slc6_amd64_gcc630/external/openssl/1.0.2d/include/


# When compiling on vocms.
#LIBS += -L/cvmfs/cms.cern.ch/slc6_amd64_gcc630/external/curl/7.52.1/lib -lcurl      \
#        -L/cvmfs/cms.cern.ch/slc6_amd64_gcc630/external/openssl/1.0.2d/lib -lssl    \
#        -L/cvmfs/cms.cern.ch/slc6_amd64_gcc630/external/openssl/1.0.2d/lib -lcrypto \
#        -pthread                                                                    \
#        -L/cvmfs/cms.cern.ch/slc6_amd64_gcc630/external/zlib-x86_64/1.2.11/lib -lz

# When devloping on local machine with installed libraries.
LIBS += -lcurl -lssl -lcrypto -pthread -lz

HEADERS += \
    dqmguicrawler.h
