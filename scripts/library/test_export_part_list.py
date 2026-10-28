#!/usr/bin/env python3
"""Regression tests for export_part_list.py's completeness checks.

The checks in that script are the only thing standing between a partially
packaged subset and a release where parts silently render incomplete, so they
need tests of their own. Every case here builds a tiny synthetic archive rather
than using the real one: the real archive currently has no defects, which means
it cannot show that the checks would notice any.

Run with:
    python3 scripts/library/test_export_part_list.py
    python3 -m unittest discover -s scripts/library -p 'test_*.py'

Each case asserts on the process exit code, because that is the contract the
packaging pipeline relies on — a non-zero code stops the release.
"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import tempfile
import unittest
import zipfile
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / "export_part_list.py"

_SPEC = importlib.util.spec_from_file_location("export_part_list", SCRIPT)
export_part_list = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(export_part_list)

# A minimal but structurally faithful set: one seed part, a subpart it uses, and
# a primitive the subpart uses. The nesting is the point — without it, a check
# that only looks at top-level parts would appear to work.
PART = "ldraw/parts/3001.dat"
SUBPART = "ldraw/parts/s/3001s01.dat"
PRIMITIVE = "ldraw/p/box.dat"

LICENCE = "0 !LICENSE Licensed under CC BY 4.0 : see CAreadme.txt"

BODIES = {
    PART: "\n".join([
        "0 Brick  2 x  4",
        "0 Name: 3001.dat",
        LICENCE,
        "1 16 0 0 0 1 0 0 0 1 0 0 0 1 s\\3001s01.dat",
    ]) + "\n",
    SUBPART: "\n".join([
        "0 ~Brick  2 x  4 Subpart",
        "0 Name: s/3001s01.dat",
        "1 16 0 0 0 1 0 0 0 1 0 0 0 1 box.dat",
    ]) + "\n",
    PRIMITIVE: "\n".join([
        "0 Box",
        "0 Name: box.dat",
        "3 16 0 0 0 1 0 0 0 1 0",
    ]) + "\n",
}


def build_case(directory: Path, members, seed_ids=("3001",),
               manifest_ids=("3001",)) -> Path:
    """Write selection.txt, manifest.json and library-lite.zip into directory."""
    directory.mkdir(parents=True, exist_ok=True)

    (directory / "selection.txt").write_text(
        "# test fixture\n[Brick]\n" + "\n".join(seed_ids) + "\n", encoding="utf-8")

    entries = []
    for part_id in manifest_ids:
        entries.append({
            "path": f"ldraw/parts/{part_id}.dat",
            "kind": "part",
            "sha256": "",
            "bytes": 0,
            "description": "Brick  2 x  4",
            "author": "fixture",
            "ldraw_org": "Part",
            "license": "Licensed under CC BY 4.0 : see CAreadme.txt",
            "categories": [],
        })
    (directory / "manifest.json").write_text(
        json.dumps({"schema_version": 1, "files": entries}, indent=1),
        encoding="utf-8")

    with zipfile.ZipFile(directory / "library-lite.zip", "w",
                         zipfile.ZIP_DEFLATED) as bundle:
        for name in members:
            bundle.writestr(name, BODIES[name])

    return directory


class ExportPartListTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)

    def run_script(self, assets: Path, output: Path) -> tuple[int, str]:
        """Invoke the real entry point and capture its exit code and stderr.

        The script reports fatal packaging defects by raising SystemExit, which
        is how the command line turns into a non-zero exit code. Catch it here
        and return the code, so the tests assert on the same contract the
        packaging pipeline sees.
        """
        stderr = io.StringIO()
        argv = ["export_part_list.py", "--assets", str(assets),
                "--output", str(output)]
        code = 0
        with contextlib.redirect_stderr(stderr), \
                contextlib.redirect_stdout(io.StringIO()):
            import sys
            saved = sys.argv
            sys.argv = argv
            try:
                code = export_part_list.main()
            except SystemExit as exit_request:
                code = exit_request.code if isinstance(exit_request.code, int) else 1
                # The message lives on the exception, not in stderr: that is how
                # SystemExit("...") works, and Python prints it to stderr only
                # when the interpreter unwinds. Fold it in so assertions can see
                # both paths the same way.
                if exit_request.code not in (0, None):
                    stderr.write(str(exit_request.code) + "\n")
            finally:
                sys.argv = saved
        return code or 0, stderr.getvalue()

    def make_baseline(self) -> Path:
        """A complete archive, plus the CSV that matches it."""
        assets = build_case(self.root / "assets", [PART, SUBPART, PRIMITIVE])
        output = self.root / "expected.csv"
        code, _ = self.run_script(assets, output)
        self.assertEqual(code, 0, "the complete fixture should export cleanly")
        return assets

    def test_complete_archive_passes(self) -> None:
        assets = self.make_baseline()
        code, stderr = self.run_script(assets, self.root / "expected.csv")
        self.assertEqual(code, 0, stderr)

    def test_missing_primitive_under_subpart_fails(self) -> None:
        """A primitive referenced only by a subpart must still be checked.

        This is the case a top-level-only walk misses: the part's own reference
        (to the subpart) resolves fine, and nothing else is ever inspected.
        """
        assets = self.make_baseline()
        build_case(assets, [PART, SUBPART])          # primitive dropped
        code, stderr = self.run_script(assets, self.root / "expected.csv")
        self.assertNotEqual(code, 0, "missing primitive must fail the export")
        self.assertIn("box.dat", stderr)

    def test_missing_seed_part_fails(self) -> None:
        """A seed listed in selection.txt but absent from the archive must fail."""
        assets = self.make_baseline()
        build_case(assets, [SUBPART, PRIMITIVE])     # the seed part itself gone
        code, stderr = self.run_script(assets, self.root / "expected.csv")
        self.assertNotEqual(code, 0, "missing seed part must fail the export")
        self.assertIn("3001", stderr)

    def test_missing_closure_part_fails(self) -> None:
        """A top-level part referenced by a seed but absent must fail.

        The seed part is rewritten to reference a second top-level part, which
        makes 3002 a closure member. The archive then omits it.
        """
        # Patch the body before building anything, and restore afterwards, so
        # the baseline export in make_baseline() uses the two-part variant too.
        seed = "\n".join([
            "0 Brick  2 x  4",
            "0 Name: 3001.dat",
            LICENCE,
            "1 16 0 0 0 1 0 0 0 1 0 0 0 1 s\\3001s01.dat",
            "1 16 0 0 0 1 0 0 0 1 0 0 0 1 3002.dat",
        ]) + "\n"
        saved_part = BODIES[PART]
        BODIES[PART] = seed
        self.addCleanup(BODIES.__setitem__, PART, saved_part)

        BODIES["ldraw/parts/3002.dat"] = "\n".join([
            "0 Brick  1 x  2",
            "0 Name: 3002.dat",
            LICENCE,
            "3 16 0 0 0 1 0 0 0 1 0",
        ]) + "\n"
        self.addCleanup(BODIES.pop, "ldraw/parts/3002.dat", None)

        # Complete archive first: this proves the fixture is valid before the
        # defect is introduced, so a failure below cannot be blamed on the setup.
        complete = build_case(self.root / "complete",
                              [PART, SUBPART, PRIMITIVE, "ldraw/parts/3002.dat"],
                              manifest_ids=("3001", "3002"))
        code, stderr = self.run_script(complete, self.root / "complete.csv")
        self.assertEqual(code, 0, f"two-part fixture should export: {stderr}")

        broken = build_case(self.root / "broken", [PART, SUBPART, PRIMITIVE],
                            manifest_ids=("3001", "3002"))
        code, stderr = self.run_script(broken, self.root / "complete.csv")
        self.assertNotEqual(code, 0, "missing closure part must fail the export")
        self.assertIn("3002.dat", stderr)

    def test_missing_manifest_entry_fails(self) -> None:
        """A shipped part with no manifest entry must fail with a clear message.

        Without this the export dies on a bare KeyError, which reads like a bug
        in the script rather than a defect in the packaging artefacts.
        """
        assets = self.make_baseline()
        build_case(assets, [PART, SUBPART, PRIMITIVE], manifest_ids=())
        code, stderr = self.run_script(assets, self.root / "expected.csv")
        self.assertNotEqual(code, 0, "missing manifest entry must fail")
        self.assertIn("manifest", stderr.lower())

    def test_wrong_root_directory_fails(self) -> None:
        """Every entry must sit under ldraw/, matching what the loader accepts."""
        assets = build_case(
            self.root / "assets",
            [PART, SUBPART, PRIMITIVE])
        # Rewrite the archive with the entries at the archive root instead.
        with zipfile.ZipFile(assets / "library-lite.zip", "w",
                             zipfile.ZIP_DEFLATED) as bundle:
            for name in (PART, SUBPART, PRIMITIVE):
                bundle.writestr(name[len("ldraw/"):], BODIES[name])

        code, stderr = self.run_script(assets, self.root / "expected.csv")
        self.assertNotEqual(code, 0, "entries outside ldraw/ must fail")
        self.assertIn("3001", stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
