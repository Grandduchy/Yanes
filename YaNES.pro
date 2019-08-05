TEMPLATE = app
QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
TARGET = app
DEFINES += QT_DEPRECATED_WARNINGS
CONFIG += c++14

SOURCES += \
        src/Cpu6502.cpp \
        src/GamePak.cpp \
        src/Memory.cpp \
        src/NES.cpp \
        src/Ppu.cpp \
        src/debugview.cpp \
        src/main.cpp \
        src/mainwindow.cpp \
        src/drawwidget.cpp

INCLUDEPATH += include/

HEADERS += \
    include/Cpu6502.hpp \
    include/GamePak.hpp \
    include/Memory.hpp \
    include/NES.hpp \
    include/Ppu.hpp \
    include/debugview.h \
    include/drawwidget.hpp \
    include/mainwindow.h

FORMS += \
    forms/debugview.ui \
    forms/mainwindow.ui \
    forms/drawwidget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


