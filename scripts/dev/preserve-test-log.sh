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
# Exit status is always 0 unless the arguments are wrong: logging must never
# turn a passing suite into a failing one.
set -euo pipefail

if [ $# -ne 3 ]; then
    echo "usage: preserve-test-log.sh <suite> <exit-status> <result-file>" >&2
    exit 2
fi

SUITE="$1"
STATUS="$2"
RESULT_FILE="$3"

DEFAULT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG_ROOT="${PRESERVE_LOG_ROOT:-$DEFAULT_ROOT}"

if [ ! -f "$RESULT_FILE" ]; then
    echo "warning: no result file to preserve for $SUITE: $RESULT_FILE" >&2
    exit 0
fi

mkdir -p "$LOG_ROOT/build/last-run"
cp "$RESULT_FILE" "$LOG_ROOT/build/last-run/d3-$SUITE-results.txt"

# A suite failed if it told us so, whatever the exit status claimed. QtTest exits
# non-zero on failure, but the FAIL! line is the thing that names the test.
if [ "$STATUS" -ne 0 ] || grep -q '^FAIL!' "$RESULT_FILE"; then
    FAIL_DIR="$LOG_ROOT/build/last-failure"
    mkdir -p "$FAIL_DIR"
    cp "$RESULT_FILE" "$FAIL_DIR/d3-$SUITE-results.txt"
    {
        printf 'suite=%s\n' "$SUITE"
        printf 'exit_status=%s\n' "$STATUS"
        printf 'captured=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
        printf 'failed_tests:\n'
        grep -E '^FAIL!' "$RESULT_FILE" || printf '  (exit status only, no FAIL! line: crash or platform error)\n'
    } > "$FAIL_DIR/d3-$SUITE-meta.txt"
    echo "   preserved failure log: build/last-failure/d3-$SUITE-results.txt" >&2
fi

exit 0
