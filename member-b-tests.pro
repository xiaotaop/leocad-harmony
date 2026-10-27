# Desktop characterization tests; reuse the locked upstream source list.
include(leocad.pro)
QT += testlib
CONFIG += console testcase
CONFIG -= app_bundle
TARGET = leocad-model-tests
SOURCES -= qt/qtmain.cpp
SOURCES += tests/model/tst_model_io.cpp
DEFINES += LC_DISABLE_UPDATE_CHECK=1
