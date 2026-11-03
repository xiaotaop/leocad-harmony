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

Two rules here are not about retention but about honesty, and both are checked
because the previous version of this file got the first one wrong by omission:
the script decides "failure" from the exit status OR a printed FAIL! line, and
each half needs a test or half of the rule can be deleted with the suite still
green; and the script promises never to turn a passing suite into a failing one,
which "set -e" used to break by aborting the caller when the log root was
unwritable.

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

# Synthetic log bodies. They are NOT a transcript of the real D8 failure — that
# transcript no longer exists, which is the whole reason this file exists. What
# they do carry is the shape the script has to survive: a FAIL! line naming a
# test, a "Loc:" line, and the Totals line underneath it. The names and the
# location are copied from the suite that actually failed so the fixture is at
# least not fictional in its details: ArchiveValidationTest::truncatedArchiveIsRejected
# lives at tests/library/d3/tst_archive_validation.cpp:154.
FAILED_RUN = """********* Start testing of ArchiveValidationTest *********
PASS   : ArchiveValidationTest::goodArchiveIsAccepted()
FAIL!  : ArchiveValidationTest::truncatedArchiveIsRejected() Compared values are not the same
   Actual   (Check.Problem): "opened"
   Expected (Check.Problem): "rejected"
   Loc: [../leocad-harmony/tests/library/d3/tst_archive_validation.cpp(154)]
Totals: 12 passed, 1 failed, 0 skipped, 0 blacklisted
********* Finished testing of ArchiveValidationTest *********
"""

PASSED_RUN = """********* Start testing of ArchiveValidationTest *********
PASS   : ArchiveValidationTest::goodArchiveIsAccepted()
PASS   : ArchiveValidationTest::truncatedArchiveIsRejected()
Totals: 13 passed, 0 failed, 0 skipped, 0 blacklisted
********* Finished testing of ArchiveValidationTest *********
"""

# QtTest prints its summary before it exits, so a FAIL! line followed by a crash
# can reach disk while the process still reports success. The runner keeps a
# belt-and-braces "printed FAIL! is a failure" check for exactly this; a log left
# behind by such a run is the only thing that would explain it afterwards.
PASSED_STATUS_BUT_FAILED = PASSED_RUN + """FAIL!  : ArchiveValidationTest::danglingReferenceIsRejected() Aborted before the summary was updated
   Loc: [../leocad-harmony/tests/library/d3/tst_archive_validation.cpp(145)]
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
        self.assertIn("tst_archive_validation.cpp(154)", text)
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
        self.assertIn("exit_status=1", text)
        self.assertRegex(
            text,
            r"captured=\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z",
            "the timestamp must stay ISO-8601 in UTC; a local-time format is unreadable next to CI logs",
        )
        self.assertIn("failed_tests:", text, "the header is what makes the list skimmable")
        self.assertIn("truncatedArchiveIsRejected", text)

    def test_a_crash_without_fail_line_is_still_preserved(self) -> None:
        """Exit status alone marks a failure: a crash prints no FAIL! line."""
        self.preserve("********* Start testing *********\n", 134)

        failure_log = self.root / "build" / "last-failure" / f"d3-{SUITE}-results.txt"
        self.assertTrue(failure_log.exists(), "exit status only must still be a failure")
        meta = self.root / "build" / "last-failure" / f"d3-{SUITE}-meta.txt"
        self.assertIn("exit_status=134", meta.read_text(encoding="utf-8"))

    def test_a_fail_line_with_exit_zero_is_still_preserved(self) -> None:
        """The other half of the rule: a printed FAIL! outranks a zero exit status.

        Without this case the condition can lose its "|| grep FAIL!" arm and the
        whole file still passes, which is how a rule that two places document
        goes unenforced. Documented in docs/MEMBER_C_D8.md.
        """
        self.preserve(PASSED_STATUS_BUT_FAILED, 0)

        failure_log = self.root / "build" / "last-failure" / f"d3-{SUITE}-results.txt"
        self.assertTrue(
            failure_log.exists(),
            "a suite that printed FAIL! was recorded as a pass",
        )
        self.assertIn("danglingReferenceIsRejected", failure_log.read_text(encoding="utf-8"))
        meta = self.root / "build" / "last-failure" / f"d3-{SUITE}-meta.txt"
        text = meta.read_text(encoding="utf-8")
        self.assertIn("exit_status=0", text)
        self.assertIn("danglingReferenceIsRejected", text)

    def test_passing_runs_leave_no_failure_record(self) -> None:
        """No phantom failure: green from the start must not create one."""
        self.preserve(PASSED_RUN, 0)
        self.assertFalse((self.root / "build" / "last-failure").exists())

    def test_an_unwritable_log_root_does_not_fail_the_suite(self) -> None:
        """The script's own promise: logging never changes a suite's verdict.

        The runner runs under "set -e" and calls this before it records the
        suite, so a non-zero exit here would abandon every remaining suite. The
        failure is simulated with a plain file where a directory is needed, which
        needs no root and fails the same way on any filesystem.
        """
        result = self.root / "test-results.txt"
        result.write_text(FAILED_RUN, encoding="utf-8")
        blocker = self.root / "not-a-directory"
        blocker.write_text("", encoding="utf-8")

        env = dict(os.environ, PRESERVE_LOG_ROOT=str(blocker))
        completed = subprocess.run(
            ["bash", str(SCRIPT), SUITE, "1", str(result)],
            env=env,
            capture_output=True,
            text=True,
        )

        self.assertEqual(
            completed.returncode,
            0,
            f"an unwritable log root must not fail the suite: {completed.stderr}",
        )
        self.assertIn("warning:", completed.stderr, "the lost log must at least be reported")

    def test_a_failed_copy_does_not_fail_the_suite(self) -> None:
        """The same promise for the copy itself, not just for "mkdir".

        Written separately because the directory check above aborts first: with
        only that case present, "cp" could be left bare under "set -e" and the
        suite would still pass. A dangling symlink is the portable way to make
        the destination unwritable without depending on the test user's rights.
        """
        result = self.root / "test-results.txt"
        result.write_text(FAILED_RUN, encoding="utf-8")
        last_run = self.root / "build" / "last-run"
        last_run.mkdir(parents=True)
        (last_run / f"d3-{SUITE}-results.txt").symlink_to("/nonexistent/dangling-target")

        env = dict(os.environ, PRESERVE_LOG_ROOT=str(self.root))
        completed = subprocess.run(
            ["bash", str(SCRIPT), SUITE, "1", str(result)],
            env=env,
            capture_output=True,
            text=True,
        )

        self.assertEqual(
            completed.returncode,
            0,
            f"a failed copy must not fail the suite: {completed.stderr}",
        )
        self.assertIn("warning:", completed.stderr)

    def test_a_failed_meta_write_is_reported_not_leaked(self) -> None:
        """The third write needs its own case, for a shell-semantics reason.

        "if ! { ...; } > file" does not return the redirection's failure: bash
        prints its own message, the "then" branch is skipped, and the script
        carries on as though the meta file had been written. So the guard can
        look right, pass review, and never fire. Here the destination is a
        dangling symlink, which makes the redirect fail while leaving the
        directory writable, and the assertion is that the promised "warning:"
        appears rather than bash's raw error alone.
        """
        failure_dir = self.root / "build" / "last-failure"
        failure_dir.mkdir(parents=True)
        (failure_dir / f"d3-{SUITE}-meta.txt").symlink_to("/nonexistent/meta-target")

        result = self.root / "test-results.txt"
        result.write_text(FAILED_RUN, encoding="utf-8")
        env = dict(os.environ, PRESERVE_LOG_ROOT=str(self.root))
        completed = subprocess.run(
            ["bash", str(SCRIPT), SUITE, "1", str(result)],
            env=env,
            capture_output=True,
            text=True,
        )

        self.assertEqual(
            completed.returncode,
            0,
            f"a failed meta write must not fail the suite: {completed.stderr}",
        )
        self.assertIn(
            "warning:",
            completed.stderr,
            f"the guard did not fire; bash's raw error is not the promised report: {completed.stderr}",
        )

    def test_an_uncreatable_failure_dir_does_not_fail_the_suite(self) -> None:
        """The last-failure "mkdir" guard, which the last-run cases miss.

        The last-run directory is created first and succeeds here, so this
        reaches the failure branch; a regular file where build/last-failure
        belongs makes "mkdir -p" fail without needing any special rights. If
        the guard were bare, "set -e" would abort the caller and the remaining
        suites would never run — the same defect the header comment promises
        against, on a path nothing was watching.
        """
        result = self.root / "test-results.txt"
        result.write_text(FAILED_RUN, encoding="utf-8")
        build = self.root / "build"
        build.mkdir(parents=True)
        (build / "last-failure").write_text("", encoding="utf-8")

        env = dict(os.environ, PRESERVE_LOG_ROOT=str(self.root))
        completed = subprocess.run(
            ["bash", str(SCRIPT), SUITE, "1", str(result)],
            env=env,
            capture_output=True,
            text=True,
        )

        self.assertEqual(
            completed.returncode,
            0,
            f"an uncreatable failure dir must not fail the suite: {completed.stderr}",
        )
        self.assertIn("warning:", completed.stderr)
        last_run = self.root / "build" / "last-run" / f"d3-{SUITE}-results.txt"
        self.assertTrue(last_run.exists(), "the last-run copy must still be written")

    def test_a_failed_failure_copy_does_not_fail_the_suite(self) -> None:
        """The last-failure "cp" guard, reached only when its "mkdir" succeeds.

        A dangling symlink at the failure-copy path makes the destination
        unwritable while the directory itself is fine, so this exercises the
        copy and not the directory check before it.
        """
        result = self.root / "test-results.txt"
        result.write_text(FAILED_RUN, encoding="utf-8")
        failure_dir = self.root / "build" / "last-failure"
        failure_dir.mkdir(parents=True)
        (failure_dir / f"d3-{SUITE}-results.txt").symlink_to("/nonexistent/failure-target")

        env = dict(os.environ, PRESERVE_LOG_ROOT=str(self.root))
        completed = subprocess.run(
            ["bash", str(SCRIPT), SUITE, "1", str(result)],
            env=env,
            capture_output=True,
            text=True,
        )

        self.assertEqual(
            completed.returncode,
            0,
            f"a failed failure-log copy must not fail the suite: {completed.stderr}",
        )
        self.assertIn("warning:", completed.stderr)

    def test_a_failed_last_run_write_still_records_the_failure(self) -> None:
        """The failure log must not depend on the last-run copy succeeding.

        Both guards used to "exit 0" on failure, and because the last-run pair
        comes first, an unwritable last-run path skipped the failure branch
        entirely: a genuinely failed suite left no failure record at all. That
        is the one outcome this whole file exists to prevent, so the two
        destinations are written independently and this case pins it.
        """
        result = self.root / "test-results.txt"
        result.write_text(FAILED_RUN, encoding="utf-8")
        build = self.root / "build"
        build.mkdir(parents=True)
        (build / "last-run").write_text("", encoding="utf-8")

        env = dict(os.environ, PRESERVE_LOG_ROOT=str(self.root))
        completed = subprocess.run(
            ["bash", str(SCRIPT), SUITE, "1", str(result)],
            env=env,
            capture_output=True,
            text=True,
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("warning:", completed.stderr)
        failure_log = build / "last-failure" / f"d3-{SUITE}-results.txt"
        self.assertTrue(
            failure_log.exists(),
            "an unwritable last-run copy suppressed the failure log",
        )
        self.assertIn("truncatedArchiveIsRejected", failure_log.read_text(encoding="utf-8"))

    def test_a_non_numeric_exit_status_is_rejected_as_a_usage_error(self) -> None:
        """A wrong argument is an error; it must not quietly read as a pass.

        "[ 1x -ne 0 ]" prints "integer expression expected", the test fails, and
        the script would otherwise fall through to the grep and record nothing.
        """
        result = self.root / "test-results.txt"
        result.write_text(FAILED_RUN, encoding="utf-8")

        env = dict(os.environ, PRESERVE_LOG_ROOT=str(self.root))
        completed = subprocess.run(
            ["bash", str(SCRIPT), SUITE, "1x", str(result)],
            env=env,
            capture_output=True,
            text=True,
        )

        self.assertEqual(completed.returncode, 2, "a non-numeric status is a usage error")
        self.assertIn("non-negative integer", completed.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
