# D4 integration test: parts panel status.
#
# Must sit next to leocad.pro so that file's relative source paths resolve
# (the D3 projects follow the same convention). Build with:
#   mkdir -p build/d4-panel && cd build/d4-panel
#   qmake ../../d4-panel-tests.pro CONFIG+=debug && make -j4
#
# Or run it with scripts/dev/run-d3-tests.sh panel.

include(leocad.pro)

QT += testlib
CONFIG += console testcase
CONFIG -= app_bundle
TARGET = leocad-d4-panel

SOURCES -= qt/qtmain.cpp
SOURCES += tests/library/d4/tst_parts_panel.cpp
