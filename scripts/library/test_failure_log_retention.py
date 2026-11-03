#!/usr/bin/env python3
"""Proves that a passing run cannot erase the previous failure's log.

The review of D8 found the reason an unexplained archive failure was
unreadable: run-d3-tests.sh copied every suite's result to one path,
build/last-run/d3-<suite>-results.txt, with no condition. The sequence that
lost the evidence is short and entirely ordinary:

    run 1: archive fails   -> copy writes the failure
    run 2: archive passes  -> the loop's "rm -rf $BUILD" deletes the build copy,
                              and the copy overwrites the only remaining one

so the file on disk afterwards says "13 passed" and the FAIL! line is gone. The
comment in the runner claimed the opposite, which is how the bug survived review.

This test drives scripts/dev/preserve-test-log.sh — the same code the runner
calls — through that exact sequence and asserts the failure text is still
readable at the end. It runs in a temporary root via PRESERVE_LOG_ROOT, so it
never touches build/ and does not need Qt, a compiler, or a completed suite: the
retention rule is checkable in milliseconds, which is why it is checked here
rather than only on a 20-minute full run.

What this does NOT establish: that a real suite's failure produces a FAIL! line,
or that the runner passes the right arguments. Those are covered by running the
suites; this file is about whether the evidence survives to be read.

Run with:
    python3 scripts/library/test_failure_log_retention.py
"""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SCRIPT = REPO / "scripts" / "dev" / "preserve-test-log.sh"

FAILED_RUN = """********* Start testing of ArchiveTest *********
PASS   : ArchiveTest::goodArchiveLoads()
FAIL!  : ArchiveTest::truncatedArchiveIsRejected() Compared values are not the same
   Actual   (message): "opened"
   Expected (message): "rejected"
   Loc: [tst_archive.cpp(214)]
Totals: 12 passed, 1 failed, 0 skipped, 0 blacklisted
********* Finished testing of ArchiveTest *********
"""

PASSED_RUN = """********* Start testing of ArchiveTest *********
PASS   : ArchiveTest::goodArchiveLoads()
PASS   : ArchiveTest::truncatedArchiveIsRejected()
Totals: 13 passed, 0 failed, 0 skipped, 0 blacklisted
********* Finished testing of ArchiveTest *********
"""

SUITE = "archive"


class FailureLogRetention(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix="retention-"))
        self.addCleanup(shutil.rmtree, self.root, True)

    def preserve(self, text: str, status: int) -> None:
        """Run the real script against a result file, as the runner does."""
        result = self.root / "test-results.txt"
        result.write_text(text, encoding="utf-8")
        env = dict(os.environ, PRESERVE_LOG_ROOT=str(self.root))
        subprocess.run(
            ["bash", str(SCRIPT), SUITE, str(status), str(result)],
            check=True,
            env=env,
            capture_output=True,
        )

    def test_a_passing_rerun_keeps_the_previous_failure_readable(self) -> None:
        """The exact sequence that lost the D8 evidence, in order."""
        self.preserve(FAILED_RUN, 1)
        # The next run deletes the build directory; the preserved copy must be
        # enough on its own, so model that by dropping the source result file.
        (self.root / "test-results.txt").unlink()

        self.preserve(PASSED_RUN, 0)

        failure_log = self.root / "build" / "last-failure" / f"d3-{SUITE}-results.txt"
        self.assertTrue(failure_log.exists(), "the failure log was deleted by a passing run")
        text = failure_log.read_text(encoding="utf-8")
        self.assertIn("truncatedArchiveIsRejected", text)
        self.assertIn("tst_archive.cpp(214)", text)
        self.assertIn("12 passed, 1 failed", text)
        self.assertNotIn("13 passed", text, "the passing run overwrote the failure log")

    def test_last_run_tracks_the_most_recent_result(self) -> None:
        """last-run is the opposite on purpose: it always follows the latest run."""
        self.preserve(FAILED_RUN, 1)
        self.preserve(PASSED_RUN, 0)

        last_run = self.root / "build" / "last-run" / f"d3-{SUITE}-results.txt"
        self.assertIn("13 passed", last_run.read_text(encoding="utf-8"))

    def test_the_failure_copy_records_when_and_why(self) -> None:
        """A log without its date cannot be matched to the run that produced it."""
        self.preserve(FAILED_RUN, 1)

        meta = self.root / "build" / "last-failure" / f"d3-{SUITE}-meta.txt"
        text = meta.read_text(encoding="utf-8")
        self.assertIn(f"suite={SUITE}", text)
        self.assertIn("captured=", text)
        self.assertIn("truncatedArchiveIsRejected", text)

    def test_a_crash_without_fail_line_is_still_preserved(self) -> None:
        """Exit status alone marks a failure: a crash prints no FAIL! line."""
        self.preserve("********* Start testing *********\n", 134)

        failure_log = self.root / "build" / "last-failure" / f"d3-{SUITE}-results.txt"
        self.assertTrue(failure_log.exists(), "exit status only must still be a failure")
        meta = self.root / "build" / "last-failure" / f"d3-{SUITE}-meta.txt"
        self.assertIn("exit_status=134", meta.read_text(encoding="utf-8"))

    def test_passing_runs_leave_no_failure_record(self) -> None:
        """No phantom failure: green from the start must not create one."""
        self.preserve(PASSED_RUN, 0)
        self.assertFalse((self.root / "build" / "last-failure").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
