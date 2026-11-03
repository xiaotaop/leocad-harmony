#!/usr/bin/env bash
# Build and run the D3 integration tests.
#
# Why a script: four separate qmake projects, each needing its own shadow build
# directory, plus one generated fixture set. Doing it by hand is where the review
# found the tests were never actually running in CI.
#
# Usage:
#   scripts/dev/run-d3-tests.sh           # build and run every suite
#   scripts/dev/run-d3-tests.sh archive   # one of: archive priority menu modellist
#                                         #         panel preferences theme chain
#                                         #         localization files
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
    SUITES=(archive priority menu modellist panel preferences theme chain localization files)
fi

FAILED=0

for SUITE in "${SUITES[@]}"; do
    # The D4 panel suite lives beside the D3 ones but is not part of that
    # milestone, so it keeps its own project name rather than being numbered D3.
    case "$SUITE" in
    panel)       PROJECT="d4-${SUITE}-tests.pro"; TARGET="leocad-d4-${SUITE}" ;;
    preferences|theme) PROJECT="d5-${SUITE}-tests.pro"; TARGET="leocad-d5-${SUITE}" ;;
    chain)       PROJECT="d6-${SUITE}-tests.pro"; TARGET="leocad-d6-${SUITE}" ;;
    localization) PROJECT="d7-${SUITE}-tests.pro"; TARGET="leocad-d7-${SUITE}" ;;
    files)       PROJECT="d7-files-tests.pro"; TARGET="leocad-d7-files" ;;
    *)           PROJECT="d3-${SUITE}-tests.pro"; TARGET="leocad-d3-${SUITE}" ;;
    esac

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

    BINARY="$BUILD/build/debug/$TARGET"
    if [ ! -x "$BINARY" ]; then
        echo "FAIL $SUITE: binary not produced" >&2
        FAILED=1
        continue
    fi

    echo "== $SUITE"
    if QT_QPA_PLATFORM=offscreen "$BINARY" > "$BUILD/test-results.txt" 2>&1; then
        STATUS=0
    else
        STATUS=$?
        FAILED=1
    fi

    # Show the lines that explain the result, not the last three of it. The
    # summary line comes last, so "tail -3" cut the FAIL! line above it and a
    # failure printed "12 passed, 1 failed" with no clue which test, or why.
    # One archive failure was lost exactly that way on D8: the next run rebuilds
    # $BUILD from scratch, so the only copy of the reason disappeared with it.
    grep -E '^(FAIL!|QFATAL|Totals)' "$BUILD/test-results.txt" || tail -3 "$BUILD/test-results.txt"

    # Keep the result outside $BUILD, which the next run deletes, so a failure
    # that does not reproduce can still be read afterwards.
    #
    # A passing run must NOT erase a previous failure: an earlier version copied
    # every run to one file, so "archive failed, then passed on the retry" left
    # only "13 passed" behind and the reason was lost again — the very thing the
    # comment above claimed to prevent. The script keeps last-run (whatever
    # happened) and last-failure (updated only on failure) separate, and is
    # exercised directly by scripts/library/test_failure_log_retention.py.
    bash "$REPO_ROOT/scripts/dev/preserve-test-log.sh" "$SUITE" "$STATUS" "$BUILD/test-results.txt"

    # QtTest exits non-zero on failure, but be explicit: a suite that printed
    # "FAIL!" must not pass just because the process exited 0.
    if grep -q '^FAIL!' "$BUILD/test-results.txt"; then
        FAILED=1
    fi
    if [ "$STATUS" -ne 0 ] && ! grep -q '^FAIL!' "$BUILD/test-results.txt"; then
        echo "   (exit $STATUS without a FAIL! line: crash or platform error)"
    fi
done

exit "$FAILED"
