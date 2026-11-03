#!/usr/bin/env bash
# Preserve a test suite's result log so it can still be read after the build
# directory is thrown away.
#
# Why this is a separate script: the retention rule is the part that was wrong
# before, and a rule inside a 20-minute build loop cannot be regression-tested.
# The runner calls this once per suite; scripts/library/test_failure_log_retention.py
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
#   preserve-test-log.sh <suite> <exit-status> <result-file>
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

if [ $# -ne 3 ]; then
    echo "usage: preserve-test-log.sh <suite> <exit-status> <result-file>" >&2
    exit 2
fi

SUITE="$1"
STATUS="$2"
RESULT_FILE="$3"

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
if ! mkdir -p "$LOG_ROOT/build/last-run"; then
    warn "cannot create $LOG_ROOT/build/last-run; no log preserved for $SUITE"
    exit 0
fi
if ! cp "$RESULT_FILE" "$LOG_ROOT/build/last-run/d3-$SUITE-results.txt"; then
    warn "cannot write the last-run copy for $SUITE; no log preserved"
    exit 0
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
    if ! {
        printf 'suite=%s\n' "$SUITE"
        printf 'exit_status=%s\n' "$STATUS"
        printf 'captured=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
        printf 'failed_tests:\n'
        grep -E '^FAIL!' "$RESULT_FILE" || printf '  (exit status only, no FAIL! line: crash or platform error)\n'
    } > "$FAIL_DIR/d3-$SUITE-meta.txt"; then
        warn "cannot write $FAIL_DIR/d3-$SUITE-meta.txt"
        exit 0
    fi
    echo "   preserved failure log: build/last-failure/d3-$SUITE-results.txt" >&2
fi

exit 0
