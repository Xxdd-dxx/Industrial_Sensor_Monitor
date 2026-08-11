QT       += core gui serialport sql charts
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    serialworker.cpp

HEADERS += \
    mainwindow.h \
    serialworker.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    res.qrc