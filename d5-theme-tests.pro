# D5 regression: colour theme readability.
#
# Must sit next to leocad.pro so that file's relative source paths resolve
# (the other UI projects follow the same convention). Build with:
#   mkdir -p build/d5-theme && cd build/d5-theme
#   qmake ../../d5-theme-tests.pro CONFIG+=debug && make -j4
#
# Runs with the offscreen platform plugin; it inspects palettes and styles, not
# pixels, so no GPU is needed.

include(leocad.pro)

QT += testlib
CONFIG += console testcase
CONFIG -= app_bundle
TARGET = leocad-d5-theme

SOURCES -= qt/qtmain.cpp
SOURCES += tests/ui/d5/tst_theme_readability.cpp
