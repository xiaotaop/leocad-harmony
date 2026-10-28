#!/usr/bin/env bash
# Build and run the D3 integration tests.
#
# Why a script: four separate qmake projects, each needing its own shadow build
# directory, plus one generated fixture set. Doing it by hand is where the review
# found the tests were never actually running in CI.
#
# Usage:
#   scripts/dev/run-d3-tests.sh           # build and run all four
#   scripts/dev/run-d3-tests.sh archive   # one of: archive priority menu modellist
#
# Requires: source .devtools/env.sh (or qmake on PATH). Exits non-zero if any
# suite fails, so CI can gate on it.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

if ! command -v qmake >/dev/null 2>&1; then
    echo "error: qmake not on PATH; source .devtools/env.sh first" >&2
    exit 1
fi

# The archive suite needs the broken-archive variants; generate them from the
# current release archive so they cannot go stale.
python3 tests/library/d3/prepare_variants.py

SUITES=("$@")
if [ ${#SUITES[@]} -eq 0 ]; then
    SUITES=(archive priority menu modellist)
fi

FAILED=0

for SUITE in "${SUITES[@]}"; do
    PROJECT="d3-${SUITE}-tests.pro"
    if [ ! -f "$PROJECT" ]; then
        echo "error: no such suite: $SUITE ($PROJECT)" >&2
        exit 2
    fi

    BUILD="$REPO_ROOT/build/d3-$SUITE"
    rm -rf "$BUILD"
    mkdir -p "$BUILD"
    (
        cd "$BUILD"
        qmake "$REPO_ROOT/$PROJECT" CONFIG+=debug CONFIG-=debug_and_release >/dev/null
        make -j"${JOBS:-4}" >/dev/null
    )

    BINARY="$BUILD/build/debug/leocad-d3-$SUITE"
    if [ ! -x "$BINARY" ]; then
        echo "FAIL $SUITE: binary not produced" >&2
        FAILED=1
        continue
    fi

    echo "== $SUITE"
    if QT_QPA_PLATFORM=offscreen "$BINARY" 2>&1 | tee "$BUILD/test-results.txt" | tail -3; then
        :
    else
        FAILED=1
    fi

    # QtTest exits non-zero on failure, but be explicit: a suite that printed
    # "FAIL!" must not pass just because the process exited 0.
    if grep -q '^FAIL!' "$BUILD/test-results.txt"; then
        FAILED=1
    fi
done

exit "$FAILED"
