QT += core gui qml quick quickcontrols2 testlib dbus svg
CONFIG += c++20 testcase
TARGET = plate_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/backend.h \
    ../src/filepicker.h \
    ../src/plate.h \
    ../src/platepainter.h \
    ../src/theme.h \
    ../src/translator.h \
    ../src/templates.h

SOURCES += \
    plate_tests.cpp \
    ../src/backend.cpp \
    ../src/plate.cpp \
    ../src/platepainter.cpp \
    ../src/theme.cpp \
    ../src/translator.cpp \
    ../src/templates.cpp

RESOURCES += ../src/resources.qrc
