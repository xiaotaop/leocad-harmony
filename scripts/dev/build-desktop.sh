#!/usr/bin/env bash
# Build the desktop edition of LeoCAD for local verification.
#
# Member C owns UI, parts-library and localisation code, so those changes must be
# compiled and looked at before they are proposed — a pull request with no build
# result is not reviewable (see CONTRIBUTING.md).
#
# Usage:
#   scripts/dev/build-desktop.sh            # incremental debug build
#   scripts/dev/build-desktop.sh --clean    # wipe build dir first
#
# Requires: scripts/dev/install-qt-desktop.sh has been run.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

if [ ! -f .devtools/env.sh ]; then
    echo "error: .devtools/env.sh missing; run scripts/dev/install-qt-desktop.sh" >&2
    exit 1
fi
# shellcheck disable=SC1091
source .devtools/env.sh

BUILD_DIR="$REPO_ROOT/build/desktop"
if [ "${1:-}" = "--clean" ]; then
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

# qmake writes into build/debug or build/release depending on CONFIG; a shadow
# build directory keeps generated ui_*.h and moc files out of the source tree.
cd "$BUILD_DIR"
qmake "$REPO_ROOT/leocad.pro" CONFIG+=debug "$@"

# This machine has 7 GB of RAM. Compiling 32 translation units in parallel (one
# per core) drives it into swap: individual compilers stall for many minutes
# without finishing. Cap the job count at a level that fits in memory; override
# with JOBS=... when running somewhere with more RAM.
JOBS="${JOBS:-4}"
make -j"$JOBS"

echo
echo "binary: $REPO_ROOT/build/debug/leocad"
