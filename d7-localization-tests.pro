# D7 integration test: Chinese resources and the menu width they need.
#
# Must sit next to leocad.pro so that file's relative source paths resolve
# (the D3/D4/D5/D6 projects follow the same convention). Build with:
#   mkdir -p build/d7-localization && cd build/d7-localization
#   qmake ../../d7-localization-tests.pro CONFIG+=debug && make -j4
#
# Or run it with scripts/dev/run-d3-tests.sh localization.

include(leocad.pro)

QT += testlib
CONFIG += console testcase
CONFIG -= app_bundle
TARGET = leocad-d7-localization

SOURCES -= qt/qtmain.cpp
SOURCES += tests/ui/d7/tst_localization.cpp
