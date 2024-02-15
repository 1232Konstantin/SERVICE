TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

# бустовский скомпилированная либа контекст
INCLUDEPATH = D:\boost_1_83_0\boost_1_83_0


LIBS += -lWs2_32 -lWSock32 -lstdc++fs

SOURCES += \
        main.cpp

HEADERS += \
    ClientLib/async_client.h \
    ClientLib/circlebuffer.h \
    ClientLib/common.h

DISTFILES += \
    ClientLib/WS2_32.Lib \
    ClientLib/WSock32.Lib

