#!/usr/bin/env bash
# Preserve a test suite's result log so it can still be read after the build
# directory is thrown away.
#
# Why this is a separate script: the retention rule is the part that was wrong
# before, and a rule inside a 20-minute build loop cannot be regression-tested.
# The runner calls this once per suite — for every outcome, including a build
# that never produced a test binary; scripts/library/test_failure_log_retention.py
# calls it directly to prove that a green run does not erase the previous failure.
#
# Two destinations, deliberately different:
#
#   build/last-run/d3-<suite>-results.txt      the most recent result, whatever it was
#   build/last-failure/d3-<suite>-results.txt  the most recent FAILURE only
#
# The failure copy is written only when the run actually failed, so a later
# passing run cannot overwrite it. That is the whole point: one archive failure
# on D8 was never explained, and the next green run would have replaced the only
# evidence with "13 passed" if both went to the same file.
#
# Usage:
#   preserve-test-log.sh <suite> <exit-status> <result-file> [<failed-stage> [<build-log>]]
#
# <failed-stage> is "test" for a suite that ran, or "qmake" / "make" / "binary"
# for one that never got that far. It is a parameter rather than a third log
# type because the runner is the only thing that knows how far the build got,
# and the retention rule has to stay testable without running a build. The meta
# file records it so a reader can tell a compile failure from a test failure by
# opening one small text file.
#
# <build-log> is the qmake/make output. When it is given and exists it is
# preserved under the same two destinations as the result file, so a failure
# that never reached a test binary still leaves the compiler's own words behind.
#
# Once the arguments are accepted this script exits 0 even if the log root turns
# out to be unwritable. Both reasons are about not losing more than the write
# itself: the runner runs under "set -e", so returning non-zero here would
# abandon every remaining suite, and a suite's verdict must come from the tests,
# never from the logging that follows them. A failed write is reported on stderr
# — loudly, but not fatally. (An earlier version of this comment claimed that
# property while "set -e" quietly broke it: the promise is only worth making if
# the code keeps it, and the test file now checks both halves.)
set -euo pipefail

if [ $# -lt 3 ] || [ $# -gt 5 ]; then
    echo "usage: preserve-test-log.sh <suite> <exit-status> <result-file> [<failed-stage> [<build-log>]]" >&2
    exit 2
fi

SUITE="$1"
STATUS="$2"
RESULT_FILE="$3"
STAGE="${4:-test}"
BUILD_LOG="${5:-}"

# A status that is not a number is a wrong argument, not a failed suite: say so
# instead of letting "[ -ne ]" print "integer expression expected" and then
# silently decide the run passed.
case "$STATUS" in
'' | *[!0-9]*)
    echo "usage: <exit-status> must be a non-negative integer, got: $STATUS" >&2
    exit 2
    ;;
esac

DEFAULT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG_ROOT="${PRESERVE_LOG_ROOT:-$DEFAULT_ROOT}"

warn() {
    echo "warning: $*" >&2
}

if [ ! -f "$RESULT_FILE" ]; then
    echo "warning: no result file to preserve for $SUITE: $RESULT_FILE" >&2
    exit 0
fi

# Neither write below may abort the caller; see the note at the top of the file.
#
# Neither one may skip the failure branch either. These used to "exit 0" on
# failure, which was wrong in the one case that matters: if the last-run copy
# could not be written, a genuinely failed suite went on to record no failure
# log at all — the feature would lose the evidence precisely when the disk was
# already misbehaving. The two destinations are independent, so a problem with
# the first must not prevent the second.
if ! mkdir -p "$LOG_ROOT/build/last-run"; then
    warn "cannot create $LOG_ROOT/build/last-run; no last-run log preserved for $SUITE"
elif ! cp "$RESULT_FILE" "$LOG_ROOT/build/last-run/d3-$SUITE-results.txt"; then
    warn "cannot write the last-run copy for $SUITE; no last-run log preserved"
fi

# The build log follows the result to the same place. It is copied only when the
# caller supplied one and it exists, and an absent build log never removes a
# previous one: a suite whose build worked still leaves the last build's output
# readable rather than trading it for nothing. Any write failure is a warning
# for the same reason as above — this must not decide a suite's verdict, and it
# must not stop the failure branch below from running.
if [ -n "$BUILD_LOG" ] && [ -f "$BUILD_LOG" ]; then
    if ! cp "$BUILD_LOG" "$LOG_ROOT/build/last-run/d3-$SUITE-build.log"; then
        warn "cannot write the last-run build log for $SUITE; the build output was not preserved"
    fi
fi

# A suite failed if it told us so, whatever the exit status claimed. QtTest exits
# non-zero on failure, but the FAIL! line is the thing that names the test, and a
# FAIL! printed just before a crash can leave the process exit status at 0. Both
# halves of this condition are load-bearing and both are covered by tests.
if [ "$STATUS" -ne 0 ] || grep -q '^FAIL!' "$RESULT_FILE"; then
    FAIL_DIR="$LOG_ROOT/build/last-failure"
    if ! mkdir -p "$FAIL_DIR"; then
        warn "cannot create $FAIL_DIR; the failure log for $SUITE was not preserved"
        exit 0
    fi
    if ! cp "$RESULT_FILE" "$FAIL_DIR/d3-$SUITE-results.txt"; then
        warn "cannot write the failure log for $SUITE"
        exit 0
    fi
    # A build failure has no test output at all; without this copy the failure
    # directory would hold a result file that says "the build failed" and
    # nothing that says why.
    if [ -n "$BUILD_LOG" ] && [ -f "$BUILD_LOG" ]; then
        if ! cp "$BUILD_LOG" "$FAIL_DIR/d3-$SUITE-build.log"; then
            warn "cannot write the failure build log for $SUITE; the build output was not preserved"
            exit 0
        fi
    fi
    # Build the body first, then write it with a single simple command. This
    # shape is not a style choice: "if ! { ... } > file" does NOT return the
    # redirection failure — bash prints its own error, the "then" branch is
    # skipped, and execution continues as if the write had succeeded. The
    # simple command below does report it, so the promise of a "warning:" plus
    # a zero exit holds here too.
    META="$(
        printf 'suite=%s\n' "$SUITE"
        printf 'exit_status=%s\n' "$STATUS"
        printf 'stage=%s\n' "$STAGE"
        printf 'captured=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
        printf 'failed_tests:\n'
        # A build failure has no FAIL! lines because no test ever ran; saying
        # "crash or platform error" there would mislabel the one case this
        # stage field exists to distinguish.
        if [ "$STAGE" = test ]; then
            grep -E '^FAIL!' "$RESULT_FILE" || printf '  (exit status only, no FAIL! line: crash or platform error)\n'
        else
            printf '  (none: build failed during %s, so no test ran)\n' "$STAGE"
        fi
    )"
    if ! printf '%s\n' "$META" > "$FAIL_DIR/d3-$SUITE-meta.txt"; then
        warn "cannot write $FAIL_DIR/d3-$SUITE-meta.txt"
        exit 0
    fi
    echo "   preserved failure log: build/last-failure/d3-$SUITE-results.txt" >&2
fi

exit 0
