#!/usr/bin/env python3
"""Build the curated LDraw subset shipped with the first HarmonyOS release.

Input
    assets/ldraw-lite/selection.txt   seed part ids, grouped by category
    <cache>/complete.zip              official library (see fetch_ldraw.py)

Output
    assets/ldraw-lite/library-lite.zip
    assets/ldraw-lite/manifest.json

The archive layout is not free-form: lcPiecesLibrary::OpenArchive only accepts
an archive rooted at ``ldraw/`` with parts under ``parts/`` and primitives under
``p/`` (it upper-cases the names before matching), and LoadColors() extracts
``ldraw/ldconfig.ldr`` from that same archive. A archive that does not follow
this layout silently loses pieces or colours, so the layout is asserted at the
end of a build.

Dependency closure
    Every ``1 <colour> ... <file>`` reference inside a selected part is resolved
    transitively. Subparts go to ``ldraw/parts/s/`` and primitives to
    ``ldraw/p/``. If the closure grows past the configured ceiling the build
    fails loudly instead of shipping a library that silently misses pieces.

Determinism
    Entries are written in sorted order with a fixed timestamp and a fixed
    compression level, so two builds from the same inputs are byte identical.

Usage:
    scripts/library/build_library.py                 # build
    scripts/library/build_library.py --check         # verify only
    scripts/library/build_library.py --max-files 3000
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import zipfile
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath

REPO_ROOT = Path(__file__).resolve().parents[2]
ASSET_DIR = REPO_ROOT / "assets" / "ldraw-lite"
SELECTION_FILE = ASSET_DIR / "selection.txt"
ARCHIVE_FILE = ASSET_DIR / "library-lite.zip"
MANIFEST_FILE = ASSET_DIR / "manifest.json"
DEFAULT_CACHE = Path.home() / ".cache" / "leocad-harmony" / "ldraw"

LIBRARY_MIN_PARTS = 500
LIBRARY_MAX_PARTS = 1500
DEFAULT_MAX_FILES = 3500

# Fixed timestamp keeps the archive byte-reproducible: 2026-01-01 00:00:00.
ZIP_DATE_TIME = (2026, 1, 1, 0, 0, 0)
COMPRESS_LEVEL = 9

# Licences the LDraw Contributor Agreement permits us to redistribute.
ALLOWED_LICENSES = (
    "Licensed under CC BY 4.0 : see CAreadme.txt",
    "Licensed under CC BY 2.0 and CC BY 4.0 : see CAreadme.txt",
    # Deprecated wording; "CCAL 2.0" means CC BY 2.0.
    "Redistributable under CCAL version 2.0 : see CAreadme.txt",
)

# A type-1 sub-file reference is
#   "1 <colour> x y z a b c d e f g h i <file>"
# i.e. 13 whitespace separated fields before the file name, so the name sits at
# index 14. Splitting is safer than a fixed-width regex because real LDraw files
# are not consistently spaced.
REFERENCE_FILE_FIELD = 14
CATEGORY_RE = re.compile(r"^0\s+!CATEGORY\s+(?P<category>.+?)\s*$")
LICENSE_RE = re.compile(r"^0\s+!LICENSE\s+(?P<license>.+?)\s*$")
LDRAW_ORG_RE = re.compile(r"^0\s+!LDRAW_ORG\s+(?P<org>.+?)\s*$")
AUTHOR_RE = re.compile(r"^0\s+Author:\s*(?P<author>.+?)\s*$")
NAME_RE = re.compile(r"^0\s+Name:\s*(?P<name>.+?)\s*$")


class BuildError(RuntimeError):
    pass


@dataclass
class PartRecord:
    path: str                 # archive path, e.g. ldraw/parts/3001.dat
    kind: str                 # part | subpart | primitive | config
    sha256: str
    size: int
    description: str = ""
    author: str = ""
    ldraw_org: str = ""
    license: str = ""
    categories: list[str] = field(default_factory=list)


def die(message: str) -> "None":
    raise BuildError(message)


def normalise_reference(name: str) -> str:
    """Map an LDraw reference to a lower-case archive-relative path.

    LDraw references are case-insensitive and use backslashes on Windows.
    """
    cleaned = name.replace("\\", "/").strip()
    return cleaned.lower()


def classify(relative: str) -> str:
    """Classify a path below ldraw/ (already lower-cased)."""
    if relative == "ldconfig.ldr":
        return "config"
    if relative.startswith("parts/s/"):
        return "subpart"
    if relative.startswith("parts/"):
        return "part"
    if relative.startswith("p/"):
        return "primitive"
    return "other"


def parse_header(data: bytes) -> dict:
    """Read the LDraw header fields this project needs to record.

    Only the first block of `0` comment lines is inspected; the first non-comment
    line ends the header, which is what the LDraw header specification requires.
    """
    header: dict = {"description": "", "author": "", "ldraw_org": "",
                    "license": "", "categories": []}
    text = data.decode("utf-8", "replace")
    for line in text.splitlines()[:60]:
        if line and not line.startswith("0"):
            break
        if not header["description"]:
            header["description"] = part_description(data)
        category_match = CATEGORY_RE.match(line)
        if category_match:
            header["categories"].extend(
                part.strip()
                for part in category_match.group("category").split(",")
                if part.strip())
        license_match = LICENSE_RE.match(line)
        if license_match:
            header["license"] = license_match.group("license").strip()
        org_match = LDRAW_ORG_RE.match(line)
        if org_match:
            header["ldraw_org"] = org_match.group("org").strip()
        author_match = AUTHOR_RE.match(line)
        if author_match:
            header["author"] = author_match.group("author").strip()
    return header


def part_description(data: bytes) -> str:
    """Return the first header line, which lcPiecesLibrary uses as the label."""
    for line in data.decode("utf-8", "replace").splitlines()[:8]:
        if line.startswith("0 ") and not re.match(
                r"0\s+(Name:|Author:|!|//)", line):
            return line[2:].strip()
        if line and not line.startswith("0"):
            break
    return ""


def load_selection(path: Path) -> list[tuple[str, str]]:
    """Read selection.txt -> [(part_id, category)] preserving file order.

    Format: one part id per line. ``#`` starts a comment. A line beginning with
    ``[name]`` sets the category for the ids that follow.
    """
    if not path.exists():
        die(f"missing seed list: {path}")
    entries: list[tuple[str, str]] = []
    category = ""
    seen: set[str] = set()
    for lineno, raw in enumerate(path.read_text("utf-8").splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            category = line[1:-1].strip()
            continue
        part_id = line.lower()
        if not part_id.endswith(".dat"):
            part_id += ".dat"
        if part_id in seen:
            print(f"warning: {path.name}:{lineno}: duplicate seed {part_id}",
                  file=sys.stderr)
            continue
        seen.add(part_id)
        entries.append((part_id, category))
    if not entries:
        die(f"{path} contains no seed parts")
    return entries


class LibrarySource:
    """Read access to the official archive with case-insensitive lookup."""

    def __init__(self, archive: Path):
        if not archive.exists():
            die(f"missing official library archive: {archive}\n"
                f"run scripts/library/fetch_ldraw.py first")
        self.archive = archive
        self._zip = zipfile.ZipFile(archive)
        self._index: dict[str, str] = {}
        for name in self._zip.namelist():
            self._index[name.lower()] = name

    def close(self) -> None:
        self._zip.close()

    def read(self, relative: str) -> bytes:
        """Read a file by its path below ldraw/ (case-insensitive)."""
        key = f"ldraw/{relative.lower()}"
        actual = self._index.get(key)
        if actual is None:
            raise KeyError(relative)
        return self._zip.read(actual)

    def exists(self, relative: str) -> bool:
        return f"ldraw/{relative.lower()}" in self._index

    def ldconfig_bytes(self) -> bytes:
        return self.read("ldconfig.ldr")

    def library_release(self) -> str:
        header = self.ldconfig_bytes()[:4096].decode("utf-8", "replace")
        match = re.search(r"!LDRAW_ORG\s+Configuration\s+UPDATE\s+(\S+)", header)
        return f"UPDATE {match.group(1)}" if match else "unknown"


def resolve_closure(source: LibrarySource, seeds: list[str],
                    max_files: int) -> tuple[list[str], list[str]]:
    """Expand seeds into a dependency-closed set of ldraw/-relative paths.

    Returns (ordered_paths, warnings). Raises BuildError on a missing include or
    when the closure exceeds max_files.
    """
    resolved: dict[str, str] = {}     # lower-case relative -> actual case relative
    queue: list[str] = []

    def enqueue(relative: str, origin: str) -> None:
        key = relative.lower()
        if key in resolved:
            return
        if not source.exists(relative):
            die(f"{origin} references missing file '{relative}'")
        resolved[key] = relative
        queue.append(relative)

    for seed in seeds:
        try:
            enqueue(f"parts/{seed}", "selection.txt")
        except KeyError:
            die(f"seed part '{seed}' is not in the official library")

    warnings: list[str] = []
    while queue:
        relative = queue.pop()
        if classify(relative.lower()) == "config":
            continue
        data = source.read(relative)
        for line in data.decode("utf-8", "replace").splitlines():
            if not line.startswith("1 "):
                continue
            fields = line.split()
            if len(fields) <= REFERENCE_FILE_FIELD:
                continue
            ref = normalise_reference(fields[REFERENCE_FILE_FIELD])
            # A reference may omit the directory: 'stud.dat' means p/stud.dat,
            # while 's/3001s01.dat' is relative to parts/.
            if ref.startswith("s/"):
                candidate = f"parts/{ref}"
            elif ref.startswith("parts/") or ref.startswith("p/"):
                candidate = ref
            else:
                candidate = f"p/{ref}"
            if not source.exists(candidate):
                # Fall back to parts/ for references that live there.
                fallback = f"parts/{ref}"
                if source.exists(fallback):
                    candidate = fallback
                else:
                    die(f"{relative} references missing file '{ref}'")
            enqueue(candidate, relative)
            if len(resolved) > max_files:
                die(f"dependency closure exceeded {max_files} files; "
                    f"trim assets/ldraw-lite/selection.txt")

    paths = sorted(resolved.values(), key=lambda item: item.lower())
    return paths, warnings


def build_records(source: LibrarySource, paths: list[str]) -> list[PartRecord]:
    records: list[PartRecord] = []
    for relative in paths:
        data = source.read(relative)
        header = parse_header(data)
        kind = classify(relative.lower())
        record = PartRecord(
            path=f"ldraw/{relative}",
            kind=kind,
            sha256=hashlib.sha256(data).hexdigest(),
            size=len(data),
            description=part_description(data),
            author=header["author"],
            ldraw_org=header["ldraw_org"],
            license=header["license"],
            categories=list(dict.fromkeys(header["categories"])),
        )
        records.append(record)

    # The colour configuration is mandatory: LoadColors() looks for it inside
    # the archive first and would otherwise fall back to the built-in file.
    config_relative = "ldconfig.ldr"
    data = source.ldconfig_bytes()
    records.append(PartRecord(
        path=f"ldraw/{config_relative}",
        kind="config",
        sha256=hashlib.sha256(data).hexdigest(),
        size=len(data),
        description="LDraw.org configuration file",
        author="LDraw.org",
        ldraw_org="Configuration",
        license="",
        categories=[],
    ))
    return records


def validate_records(records: list[PartRecord]) -> list[str]:
    """Return a list of fatal problems (empty when the subset is acceptable)."""
    problems: list[str] = []
    parts = [r for r in records if r.kind == "part"]
    if not (LIBRARY_MIN_PARTS <= len(parts) <= LIBRARY_MAX_PARTS):
        problems.append(
            f"part count {len(parts)} outside [{LIBRARY_MIN_PARTS}, "
            f"{LIBRARY_MAX_PARTS}]")

    licences: dict[str, int] = {}
    for record in records:
        if record.kind == "config":
            continue
        if record.license not in ALLOWED_LICENSES:
            problems.append(
                f"{record.path}: unrecognised licence '{record.license}'")
        licences[record.license] = licences.get(record.license, 0) + 1

    # Part ids must be unique when compared case-insensitively; the archive is
    # indexed case-insensitively by lcPiecesLibrary, so a duplicate would make
    # one of the two unreachable.
    seen: dict[str, str] = {}
    for record in records:
        key = record.path.lower()
        if key in seen:
            problems.append(f"duplicate archive path: {record.path}")
        seen[key] = record.path
    return problems


def write_archive(records: list[PartRecord], source: LibrarySource,
                  destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(".zip.tmp")
    if temporary.exists():
        temporary.unlink()

    with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED,
                         compresslevel=COMPRESS_LEVEL) as bundle:
        for record in sorted(records, key=lambda item: item.path.lower()):
            relative = record.path[len("ldraw/"):] if record.path.startswith(
                "ldraw/") else record.path
            info = zipfile.ZipInfo(filename=record.path,
                                   date_time=ZIP_DATE_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            bundle.writestr(info, source.read(relative))
    temporary.replace(destination)


def write_manifest(records: list[PartRecord], source: LibrarySource,
                   archive: Path, cache_meta: dict) -> dict:
    by_kind: dict[str, int] = {}
    licences: dict[str, int] = {}
    categories: dict[str, int] = {}
    for record in records:
        by_kind[record.kind] = by_kind.get(record.kind, 0) + 1
        if record.license:
            licences[record.license] = licences.get(record.license, 0) + 1
        for category in record.categories:
            categories[category] = categories.get(category, 0) + 1

    manifest = {
        "schema_version": 1,
        "generator": {
            "script": "scripts/library/build_library.py",
            "seed_file": "assets/ldraw-lite/selection.txt",
            "utc": "fixed for reproducibility",
        },
        "source": {
            "url": cache_meta.get("url", ""),
            "release": source.library_release(),
            "etag": cache_meta.get("etag", ""),
            "archive_sha256": cache_meta.get("sha256", ""),
        },
        "license": {
            "spdx": "CC-BY-4.0",
            "url": "https://creativecommons.org/licenses/by/4.0/",
            "agreement": "LDraw Contributor Agreement (CAreadme.txt)",
            "notes": "modified: repackaged subset of the official parts library",
            "attribution_required": [
                "name the creator (per-file !Author header)",
                "provide a URL for the work",
                "name the license (CC BY 4.0)",
                "provide a URL for the license",
                "note whether the work has been modified (it has: subset)",
            ],
        },
        "attribution": {
            "work": "LDraw.org Parts Library",
            "work_url": "https://library.ldraw.org/",
            "creator_note": "individual authors are recorded per file in the "
                            "!Author header and repeated in this manifest",
        },
        "counts": {
            "parts": by_kind.get("part", 0),
            "subparts": by_kind.get("subpart", 0),
            "primitives": by_kind.get("primitive", 0),
            "config": by_kind.get("config", 0),
            "total_files": len(records),
        },
        "licenses_by_file": licences,
        "categories_by_file": dict(sorted(categories.items())),
        "archive": {
            "path": "assets/ldraw-lite/library-lite.zip",
            "sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
            "bytes": archive.stat().st_size,
        },
        "files": [
            {
                "path": record.path,
                "kind": record.kind,
                "sha256": record.sha256,
                "bytes": record.size,
                "description": record.description,
                "author": record.author,
                "ldraw_org": record.ldraw_org,
                "license": record.license,
                "categories": record.categories,
            }
            for record in sorted(records, key=lambda item: item.path.lower())
        ],
    }
    MANIFEST_FILE.write_text(json.dumps(manifest, indent=2, ensure_ascii=False)
                             + "\n", "utf-8")
    return manifest


def load_cache_meta(cache_dir: Path) -> dict:
    meta_path = cache_dir / "cache-meta.json"
    if not meta_path.exists():
        return {}
    try:
        return json.loads(meta_path.read_text("utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, default=DEFAULT_CACHE)
    parser.add_argument("--max-files", type=int, default=DEFAULT_MAX_FILES)
    parser.add_argument("--check", action="store_true",
                        help="build in memory and validate without writing")
    args = parser.parse_args()

    cache_dir = args.cache.expanduser().resolve()
    seed_entries = load_selection(SELECTION_FILE)
    seeds = [part_id for part_id, _ in seed_entries]

    source = LibrarySource(cache_dir / "complete.zip")
    try:
        paths, warnings = resolve_closure(source, seeds, args.max_files)
        for warning in warnings:
            print(f"warning: {warning}", file=sys.stderr)

        # Seed categories are authoritative where a file has no !CATEGORY.
        seed_category = {part_id: category for part_id, category in seed_entries}
        records = build_records(source, paths)
        for record in records:
            if record.kind != "part":
                continue
            seed = PurePosixPath(record.path).name.lower()
            category = seed_category.get(seed, "")
            if category and category not in record.categories:
                record.categories.append(category)

        problems = validate_records(records)
        if problems:
            print("library validation failed:", file=sys.stderr)
            for problem in problems:
                print(f"  - {problem}", file=sys.stderr)
            return 1

        counts = {}
        for record in records:
            counts[record.kind] = counts.get(record.kind, 0) + 1
        print(f"parts={counts.get('part', 0)} "
              f"subparts={counts.get('subpart', 0)} "
              f"primitives={counts.get('primitive', 0)} "
              f"config={counts.get('config', 0)} "
              f"total={len(records)}")

        if args.check:
            print("check-only: nothing written")
            return 0

        write_archive(records, source, ARCHIVE_FILE)
        manifest = write_manifest(records, source, ARCHIVE_FILE,
                                  load_cache_meta(cache_dir))
        print(f"wrote {ARCHIVE_FILE.relative_to(REPO_ROOT)} "
              f"({manifest['archive']['bytes']} bytes, "
              f"sha256 {manifest['archive']['sha256'][:16]}...)")
        print(f"wrote {MANIFEST_FILE.relative_to(REPO_ROOT)}")
        return 0
    finally:
        source.close()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except BuildError as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
