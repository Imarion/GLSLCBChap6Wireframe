QT += gui core

CONFIG += c++11

TARGET = Wireframe
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    Wireframe.cpp \
    vbomesh.cpp

HEADERS += \
    Wireframe.h \
    vbomesh.h

OTHER_FILES += \
    vshader.txt \
    fshader.txt \
    gshader.txt

RESOURCES += \
    shaders.qrc

DISTFILES += \
    fshader.txt \
    vshader.txt \
    gshader.txt
