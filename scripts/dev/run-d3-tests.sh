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
#
# Evidence, including for runs that never reach a test suite:
#
#   qmake fails      -> the loop used to die here under "set -e", before anything
#   make fails          was preserved. A build that cannot compile is exactly the
#   binary missing      failure someone needs to read afterwards, so all three now
#                       write a result file and hand it to preserve-test-log.sh,
#                       with the qmake/make output kept as build.log beside it.
#
# The retention rule itself is NOT written here. It lives in
# scripts/dev/preserve-test-log.sh, which this script calls once per suite for
# every outcome; scripts/library/test_failure_log_retention.py drives both
# scripts directly. A rule written inside a 20-minute build loop cannot be
# regression-tested — that lesson cost D8 an unexplained archive failure, and the
# build-failure paths were the part still outside the tested code.
#
# Two overrides exist for the same reason — scripts/library/test_failure_log_retention.py
# runs THIS script with a deliberately broken build to prove the evidence paths,
# and a test must not write into, still less delete, the build directories of a
# tree someone else is working in:
#
#   D3_BUILD_ROOT      where the per-suite shadow builds go  (default: <repo>/build)
#   PRESERVE_LOG_ROOT  where the preserved logs go           (default: <repo>/build)
#
# Neither is set by the runner itself.

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

BUILD_ROOT="${D3_BUILD_ROOT:-$REPO_ROOT/build}"

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

    BUILD="$BUILD_ROOT/d3-$SUITE"
    rm -rf "$BUILD"
    mkdir -p "$BUILD"
    BUILD_LOG="$BUILD/build.log"

    # STAGE names how far the run got; it is written into the meta file so a
    # reader can tell "the compiler never finished" from "a test failed" without
    # opening the log. STEP_STATUS is the status that decided the suite.
    STAGE=test
    STEP_STATUS=0
    RESULT="$BUILD/test-results.txt"

    # Build, without letting a failure abort the loop. The old shape ran both
    # commands bare under "set -e", so a broken build ended the whole run before
    # the preserve call further down: no last-run, no last-failure, no log. The
    # output goes to build.log instead of /dev/null for the same reason — the
    # first compile error is the evidence.
    STAGE=qmake
    if ( cd "$BUILD" && qmake "$REPO_ROOT/$PROJECT" CONFIG+=debug CONFIG-=debug_and_release ) > "$BUILD_LOG" 2>&1; then
        STAGE=make
        ( cd "$BUILD" && make -j"${JOBS:-4}" ) >> "$BUILD_LOG" 2>&1 || STEP_STATUS=$?
    else
        STEP_STATUS=$?
    fi

    if [ "$STEP_STATUS" -ne 0 ]; then
        echo "FAIL $SUITE: build failed during $STAGE (exit $STEP_STATUS)" >&2
        FAILED=1
        RESULT_TEXT="BUILD FAILED
suite: $SUITE
failed stage: $STAGE
$STAGE exited $STEP_STATUS
project: $PROJECT
no test ran; the build output is preserved as d3-$SUITE-build.log"
        # A failed write here must not abort the loop either: the suite is
        # already marked failed, and preserve-test-log.sh reports the missing
        # file itself without changing that verdict.
        if ! printf '%s\n' "$RESULT_TEXT" > "$RESULT"; then
            echo "warning: cannot write $RESULT; the $STAGE failure for $SUITE will be reported without a result file" >&2
        fi
    else
        BINARY="$BUILD/build/debug/$TARGET"
        if [ ! -x "$BINARY" ]; then
            # make succeeded and still produced nothing runnable. This path used
            # to print one FAIL line and "continue", leaving no record at all.
            # A status is what makes the record a failure record: passing 0 here
            # would write a result saying the build failed while telling
            # preserve-test-log.sh the suite succeeded.
            STAGE=binary
            STEP_STATUS=1
            if [ -e "$BINARY" ]; then
                DETAIL="binary exists but is not executable: $BINARY"
            else
                DETAIL="binary not produced: $BINARY"
            fi
            echo "FAIL $SUITE: $DETAIL" >&2
            FAILED=1
            RESULT_TEXT="BUILD FAILED
suite: $SUITE
failed stage: $STAGE
$DETAIL
project: $PROJECT
no test ran; the build output is preserved as d3-$SUITE-build.log"
            if ! printf '%s\n' "$RESULT_TEXT" > "$RESULT"; then
                echo "warning: cannot write $RESULT; the $STAGE failure for $SUITE will be reported without a result file" >&2
            fi
        else
            # Back to "test": the build got all the way through, so anything
            # that fails from here on is a test result, not a build failure.
            # Missing this line made every ordinary test failure record
            # "stage=make ... no test ran", which is the opposite of the truth.
            STAGE=test
            echo "== $SUITE"
            if QT_QPA_PLATFORM=offscreen "$BINARY" > "$RESULT" 2>&1; then
                STATUS=0
            else
                STATUS=$?
                FAILED=1
            fi
            STEP_STATUS=$STATUS

            # Show the lines that explain the result, not the last three of it. The
            # summary line comes last, so "tail -3" cut the FAIL! line above it and a
            # failure printed "12 passed, 1 failed" with no clue which test, or why.
            # One archive failure was lost exactly that way on D8: the next run rebuilds
            # $BUILD from scratch, so the only copy of the reason disappeared with it.
            grep -E '^(FAIL!|QFATAL|Totals)' "$RESULT" || tail -3 "$RESULT"

            # QtTest exits non-zero on failure, but be explicit: a suite that printed
            # "FAIL!" must not pass just because the process exited 0.
            if grep -q '^FAIL!' "$RESULT"; then
                FAILED=1
            fi
            if [ "$STATUS" -ne 0 ] && ! grep -q '^FAIL!' "$RESULT"; then
                echo "   (exit $STATUS without a FAIL! line: crash or platform error)"
            fi
        fi
    fi

    # Keep the result outside $BUILD, which the next run deletes, so a failure
    # that does not reproduce can still be read afterwards.
    #
    # A passing run must NOT erase a previous failure: an earlier version copied
    # every run to one file, so "archive failed, then passed on the retry" left
    # only "13 passed" behind and the reason was lost again — the very thing the
    # comment above claimed to prevent. The script keeps last-run (whatever
    # happened) and last-failure (updated only on failure) separate, and is
    # exercised directly by scripts/library/test_failure_log_retention.py.
    #
    # The build log travels with the result, so a run that never got as far as a
    # test binary still leaves the compiler's own words behind.
    bash "$REPO_ROOT/scripts/dev/preserve-test-log.sh" "$SUITE" "$STEP_STATUS" "$RESULT" "$STAGE" "$BUILD_LOG"
done

exit "$FAILED"
