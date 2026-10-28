#!/usr/bin/env python3
"""Tests for the curated LDraw parts library and its manifest.

Run with:
    python3 -m unittest discover -s tests/library -v

These cover the four checks the development plan assigns to Member C (file
existence, id uniqueness, category completeness, licence completeness) plus
dependency closure and reproducibility, which are what actually prevent a
shipped part from failing to load at runtime.
"""

from __future__ import annotations

import hashlib
import importlib.util
import json
import sys
import unittest
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

ASSET_DIR = REPO_ROOT / "assets" / "ldraw-lite"
ARCHIVE = ASSET_DIR / "library-lite.zip"
MANIFEST = ASSET_DIR / "manifest.json"
SELECTION = ASSET_DIR / "selection.txt"

LIBRARY_MIN_PARTS = 500
LIBRARY_MAX_PARTS = 1500
REFERENCE_FILE_FIELD = 14

ALLOWED_LICENSES = (
    "Licensed under CC BY 4.0 : see CAreadme.txt",
    "Licensed under CC BY 2.0 and CC BY 4.0 : see CAreadme.txt",
    "Redistributable under CCAL version 2.0 : see CAreadme.txt",
)


def load_module(name: str, relative: str):
    """Import a script by path (the scripts are not a package).

    The module is registered in sys.modules before execution because
    @dataclass resolves annotations through sys.modules[cls.__module__]; without
    the registration it raises AttributeError on Python 3.10.
    """
    path = REPO_ROOT / relative
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


class LibraryManifestTests(unittest.TestCase):
    """Invariants of the committed subset and its manifest."""

    @classmethod
    def setUpClass(cls) -> None:
        if not ARCHIVE.exists() or not MANIFEST.exists():
            raise unittest.SkipTest(
                "library-lite.zip or manifest.json missing; run "
                "scripts/library/build_library.py")
        cls.manifest = json.loads(MANIFEST.read_text("utf-8"))
        cls.files = cls.manifest["files"]
        cls.bundle = zipfile.ZipFile(ARCHIVE)
        cls.names = set(cls.bundle.namelist())

    @classmethod
    def tearDownClass(cls) -> None:
        cls.bundle.close()

    def test_archive_hash_matches_manifest(self):
        digest = hashlib.sha256(ARCHIVE.read_bytes()).hexdigest()
        self.assertEqual(digest, self.manifest["archive"]["sha256"],
                         "archive changed without regenerating the manifest")

    def test_every_manifest_entry_exists_in_archive(self):
        missing = sorted({entry["path"] for entry in self.files} - self.names)
        self.assertEqual(missing, [], f"manifest lists absent files: {missing}")

    def test_every_archive_file_is_in_manifest(self):
        listed = {entry["path"] for entry in self.files}
        extra = sorted(self.names - listed)
        self.assertEqual(extra, [], f"archive has unlisted files: {extra}")

    def test_per_file_hashes_match(self):
        for entry in self.files:
            if entry["path"] not in self.names:
                continue
            data = self.bundle.read(entry["path"])
            self.assertEqual(hashlib.sha256(data).hexdigest(), entry["sha256"],
                             f"{entry['path']}: hash mismatch")
            self.assertEqual(len(data), entry["bytes"],
                             f"{entry['path']}: length mismatch")

    def test_part_count_within_first_release_range(self):
        parts = [e for e in self.files if e["kind"] == "part"]
        self.assertGreaterEqual(len(parts), LIBRARY_MIN_PARTS,
                                "fewer than 500 parts in the curated library")
        self.assertLessEqual(len(parts), LIBRARY_MAX_PARTS,
                             "more than 1500 parts; trim selection.txt")

    def test_declared_counts_match_files(self):
        counts = self.manifest["counts"]
        for kind, key in (("part", "parts"), ("subpart", "subparts"),
                          ("primitive", "primitives"), ("config", "config")):
            actual = sum(1 for e in self.files if e["kind"] == kind)
            self.assertEqual(counts[key], actual, f"counts.{key} mismatch")
        self.assertEqual(counts["total_files"], len(self.files))

    def test_ids_unique_case_insensitively(self):
        # lcPiecesLibrary indexes parts with an upper-cased name, so two files
        # differing only in case would make one of them unreachable.
        lowered = [name.lower() for name in self.names]
        duplicates = {name for name in lowered if lowered.count(name) > 1}
        self.assertEqual(duplicates, set(), f"duplicate paths: {duplicates}")

    def test_every_file_has_a_redistributable_licence(self):
        for entry in self.files:
            if entry["kind"] == "config":
                continue
            self.assertIn(entry["license"], ALLOWED_LICENSES,
                          f"{entry['path']}: licence '{entry['license']}'")

    def test_licence_totals_match_manifest(self):
        actual: dict[str, int] = {}
        for entry in self.files:
            if entry["kind"] == "config":
                continue
            actual[entry["license"]] = actual.get(entry["license"], 0) + 1
        self.assertEqual(actual, self.manifest["licenses_by_file"])

    def test_colour_configuration_is_packaged(self):
        # LoadColors() reads ldraw/ldconfig.ldr from the archive; without it the
        # application silently falls back to the built-in colour list.
        self.assertIn("ldraw/ldconfig.ldr", self.names)

    def test_archive_layout_matches_loader_expectations(self):
        # lcPiecesLibrary::OpenArchive requires an ldraw/ root, parts/ and p/
        # below it, and upper-cases names before matching.
        for name in self.names:
            self.assertTrue(name.startswith("ldraw/"),
                            f"{name}: archive must be rooted at ldraw/")
            rest = name[len("ldraw/"):]
            self.assertTrue(
                rest.startswith(("parts/", "p/")) or rest == "ldconfig.ldr",
                f"{name}: unexpected location inside the archive")

    def test_every_subfile_reference_resolves(self):
        index = {name.lower() for name in self.names}
        unresolved: list[str] = []
        for entry in self.files:
            if entry["kind"] == "config":
                continue
            text = self.bundle.read(entry["path"]).decode("utf-8", "replace")
            for line in text.splitlines():
                if not line.startswith("1 "):
                    continue
                fields = line.split()
                if len(fields) <= REFERENCE_FILE_FIELD:
                    continue
                ref = fields[REFERENCE_FILE_FIELD].replace("\\", "/").lower()
                if ref.startswith("s/"):
                    candidates = [f"ldraw/parts/{ref}"]
                elif ref.startswith(("parts/", "p/")):
                    candidates = [f"ldraw/{ref}"]
                else:
                    candidates = [f"ldraw/p/{ref}", f"ldraw/parts/{ref}"]
                if not any(candidate in index for candidate in candidates):
                    unresolved.append(f"{entry['path']} -> {ref}")
        self.assertEqual(unresolved, [],
                         f"{len(unresolved)} unresolved references, first: "
                         f"{unresolved[0] if unresolved else ''}")

    def test_categories_from_lecad_table_are_covered(self):
        verify = load_module("verify_library", "scripts/library/verify_library.py")
        expressions = verify.read_category_expressions(
            REPO_ROOT / "common" / "lc_category.cpp")
        self.assertTrue(expressions, "could not parse the category table")

        parts = [e for e in self.files if e["kind"] == "part"]
        per_category = {name: 0 for name, _ in expressions}
        for entry in parts:
            for name, expression in expressions:
                if verify.match_category(entry.get("description", ""), expression):
                    per_category[name] += 1
                    break

        # Sticker is deliberately empty: stickers are out of first-release scope.
        empty = [name for name, count in per_category.items()
                 if count == 0 and name != "Sticker"]
        self.assertEqual(empty, [], f"categories with no parts: {empty}")

    def test_manifest_records_provenance_and_attribution(self):
        source = self.manifest["source"]
        self.assertTrue(source["url"].startswith("https://"),
                        "source url missing")
        self.assertRegex(source["release"], r"^UPDATE \d{4}-\d{2}-\d{2}$",
                         "library release not recorded")
        self.assertRegex(source["archive_sha256"], r"^[0-9a-f]{64}$",
                         "source archive hash not recorded")

        licence = self.manifest["license"]
        self.assertEqual(licence["spdx"], "CC-BY-4.0")
        self.assertIn("creativecommons.org/licenses/by/4.0", licence["url"])
        # CC BY requires five attribution items; the manifest must carry all of
        # them so the store listing can repeat them.
        self.assertEqual(len(licence["attribution_required"]), 5)
        self.assertIn("modified", " ".join(licence["attribution_required"]))

    def test_selection_file_lists_every_seed_part(self):
        self.assertTrue(SELECTION.exists(), "selection.txt missing")
        seeds = [line.split("#")[0].strip().lower() for line in
                 SELECTION.read_text("utf-8").splitlines()]
        seeds = [s for s in seeds if s and not s.startswith("[")]
        parts = {Path(e["path"]).name.lower()
                 for e in self.files if e["kind"] == "part"}
        missing = [seed for seed in seeds if seed not in parts]
        self.assertEqual(missing, [],
                         f"seed parts not in the archive: {missing[:5]}")


class LibraryVerifierTests(unittest.TestCase):
    """The verifier itself must fail on a broken library."""

    def test_verifier_passes_on_committed_library(self):
        verify = load_module("verify_library", "scripts/library/verify_library.py")
        if not ARCHIVE.exists():
            self.skipTest("library not built")
        report = verify.verify(ARCHIVE, MANIFEST)
        self.assertEqual(report.errors, [],
                         f"verifier reported: {report.errors[:3]}")

    def test_verifier_rejects_missing_archive(self):
        verify = load_module("verify_library", "scripts/library/verify_library.py")
        report = verify.verify(Path("/tmp/does-not-exist.zip"), MANIFEST)
        self.assertTrue(report.errors)


class SelectionGeneratorTests(unittest.TestCase):
    """The category matcher used to build the seed list must match LeoCAD."""

    def test_matcher_handles_begin_and_whole_word(self):
        gen = load_module("generate_selection",
                          "scripts/library/generate_selection.py")
        self.assertTrue(gen.match_category("Brick 2 x 4", "^%Brick"))
        self.assertFalse(gen.match_category("Technic Brick 1 x 2", "^%Brick"))
        self.assertTrue(gen.match_category("Plate 1 x 2", "^%Plate"))
        self.assertTrue(gen.match_category("Wheel 8 x 8", "%^Wheel"))
        self.assertFalse(gen.match_category("Wheelbarrow Body", "%^Wheel"))

    def test_matcher_handles_or_and_negation(self):
        gen = load_module("generate_selection",
                          "scripts/library/generate_selection.py")
        self.assertTrue(gen.match_category("Door 1 x 2", "^%Door | ^%Window"))
        self.assertTrue(gen.match_category("Window 1 x 2", "^%Door | ^%Window"))
        self.assertFalse(gen.match_category("Brick 1 x 2", "^%Door | ^%Window"))
        self.assertTrue(gen.match_category("Tyre 14", "!^%Brick"))


if __name__ == "__main__":
    unittest.main()
