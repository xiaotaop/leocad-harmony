QT = core testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = leocad-d7-files
INCLUDEPATH += common
CONFIG(debug, debug|release):DESTDIR = build/debug
else:DESTDIR = build/release
SOURCES += tests/files/tst_authorized_save.cpp
