# Screenshot harness for the D7 localisation evidence.
#
# Builds the real application against the real main window and saves what the
# parts panel, the options menu and the empty state look like in Chinese at two
# window sizes. See tools/d7/README.md for why this exists as a tool rather than
# a unit test.
#
#   mkdir -p build/d7-shots && cd build/d7-shots
#   qmake ../../d7-shots.pro CONFIG+=debug && make -j4

include(leocad.pro)

QT += testlib
CONFIG += console
CONFIG -= app_bundle
TARGET = leocad-d7-shots

SOURCES -= qt/qtmain.cpp
SOURCES += tools/d7/tst_d7_shots.cpp
