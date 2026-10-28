# Focused tests for the D3 startup integration.
#
# Two independent executables, because each has its own main() (QTEST_APPLESS_MAIN
# vs QTEST_MAIN) and they need different runtime requirements:
#
#   leocad-d3-archive   the archive validator; no GUI, but links the app sources
#   leocad-d3-menu      the menu scope; needs a platform plugin (offscreen is fine)
#
# Build one target at a time:
#   mkdir -p build/d3 && cd build/d3
#   qmake ../../d3-tests.pro CONFIG+=debug
#   make leocad-d3-archive -j4 && ./build/debug/leocad-d3-archive
#
# Both reuse leocad.pro's source list so the code under test is compiled exactly
# as the shipped application compiles it.

include(leocad.pro)

QT += testlib
CONFIG += console testcase
CONFIG -= app_bundle

SOURCES -= qt/qtmain.cpp
APP_SOURCES = $$SOURCES

TEMPLATE = app
TARGET = leocad-d3-archive
SOURCES = $$APP_SOURCES tests/library/d3/tst_archive_validation.cpp
