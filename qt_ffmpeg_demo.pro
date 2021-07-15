QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    decoder.cpp \
    main.cpp \
    main_window.cpp

HEADERS += \
    decoder.h \
    main_window.h

FORMS += \
    main_window.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

INCLUDEPATH += $$PWD/include_ffmpeg
DEPENDPATH += $$PWD/include_ffmpeg

win32:{
LIBS += -L$$PWD/lib_windows/ -lavdevice \
                            -lavfilter \
                            -lswscale \
                            -lavformat \
                            -lavcodec \
                            -lswresample \
                            -lavutil

INCLUDEPATH += $$PWD/lib_windows
DEPENDPATH += $$PWD/lib_windows
}
