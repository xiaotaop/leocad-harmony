# D10 performance baseline probe (member C, 2026-11-05).
#
# This file must sit at the repository root to build, because leocad.pro
# resolves its own source list relative to the directory it is included from
# (moving it to /tmp makes every relative resource path in leocad.pro miss).
# scripts/dev/perf-baseline.sh copies both this and perf-baseline.cpp there under
# a run-unique name, builds, runs and removes them again; do not "just qmake it"
# from here.
#
#   scripts/dev/perf-baseline.sh
#
# It replaces the real entry point (qt/qtmain.cpp) with the probe's main() and
# otherwise builds the whole application, so the numbers come from the real
# lcApplication/lcMainWindow code and not from a reimplementation.
include(leocad.pro)

CONFIG += console
CONFIG -= app_bundle
TARGET = leocad-perf

SOURCES -= qt/qtmain.cpp

# The staged copy lives at the repository root under a run-unique name (the
# script passes it here), so two runs on one checkout cannot overwrite or delete
# each other's staging files. Defaults to the plain name for a manual build.
isEmpty(PERF_PROBE_SOURCE): PERF_PROBE_SOURCE = perf-baseline.cpp
SOURCES += $$PERF_PROBE_SOURCE
