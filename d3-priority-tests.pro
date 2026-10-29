# D3 integration test: priority.
#
# Must sit next to leocad.pro so that file's relative source paths resolve
# (member-b-tests.pro follows the same convention). Build with:
#   mkdir -p build/d3-priority && cd build/d3-priority
#   qmake ../../d3-priority-tests.pro CONFIG+=debug && make -j4
#
# Or run all of them with scripts/dev/run-d3-tests.sh.

include(leocad.pro)

QT += testlib
CONFIG += console testcase
CONFIG -= app_bundle
TARGET = leocad-d3-priority

SOURCES -= qt/qtmain.cpp
SOURCES += tests/library/d3/tst_library_priority.cpp
