# Font-coverage probe for the D7 Chinese build. Kept out of leocad.pro on
# purpose: it links no LeoCAD code at all, so pulling it in would only add a
# target that the application never uses. Build it on its own when the question
# is "is this a font problem or a translation problem".
include(leocad.pro)

QT += gui widgets
CONFIG += console
CONFIG -= app_bundle
CONFIG += warn_on

TARGET = leocad-d7-fonts

# No LeoCAD sources are needed, and keeping the include above means the same Qt
# version and the same compiler flags as the application are used.
SOURCES -= qt/qtmain.cpp
SOURCES += tools/d7/tst_d7_fonts.cpp
