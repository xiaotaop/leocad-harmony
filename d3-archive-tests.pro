# D3 integration test: archive.
#
# Must sit next to leocad.pro so that file's relative source paths resolve
# (member-b-tests.pro follows the same convention). Build with:
#   mkdir -p build/d3-archive && cd build/d3-archive
#   qmake ../../d3-archive-tests.pro CONFIG+=debug && make -j4
#
# Or run all of them with scripts/dev/run-d3-tests.sh.

include(leocad.pro)

QT += testlib
CONFIG += console testcase
CONFIG -= app_bundle
TARGET = leocad-d3-archive

SOURCES -= qt/qtmain.cpp
SOURCES += tests/library/d3/tst_archive_validation.cpp
