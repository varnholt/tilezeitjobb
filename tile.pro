#-------------------------------------------------
#
# Project created by QtCreator 2016-03-10T19:46:55
#
#-------------------------------------------------

QT       += core gui opengl


win32 {
   LIBS += opengl32.lib
}


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = tile
TEMPLATE = app

MOC_DIR = .moc
OBJECTS_DIR = .obj

SOURCES += \
   src/arraywidget.cpp \
   src/blockattributewidget.cpp \
   src/colorreduction.cpp \
   src/dithercolor.cpp \
   src/keyboardeventfilter.cpp \
   src/layer.cpp \
   src/layerstack.cpp \
   src/main.cpp\
   src/mainwindow.cpp \
   src/nametablelayer.cpp \
   src/nespalette.cpp \
   src/nespalettewidget.cpp \
   src/overviewwidget.cpp \
   src/palette.cpp \
   src/paltestwidget.cpp \
   src/previewwidget.cpp \
   src/project.cpp \
   src/settings.cpp \
   src/sharedglcontext.cpp \
   src/spritelayer.cpp \
   src/spritelayergenerator.cpp \
   src/spritewidget.cpp \
   src/subpaletteitem.cpp \
   src/subpalettewidget.cpp \
   src/tile.cpp \
   src/tilegenerator.cpp \
   src/tilegeneratorwidget.cpp \
   src/toolbox.cpp \
   src/toolcolorpicker.cpp \
   src/tool.cpp \
   src/toolerase.cpp \
   src/toolmove.cpp \
   src/toolpalettebrush.cpp \
   src/toolpalettesetup.cpp \
   src/toolpixelbrush.cpp \
   src/toolpixelpencil.cpp \
   src/toolsprite.cpp

HEADERS += \
   src/arraywidget.h \
   src/blockattributewidget.h \
   src/colorreduction.h \
   src/constants.h \
   src/dithercolor.h \
   src/keyboardeventfilter.h \
   src/layer.h \
   src/layerstack.h \
   src/mainwindow.h \
   src/nametablelayer.h \
   src/nespalette.h \
   src/nespalettewidget.h \
   src/overviewwidget.h \
   src/palette.h \
   src/paltestwidget.h \
   src/previewwidget.h \
   src/project.h \
   src/settings.h \
   src/sharedglcontext.h \
   src/spritelayergenerator.h \
   src/spritelayer.h \
   src/spritewidget.h \
   src/subpaletteitem.h \
   src/subpalettewidget.h \
   src/tilegenerator.h \
   src/tilegeneratorwidget.h \
   src/tile.h \
   src/toolbox.h \
   src/toolcolorpicker.h \
   src/toolerase.h \
   src/tool.h \
   src/toolmove.h \
   src/toolpalettebrush.h \
   src/toolpalettesetup.h \
   src/toolpixelbrush.h \
   src/toolpixelpencil.h \
   src/toolsprite.h

FORMS += \
   src/mainwindow.ui \
    src/tilegeneratorwidget.ui

INCLUDEPATH += src

RESOURCES += \
    ressources.qrc


