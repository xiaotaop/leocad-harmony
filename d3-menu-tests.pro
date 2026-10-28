# Menu-scope tests for the first-release UI trimming.
#
# Separate from d3-tests.pro because QTEST_MAIN opens a real QApplication, which
# needs a platform plugin; the archive validator deliberately does not.
#
#   mkdir -p build/d3menu && cd build/d3menu
#   qmake ../../d3-menu-tests.pro CONFIG+=debug
#   QT_QPA_PLATFORM=offscreen make -j4 && ./build/debug/leocad-d3-menu

include(leocad.pro)

QT += testlib
CONFIG += console testcase
CONFIG -= app_bundle
TARGET = leocad-d3-menu

SOURCES -= qt/qtmain.cpp
SOURCES += tests/library/d3/tst_menu_scope.cpp
