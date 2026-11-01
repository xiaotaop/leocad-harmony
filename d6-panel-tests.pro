# D6 integration test: the panel driving the modelling chain.
#
# Must sit next to leocad.pro so that file's relative source paths resolve
# (the D3/D4/D5 projects follow the same convention). Build with:
#   mkdir -p build/d6-panel && cd build/d6-panel
#   qmake ../../d6-panel-tests.pro CONFIG+=debug && make -j4
#
# Or run it with scripts/dev/run-d3-tests.sh panelchain.

include(leocad.pro)

QT += testlib
CONFIG += console testcase
CONFIG -= app_bundle
TARGET = leocad-d6-panel

SOURCES -= qt/qtmain.cpp
SOURCES += tests/ui/d6/tst_parts_panel_chain.cpp
