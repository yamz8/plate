QT += core gui qml quick quickcontrols2 dbus svg

CONFIG += c++20 release
TARGET = plate
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/filepicker.h \
    src/plate.h \
    src/platepainter.h \
    src/plateview.h \
    src/portalfilepicker.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/plate.cpp \
    src/platepainter.cpp \
    src/plateview.cpp \
    src/portalfilepicker.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
