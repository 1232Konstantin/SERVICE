TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

# бустовский скомпилированная либа контекст
INCLUDEPATH = D:\boost_1_83_0\boost_1_83_0

LIBS += -lWs2_32 -lWSock32 -lstdc++fs

SOURCES += \
        common.cpp \
        main.cpp \
        server.cpp


HEADERS += \
    circlebuffer.h \
    commands.h \
    common.h \
    executorthread.h \
    factory.h \
    priority_queue.h \
    server.h \
    time_objects.h

DISTFILES +=
