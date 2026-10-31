# D5 UI test: preferences scope.
#
# Must sit next to leocad.pro so that file's relative source paths resolve
# (the D3/D4 projects follow the same convention). Build with:
#   mkdir -p build/d5-preferences && cd build/d5-preferences
#   qmake ../../d5-preferences-tests.pro CONFIG+=debug && make -j4
#
# Or run it with scripts/dev/run-d3-tests.sh preferences.

include(leocad.pro)

QT += testlib
CONFIG += console testcase
CONFIG -= app_bundle
TARGET = leocad-d5-preferences

SOURCES -= qt/qtmain.cpp
SOURCES += tests/ui/d5/tst_preferences_scope.cpp
