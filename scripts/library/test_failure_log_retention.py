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

D9 added the second half, which is the same question asked about the paths that
never reach a suite at all. Before it, run-d3-tests.sh ran qmake and make bare
under "set -e" and only called the preserve script afterwards, so a build that
failed ended the whole run with nothing on disk: no last-run, no last-failure,
no compiler output. A missing test binary printed one FAIL line and "continue"d,
which also left no record. Those are the failures most worth reading, so the
last class here drives the real runner — with stub qmake/make on PATH and
PRESERVE_LOG_ROOT pointing at a temporary directory — through each of the three
paths and asserts that a result file, a build log, and a meta file naming the
failed stage all survive.

What this does NOT establish: that a real suite's failure produces a FAIL! line,
or that a real qmake/make behaves like the stubs below. Those are covered by
running the suites; this file is about whether the evidence survives to be read.
The Windows entry point scripts/Run-D3Tests.ps1 is not covered either — it still
keeps no failure log of its own.

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


# ---------------------------------------------------------------------------
# D9: the paths that never reached a test suite.
#
# Everything above calls preserve-test-log.sh with arguments a human chose. That
# proves the retention rule, but it cannot prove the runner ever calls it on a
# failed build — which is exactly where the D8 evidence gap was. So the class
# below runs scripts/dev/run-d3-tests.sh itself, exactly as CI does, but with
# stub qmake/make first on PATH and PRESERVE_LOG_ROOT pointing at a temporary
# directory. No compiler, no Qt, no linking, and no 20-minute build: each test
# finishes in well under a second and touches nothing in the repository's
# build/ tree.

RUNNER = REPO / "scripts" / "dev" / "run-d3-tests.sh"

# A qmake failure is what a missing Qt module looks like, which is what the
# first build on a fresh machine usually prints.
QMAKE_FAILURE = """Project ERROR: Unknown module(s) in QT: testlib
*** qmake failed: exit 1
"""

# A compile error, in the shape gcc reports it. The point of keeping this text
# is that it names the file and line; a result file that only says "the build
# failed" would not.
MAKE_FAILURE = """g++ -c -o build/debug/tst_archive_validation.o ../tests/library/d3/tst_archive_validation.cpp
../tests/library/d3/tst_archive_validation.cpp:154:5: error: 'ArchiveValidator' was not declared in this scope
make: *** [Makefile:1234: build/debug/tst_archive_validation.o] Error 1
"""

STUB_PRELUDE = """#!/usr/bin/env bash
set -u
"""


class EvidenceForBuildFailures(unittest.TestCase):
    """The three ways a suite can fail before a test binary ever runs."""

    def setUp(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix="build-failure-"))
        self.addCleanup(shutil.rmtree, self.root, True)
        self.bin = self.root / "stub-bin"
        self.bin.mkdir()
        self.log_root = self.root / "logs"
        self.build_root = self.root / "build"
        # The runner re-generates the broken-archive fixtures before any suite,
        # which re-zips the 3.8 MB library (~2.7 s) on every invocation. That
        # step is not what this class is about and is already covered by running
        # the suite, so it is stubbed along with qmake and make: six runner
        # invocations would otherwise spend 16 s here for nothing.
        self.stub("python3", "exit 0\n")

    # -- helpers ----------------------------------------------------------

    def stub(self, name: str, body: str) -> None:
        path = self.bin / name
        path.write_text(STUB_PRELUDE + body, encoding="utf-8")
        path.chmod(0o755)

    def run_runner(self, *suites: str) -> subprocess.CompletedProcess:
        env = dict(os.environ)
        env["PATH"] = f"{self.bin}{os.pathsep}{env.get('PATH', '')}"
        env["PRESERVE_LOG_ROOT"] = str(self.log_root)
        env["D3_BUILD_ROOT"] = str(self.build_root)
        return subprocess.run(
            ["bash", str(RUNNER), *suites],
            cwd=str(REPO),
            env=env,
            capture_output=True,
            text=True,
        )

    def preserved(self, destination: str, name: str) -> Path:
        return self.log_root / "build" / destination / name

    def assert_preserved(self, suite: str, stage: str, build_text: str) -> None:
        """Everything a reader needs, in both destinations, for one bad build.

        The build log is the point: before D9 a failed build produced no file at
        all, so this asserts the compiler's own words reached disk, not merely
        that some file with the right name exists.
        """
        for destination in ("last-run", "last-failure"):
            results = self.preserved(destination, f"d3-{suite}-results.txt")
            self.assertTrue(
                results.exists(),
                f"a build failure left no {destination} result for {suite}",
            )
            text = results.read_text(encoding="utf-8")
            self.assertIn(f"suite: {suite}", text)
            self.assertIn(f"failed stage: {stage}", text, "the result file must name the stage")

            build_log = self.preserved(destination, f"d3-{suite}-build.log")
            self.assertTrue(
                build_log.exists(),
                f"the {stage} output was not preserved under {destination}",
            )
            self.assertIn(build_text, build_log.read_text(encoding="utf-8"))

        # Stage lives in the meta file too: it is the small file someone opens
        # first, and "a test failed" vs "the build never compiled" is the one
        # thing it must not get wrong.
        meta = self.preserved("last-failure", f"d3-{suite}-meta.txt")
        meta_text = meta.read_text(encoding="utf-8")
        self.assertIn(f"stage={stage}", meta_text)
        self.assertIn("captured=", meta_text)
        self.assertIn("(none: build failed during " + stage, meta_text)

    # -- a. qmake fails ---------------------------------------------------

    def test_qmake_failure_is_preserved_with_its_output(self) -> None:
        """The path that used to be a one-line "rc=1" and nothing else.

        Before D9 this ran under "set -e" inside a subshell that was never
        guarded: the runner died on the spot, the preserve call below it never
        executed, and neither build/last-run nor build/last-failure was created.
        """
        self.stub("qmake", f"printf '%s' '{QMAKE_FAILURE}' >&2\nexit 1\n")
        self.stub("make", "exit 0\n")

        completed = self.run_runner("priority")

        self.assertNotEqual(completed.returncode, 0, "a failed build must fail the run")
        self.assertIn("FAIL priority: build failed during qmake", completed.stderr)
        self.assert_preserved("priority", "qmake", "Unknown module(s) in QT: testlib")

    # -- b. make fails ----------------------------------------------------

    def test_make_failure_is_preserved_with_its_output(self) -> None:
        """qmake succeeded, the compiler did not. The runner keeps going past it."""
        self.stub("qmake", "exit 0\n")
        self.stub("make", f"printf '%s' '{MAKE_FAILURE}' >&2\nexit 2\n")

        completed = self.run_runner("priority")

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("FAIL priority: build failed during make", completed.stderr)
        self.assert_preserved("priority", "make", "make: *** [Makefile:1234")

    # -- c. the binary is missing ----------------------------------------

    def test_missing_binary_is_preserved_as_its_own_stage(self) -> None:
        """make exits 0 and produces nothing runnable.

        The old runner printed "FAIL <suite>: binary not produced" and
        "continue"d, so the only record was a line of scrollback. It is a
        separate stage from "make" because the build reported success: the next
        person needs to know the target path, not to go looking for a compile
        error that was never printed.
        """
        self.stub("qmake", "exit 0\n")
        self.stub("make", "printf '%s' 'g++ -c -o nothing.o ../common/lc_http.cpp\n'\nexit 0\n")

        completed = self.run_runner("priority")

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("FAIL priority: binary not produced", completed.stderr)
        self.assert_preserved("priority", "binary", "g++ -c -o nothing.o")

    # -- the promises that must survive the new paths ---------------------

    def test_a_bad_build_does_not_stop_the_next_suite(self) -> None:
        """One broken suite must not cost the others their evidence.

        Under the old shape the first failed qmake ended the process, so suites
        after it never even started; this pins that the loop continues and each
        suite gets its own record.
        """
        self.stub("qmake", "printf 'Project ERROR: staged failure\\n' >&2\nexit 1\n")
        self.stub("make", "exit 0\n")

        completed = self.run_runner("priority", "modellist")

        self.assertNotEqual(completed.returncode, 0)
        for suite in ("priority", "modellist"):
            self.assertTrue(
                self.preserved("last-failure", f"d3-{suite}-results.txt").exists(),
                f"{suite} left no failure record; the loop stopped at the first bad build",
            )
            self.assertTrue(self.preserved("last-failure", f"d3-{suite}-build.log").exists())
        self.assertIn("FAIL modellist: build failed during qmake", completed.stderr)

    def test_an_unwritable_log_root_keeps_a_failed_build_failed(self) -> None:
        """The D8 promise, checked on the D9 path instead of the happy one.

        preserve-test-log.sh promises never to change a suite's verdict and never
        to abort the caller. Both are easy to break while adding the build-log
        copy, and breaking them here would turn "the build is broken" into "the
        run is broken in some other way" — or, worse, into a pass.
        """
        self.stub("qmake", "printf 'Project ERROR: nope\\n' >&2\nexit 1\n")
        self.stub("make", "exit 0\n")
        blocker = self.root / "not-a-directory"
        blocker.write_text("", encoding="utf-8")

        env = dict(os.environ)
        env["PATH"] = f"{self.bin}{os.pathsep}{env.get('PATH', '')}"
        env["PRESERVE_LOG_ROOT"] = str(blocker)
        env["D3_BUILD_ROOT"] = str(self.build_root)
        completed = subprocess.run(
            ["bash", str(RUNNER), "priority"],
            cwd=str(REPO),
            env=env,
            capture_output=True,
            text=True,
        )

        self.assertNotEqual(completed.returncode, 0, "the build failed; the run must say so")
        self.assertIn("warning:", completed.stderr, "a lost log must still be reported")
        self.assertNotIn("usage:", completed.stderr)

    def test_a_passing_suite_still_records_only_last_run(self) -> None:
        """The restructured loop must not turn a green suite into a red one.

        Building the evidence paths meant moving code around the success path
        too, and a guard that swallowed a passing run would be a worse bug than
        the gap it fixes. The stub make plants an executable where the runner
        expects it; the stub binary exits 0 with a Totals line.
        """
        self.stub("qmake", "exit 0\n")
        self.stub(
            "make",
            'mkdir -p build/debug\n'
            'printf \'#!/bin/sh\\necho "Totals: 13 passed, 0 failed"\\n\' > build/debug/leocad-d3-priority\n'
            "chmod +x build/debug/leocad-d3-priority\n"
            "exit 0\n",
        )

        completed = self.run_runner("priority")

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertTrue(self.preserved("last-run", "d3-priority-results.txt").exists())
        self.assertFalse(
            (self.log_root / "build" / "last-failure").exists(),
            "a green suite must not leave a failure record",
        )

    def test_a_failing_test_suite_records_stage_test(self) -> None:
        """The ordinary failure path, after the restructure: stage=test, not a build stage."""
        self.stub("qmake", "exit 0\n")
        self.stub(
            "make",
            'mkdir -p build/debug\n'
            'printf \'#!/bin/sh\\necho "FAIL!  : ArchiveValidationTest::truncatedArchiveIsRejected()"\\necho "Totals: 12 passed, 1 failed"\\nexit 1\\n\' > build/debug/leocad-d3-priority\n'
            "chmod +x build/debug/leocad-d3-priority\n"
            "exit 0\n",
        )

        completed = self.run_runner("priority")

        self.assertNotEqual(completed.returncode, 0)
        failure = self.preserved("last-failure", "d3-priority-results.txt")
        self.assertTrue(failure.exists())
        self.assertIn("truncatedArchiveIsRejected", failure.read_text(encoding="utf-8"))
        meta = self.preserved("last-failure", "d3-priority-meta.txt").read_text(encoding="utf-8")
        self.assertIn("stage=test", meta)
        self.assertIn("truncatedArchiveIsRejected", meta)

    def test_a_failed_build_log_copy_does_not_fail_the_suite(self) -> None:
        """The build-log copy needs its own guard, and this file just proved it.

        Found by mutation, not by reading: leaving this "cp" bare left the whole
        file green. Every other unwritable-path case makes the RESULTS copy fail
        first, which aborts before the build log is ever touched — so without
        this case the guard could be deleted and the next broken disk would turn
        "this build failed" into "the runner died", losing the remaining suites.
        """
        source = self.root / "build.log"
        source.write_text(MAKE_FAILURE, encoding="utf-8")
        result = self.root / "test-results.txt"
        result.write_text(FAILED_RUN, encoding="utf-8")
        last_run = self.root / "build" / "last-run"
        last_run.mkdir(parents=True)
        (last_run / f"d3-{SUITE}-build.log").symlink_to("/nonexistent/build-log-target")

        env = dict(os.environ, PRESERVE_LOG_ROOT=str(self.root))
        completed = subprocess.run(
            ["bash", str(SCRIPT), SUITE, "1", str(result), "test", str(source)],
            env=env,
            capture_output=True,
            text=True,
        )

        self.assertEqual(
            completed.returncode,
            0,
            f"a failed build-log copy must not fail the suite: {completed.stderr}",
        )
        self.assertIn("warning:", completed.stderr)
        self.assertTrue(
            (self.root / "build" / "last-failure" / f"d3-{SUITE}-results.txt").exists(),
            "the failure branch must still run after the last-run build log failed",
        )

    def test_a_failed_failure_build_log_copy_is_reported_not_fatal(self) -> None:
        """Same guard on the failure destination, reached only when its "mkdir" worked."""
        source = self.root / "build.log"
        source.write_text(MAKE_FAILURE, encoding="utf-8")
        result = self.root / "test-results.txt"
        result.write_text(FAILED_RUN, encoding="utf-8")
        failure_dir = self.root / "build" / "last-failure"
        failure_dir.mkdir(parents=True)
        (failure_dir / f"d3-{SUITE}-build.log").symlink_to("/nonexistent/failure-build-log")

        env = dict(os.environ, PRESERVE_LOG_ROOT=str(self.root))
        completed = subprocess.run(
            ["bash", str(SCRIPT), SUITE, "1", str(result), "make", str(source)],
            env=env,
            capture_output=True,
            text=True,
        )

        self.assertEqual(
            completed.returncode,
            0,
            f"a failed build-log copy must not fail the suite: {completed.stderr}",
        )
        self.assertIn("warning:", completed.stderr)
        self.assertTrue(
            (self.root / "build" / "last-run" / f"d3-{SUITE}-results.txt").exists(),
            "the last-run copy must still have been written",
        )

    def test_the_build_log_travels_with_the_result(self) -> None:
        """A build failure preserved without its output is not evidence.

        Drives the preserve script directly with the two D9 arguments, so the
        rule is pinned independently of the runner above.
        """
        source = self.root / "build.log"
        source.write_text(MAKE_FAILURE, encoding="utf-8")
        result = self.root / "test-results.txt"
        result.write_text(
            "BUILD FAILED\nfailed stage: make\nproject: d3-archive-tests.pro\n",
            encoding="utf-8",
        )

        env = dict(os.environ, PRESERVE_LOG_ROOT=str(self.root))
        subprocess.run(
            ["bash", str(SCRIPT), SUITE, "2", str(result), "make", str(source)],
            check=True,
            env=env,
            capture_output=True,
        )

        for destination in ("last-run", "last-failure"):
            kept = self.root / "build" / destination / f"d3-{SUITE}-build.log"
            self.assertTrue(kept.exists(), f"no build log under {destination}")
            self.assertIn("make: *** [Makefile:1234", kept.read_text(encoding="utf-8"))
        meta = (self.root / "build" / "last-failure" / f"d3-{SUITE}-meta.txt").read_text(
            encoding="utf-8"
        )
        self.assertIn("stage=make", meta)

    def test_a_usage_error_is_still_exit_2(self) -> None:
        """An unknown suite stays a usage error, and it stays loud.

        exit 2 means "you asked for something that does not exist"; folding it
        into the generic failure status would let a typo in a CI job read like a
        test failure. No build is attempted for it, so nothing is preserved.
        """
        self.stub("qmake", "exit 0\n")
        self.stub("make", "exit 0\n")

        completed = self.run_runner("no-such-suite")

        self.assertEqual(completed.returncode, 2, completed.stderr)
        self.assertIn("no such suite", completed.stderr)
        self.assertFalse(self.log_root.exists(), "a usage error must not write anything")


if __name__ == "__main__":
    unittest.main(verbosity=2)