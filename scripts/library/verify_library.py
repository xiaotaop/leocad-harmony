#!/usr/bin/env python3
"""Validate the curated LDraw subset and its manifest.

This is the executable form of the four checks the development plan assigns to
Member C (file existence, id uniqueness, category completeness, licence
completeness) plus a dependency-closure check, which is what actually guarantees
that no shipped part fails to load because a subpart or primitive is missing.

Usage:
    scripts/library/verify_library.py            # verify what is committed
    scripts/library/verify_library.py --json     # machine readable summary

Exit code 0 means the subset is releasable; non-zero lists the problems.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import zipfile
from collections import Counter
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
ASSET_DIR = REPO_ROOT / "assets" / "ldraw-lite"
ARCHIVE_FILE = ASSET_DIR / "library-lite.zip"
MANIFEST_FILE = ASSET_DIR / "manifest.json"
CATEGORY_SOURCE = REPO_ROOT / "common" / "lc_category.cpp"

LIBRARY_MIN_PARTS = 500
LIBRARY_MAX_PARTS = 1500
REFERENCE_FILE_FIELD = 14

ALLOWED_LICENSES = (
    "Licensed under CC BY 4.0 : see CAreadme.txt",
    "Licensed under CC BY 2.0 and CC BY 4.0 : see CAreadme.txt",
    "Redistributable under CCAL version 2.0 : see CAreadme.txt",
)


class Report:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []
        self.summary: dict = {}

    def error(self, message: str) -> None:
        self.errors.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)


def read_category_expressions(path: Path) -> list[tuple[str, str]]:
    """Parse DefaultCategories[] out of common/lc_category.cpp."""
    text = path.read_text("utf-8")
    marker = "const char DefaultCategories[]"
    if marker not in text:
        return []
    block = text[text.index(marker):]
    block = block[block.index("{") + 1: block.index("};")]
    categories: list[tuple[str, str]] = []
    for line in block.splitlines():
        match = re.match(r'\s*"([A-Za-z0-9 &\'-]+)=(.*?)\\n"', line)
        if match:
            categories.append((match.group(1).strip(), match.group(2)))
    return categories


def match_category(description: str, expression: str) -> bool:
    """Port of lcMatchCategory(); kept in sync with generate_selection.py."""
    expression = expression.strip()
    if not expression:
        return False
    depth = 0
    for index, char in enumerate(expression):
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        elif depth == 0 and char == "!":
            return not match_category(description, expression[index + 1:])
        elif depth == 0 and char in "|&":
            left = match_category(description, expression[:index])
            right = match_category(description, expression[index + 1:])
            return left or right if char == "|" else left and right
    if "(" in expression:
        start = expression.index("(")
        depth = 0
        for index in range(start, len(expression)):
            if expression[index] == "(":
                depth += 1
            elif expression[index] == ")":
                depth -= 1
                if depth == 0:
                    return match_category(description, expression[start + 1:index])
        return False
    word = expression
    whole_word = False
    begin = False
    while word and word[0] in "^%":
        if word[0] == "^":
            whole_word = True
        else:
            begin = True
        word = word[1:]
    if not word:
        return False
    description_lower = description.lower()
    word_lower = word.lower()
    position = description_lower.find(word_lower)
    if position < 0:
        return False
    if begin and position != 0:
        if position != 1 or description_lower[0] not in "_~":
            return False
    if whole_word:
        end_index = position + len(word_lower)
        following = (description_lower[end_index]
                     if end_index < len(description_lower) else "")
        if following not in ("", " "):
            return False
        if position != 0 and description_lower[position - 1] in "_~":
            position -= 1
        if position != 0 and description_lower[position - 1] != " ":
            return False
    return True


def verify(archive_file: Path, manifest_file: Path) -> Report:
    report = Report()

    # Display paths relative to the repository when possible; a caller may pass
    # an absolute path outside it (a temporary copy, a test fixture).
    def display(path: Path) -> str:
        try:
            return str(path.relative_to(REPO_ROOT))
        except ValueError:
            return str(path)

    if not archive_file.exists():
        report.error(f"missing archive: {display(archive_file)}")
        return report
    if not manifest_file.exists():
        report.error(f"missing manifest: {display(manifest_file)}")
        return report

    manifest = json.loads(manifest_file.read_text("utf-8"))
    files = manifest.get("files", [])
    if not files:
        report.error("manifest has no files")
        return report

    # ---- archive integrity vs manifest -------------------------------
    archive_bytes = archive_file.read_bytes()
    actual_hash = hashlib.sha256(archive_bytes).hexdigest()
    declared_hash = manifest.get("archive", {}).get("sha256", "")
    if declared_hash != actual_hash:
        report.error("archive sha256 does not match manifest "
                     f"({declared_hash[:16]}... != {actual_hash[:16]}...)")

    with zipfile.ZipFile(archive_file) as bundle:
        names = bundle.namelist()
        name_set = set(names)
        manifest_paths = {entry["path"] for entry in files}

        # 1. file existence, both directions
        for path in sorted(manifest_paths - name_set):
            report.error(f"manifest lists {path} but the archive does not have it")
        for name in sorted(name_set - manifest_paths):
            report.error(f"archive contains {name} but the manifest does not list it")

        # per-file hashes
        hash_mismatch = 0
        for entry in files:
            if entry["path"] not in name_set:
                continue
            digest = hashlib.sha256(bundle.read(entry["path"])).hexdigest()
            if digest != entry["sha256"]:
                report.error(f"{entry['path']}: sha256 mismatch")
                hash_mismatch += 1
            if len(bundle.read(entry["path"])) != entry["bytes"]:
                report.error(f"{entry['path']}: byte length mismatch")
        if hash_mismatch:
            report.summary["hash_mismatches"] = hash_mismatch

        # 2. id uniqueness (lcPiecesLibrary indexes case-insensitively)
        lowered = Counter(name.lower() for name in names)
        for name, count in lowered.items():
            if count > 1:
                report.error(f"duplicate archive path (case-insensitive): {name}")

        parts = [entry for entry in files if entry["kind"] == "part"]
        subparts = [entry for entry in files if entry["kind"] == "subpart"]
        primitives = [entry for entry in files if entry["kind"] == "primitive"]
        configs = [entry for entry in files if entry["kind"] == "config"]

        # 3. counts and declared totals
        if not (LIBRARY_MIN_PARTS <= len(parts) <= LIBRARY_MAX_PARTS):
            report.error(f"part count {len(parts)} outside "
                         f"[{LIBRARY_MIN_PARTS}, {LIBRARY_MAX_PARTS}]")
        counts = manifest.get("counts", {})
        for key, actual in (("parts", len(parts)), ("subparts", len(subparts)),
                            ("primitives", len(primitives)),
                            ("config", len(configs)), ("total_files", len(files))):
            if counts.get(key) != actual:
                report.error(f"manifest counts.{key}={counts.get(key)} "
                             f"but the files list has {actual}")
        if not configs:
            report.error("archive has no ldraw/ldconfig.ldr; colours would silently "
                         "fall back to the built-in list")

        # 4. licence completeness
        licences = Counter()
        for entry in files:
            if entry["kind"] == "config":
                continue
            if entry["license"] not in ALLOWED_LICENSES:
                report.error(f"{entry['path']}: unrecognised licence "
                             f"'{entry['license']}'")
            licences[entry["license"]] += 1
        declared_licences = manifest.get("licenses_by_file", {})
        for licence, count in licences.items():
            if declared_licences.get(licence) != count:
                report.error(f"manifest licenses_by_file mismatch for '{licence}'")

        # 5. dependency closure: every type-1 reference must resolve inside the
        #    archive, otherwise the part loads with holes at runtime.
        index = {name.lower(): name for name in names}
        unresolved: list[str] = []
        for entry in files:
            if entry["kind"] == "config":
                continue
            data = bundle.read(entry["path"]).decode("utf-8", "replace")
            for line in data.splitlines():
                if not line.startswith("1 "):
                    continue
                fields = line.split()
                if len(fields) <= REFERENCE_FILE_FIELD:
                    continue
                ref = fields[REFERENCE_FILE_FIELD].replace("\\", "/").lower()
                if ref.startswith("s/"):
                    candidates = [f"ldraw/parts/{ref}"]
                elif ref.startswith("parts/") or ref.startswith("p/"):
                    candidates = [f"ldraw/{ref}"]
                else:
                    candidates = [f"ldraw/p/{ref}", f"ldraw/parts/{ref}"]
                if not any(candidate in index for candidate in candidates):
                    unresolved.append(f"{entry['path']} -> {ref}")
        if unresolved:
            report.error(f"{len(unresolved)} unresolved sub-file references; "
                         f"first: {unresolved[0]}")

        # 6. category completeness against the LeoCAD category table
        expressions = read_category_expressions(CATEGORY_SOURCE)
        if not expressions:
            report.warn("could not parse category expressions; "
                        "category coverage not checked")
        else:
            per_category: dict[str, int] = {name: 0 for name, _ in expressions}
            for entry in parts:
                description = entry.get("description", "")
                for name, expression in expressions:
                    if match_category(description, expression):
                        per_category[name] += 1
                        break
            empty = [name for name, count in per_category.items() if count == 0]
            # Sticker is intentionally empty: stickers are out of first-release
            # scope. Everything else should have at least one part.
            empty_unexpected = [name for name in empty if name != "Sticker"]
            if empty_unexpected:
                report.error("categories with no parts: "
                             + ", ".join(empty_unexpected))
            covered = sum(1 for count in per_category.values() if count)
            report.summary["category_counts"] = {
                name: count for name, count in per_category.items() if count}
            report.summary["categories_covered"] = covered
            report.summary["categories_total"] = len(expressions)

        report.summary.update({
            "parts": len(parts),
            "subparts": len(subparts),
            "primitives": len(primitives),
            "config": len(configs),
            "total_files": len(files),
            "archive_bytes": len(archive_bytes),
            "archive_sha256": actual_hash,
            "library_release": manifest.get("source", {}).get("release", ""),
            "licences": dict(licences),
        })

    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--archive", type=Path, default=ARCHIVE_FILE)
    parser.add_argument("--manifest", type=Path, default=MANIFEST_FILE)
    args = parser.parse_args()

    report = verify(args.archive, args.manifest)

    if args.json:
        json.dump({"ok": not report.errors,
                   "errors": report.errors,
                   "warnings": report.warnings,
                   "summary": report.summary}, sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        for warning in report.warnings:
            print(f"warning: {warning}", file=sys.stderr)
        if report.errors:
            print(f"FAILED ({len(report.errors)} problems)", file=sys.stderr)
            for problem in report.errors:
                print(f"  - {problem}", file=sys.stderr)
        else:
            summary = report.summary
            print(f"OK  {summary.get('parts')} parts, "
                  f"{summary.get('subparts')} subparts, "
                  f"{summary.get('primitives')} primitives, "
                  f"{summary.get('total_files')} files, "
                  f"{summary.get('archive_bytes', 0) / 1048576:.2f} MB")
            print(f"    library release: {summary.get('library_release')}")
            print(f"    categories covered: "
                  f"{summary.get('categories_covered')}/"
                  f"{summary.get('categories_total')}")
    return 1 if report.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
