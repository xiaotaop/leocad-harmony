# Real widget/renderer input integration; independent of the model-only suites.
include(leocad.pro)
QT += testlib
CONFIG += console testcase
CONFIG -= app_bundle
TARGET = leocad-input-tests
SOURCES -= qt/qtmain.cpp
SOURCES += tests/input/tst_view_input.cpp
DEFINES += LC_DISABLE_UPDATE_CHECK=1 LC_DISABLE_NETWORK=1 \
    LC_DISABLE_RENDER_DIALOG=1 LC_DISABLE_BLENDER=1 LC_DISABLE_NON_CORE_FORMATS=1
QT -= network
win32:LIBS -= -lwininet
