#!/usr/bin/env python3
"""Export the curated part list as CSV for review.

The D2 deliverable is a list of concrete part ids, not just counts. This script
turns the packaging artefacts into ``docs/library/D2_零件清单.csv`` so a reviewer
can check, per part, why it is in the library and where the panel will show it.

Inputs (all produced by the D3 packaging step, in ``assets/ldraw-lite``):

  selection.txt   seed ids grouped by LeoCAD category
  manifest.json   per-file description and licence
  library-lite.zip  the packaged subset, needed to walk the reference graph

Two different "category" answers are recorded, and they are deliberately not
merged:

  seed_category     which budget the part was picked under
  panel_categories  which categories the tree will actually list it in

The second one comes from matching the part description against
``DefaultCategories[]`` in ``common/lc_category.cpp``, which is what
``lcMatchCategory()`` does at runtime. They differ in both directions: a part
can be seeded under one budget and still appear under several categories, and
closure parts appear in the tree without having any budget at all.

Usage:
    scripts/library/export_part_list.py
    scripts/library/export_part_list.py --assets assets/ldraw-lite --check
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import sys
import zipfile
from collections import Counter, deque
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CATEGORY_SOURCE = REPO_ROOT / "common" / "lc_category.cpp"
OUTPUT_FILE = REPO_ROOT / "docs" / "library" / "D2_零件清单.csv"

FIELDS = [
    "part_id",
    "origin",
    "seed_category",
    "panel_categories",
    "category_count",
    "introduced_by",
    "root_seed",
    "description",
    "license",
]


# --- category expressions -------------------------------------------------

def read_category_expressions(path: Path) -> list[tuple[str, str]]:
    """Parse DefaultCategories[] out of common/lc_category.cpp."""
    text = path.read_text("utf-8")
    marker = "const char DefaultCategories[]"
    if marker not in text:
        raise SystemExit(f"{path}: DefaultCategories[] not found")
    block = text[text.index(marker) + len(marker):]
    block = block[block.index("{") + 1: block.index("};")]
    categories: list[tuple[str, str]] = []
    for line in block.splitlines():
        match = re.match(r'\s*"([A-Za-z0-9 &\'-]+)=(.*?)\\n"', line)
        if match:
            categories.append((match.group(1).strip(), match.group(2)))
    if not categories:
        raise SystemExit(f"{path}: no category expressions parsed")
    return categories


def match_category(description: str, expression: str) -> bool:
    """Port of lcMatchCategory() from common/lc_category.cpp."""
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

    lowered_description = description.lower()
    lowered_word = word.lower()
    position = lowered_description.find(lowered_word)
    if position < 0:
        return False

    if begin and position != 0:
        if position != 1 or lowered_description[0] not in "_~":
            return False

    if whole_word:
        end_index = position + len(lowered_word)
        following = (lowered_description[end_index]
                     if end_index < len(lowered_description) else "")
        if following not in ("", " "):
            return False
        if position != 0 and lowered_description[position - 1] in "_~":
            position -= 1
        if position != 0 and lowered_description[position - 1] != " ":
            return False
    return True


# --- reference graph ------------------------------------------------------

def load_library(archive: Path):
    """Index the subset the way lcPiecesLibrary::OpenArchive() buckets it."""
    bundle = zipfile.ZipFile(archive)
    names = bundle.namelist()

    parts: dict[str, str] = {}
    primitives: dict[str, str] = {}
    for name in names:
        upper = name.upper()
        if (upper.startswith("LDRAW/PARTS/") and "/S/" in upper
                and upper.endswith(".DAT")):
            primitives[upper[len("LDRAW/PARTS/"):]] = name
        elif upper.startswith("LDRAW/P/") and upper.endswith(".DAT"):
            primitives[upper[len("LDRAW/P/"):]] = name
        elif (upper.startswith("LDRAW/PARTS/") and "/" not in upper[len("LDRAW/PARTS/"):]
                and upper.endswith(".DAT")):
            parts[upper[len("LDRAW/PARTS/"):-4]] = name
    return bundle, parts, primitives


def resolve_reference(reference: str, parts, primitives):
    """Mirror the loader's lookup order: parts map first, then primitives.

    Returns ``("part", id)``, ``("subpart", key)`` or ``("primitive", key)``
    naming the archive member that satisfies the reference, or ``None`` when
    nothing does — the case that makes a part render incomplete with no error.

    The distinction matters for the closure walk: a reference into ``parts/s/``
    or ``p/`` still has to exist in the archive, and still has references of its
    own to follow. Returning only "primitive" and moving on would stop the walk
    one level too early and miss a missing primitive under a subpart.
    """
    if reference.startswith("LDRAW/PARTS/"):
        rest = reference[len("LDRAW/PARTS/"):]
        if rest.startswith("S/"):
            return ("subpart", rest) if rest in primitives else None
        part_id = rest[:-4]
        return ("part", part_id) if part_id in parts else None

    if reference.startswith("LDRAW/P/"):
        key = reference[len("LDRAW/P/"):]
        return ("primitive", key) if key in primitives else None

    part_id = reference[:-4]
    if part_id in parts:
        return "part", part_id
    if reference in primitives:
        kind = "subpart" if reference.startswith("S/") else "primitive"
        return kind, reference
    return None


def referenced_entries(bundle, member: str, parts, primitives):
    """Resolve every type-1 reference in one archive member.

    Returns ``(top_level_parts, library_members)``: the part ids to follow in the
    dependency closure, and every archive member the file needs in order to
    render. Raises ``SystemExit`` when a reference cannot be satisfied.
    """
    top_level: list[str] = []
    members: list[str] = []
    for raw in bundle.read(member).decode("utf-8", "replace").splitlines():
        fields = raw.split()
        if len(fields) < 15 or fields[0] != "1":
            continue
        reference = fields[14].replace("\\", "/").upper()
        if not reference.endswith(".DAT"):
            reference += ".DAT"
        resolved = resolve_reference(reference, parts, primitives)
        if resolved is None:
            raise SystemExit(
                f"{member}: unresolvable reference {fields[14]!r} — "
                f"the subset is incomplete")
        kind, key = resolved
        if kind == "part":
            top_level.append(key)
            members.append(parts[key])
        else:
            # Subparts and primitives are library members, not part ids: they
            # never join the closure, but their own references must be followed.
            members.append(primitives[key])
    return top_level, members


def read_selection(path: Path):
    """selection.txt -> ordered seed ids plus their category heading."""
    order: list[str] = []
    categories: dict[str, str] = {}
    heading = ""
    for line in path.read_text("utf-8").splitlines():
        text = line.strip()
        if not text or text.startswith("#"):
            continue
        if text.startswith("[") and text.endswith("]"):
            heading = text[1:-1]
            continue
        part_id = text[:-4] if text.lower().endswith(".dat") else text
        order.append(part_id.upper())
        categories[part_id.upper()] = heading
    return order, categories


def walk_closure(seed_order, parts, primitives, bundle):
    """Breadth-first over part references; record parent and originating seed.

    Two things are checked here that a part-id-only walk cannot see:

    * a seed listed in selection.txt but absent from the archive is an error,
      not something to skip — the caller asked for a part that will not ship;
    * subparts and primitives get their own references followed, so a missing
      primitive underneath a subpart is reported rather than silently accepted.
    """
    parent: dict[str, str] = {}
    root: dict[str, str] = {part_id: part_id for part_id in seed_order}
    queue = deque(seed_order)
    visited_members: set[str] = set()

    missing_seeds = [part_id for part_id in seed_order if part_id not in parts]
    if missing_seeds:
        raise SystemExit(
            f"{len(missing_seeds)} seed parts are listed in selection.txt but "
            f"missing from the archive: {missing_seeds[:5]}")

    while queue:
        part_id = queue.popleft()
        member = parts[part_id]
        visited_members.add(member)

        children, members = referenced_entries(bundle, member, parts, primitives)
        for child in children:
            if child in root:
                continue
            parent[child] = part_id
            root[child] = root[part_id]
            queue.append(child)

        # Follow subparts and primitives too: they can reference further files,
        # and a dangling reference one level down is just as broken.
        pending = [m for m in members if m not in visited_members]
        while pending:
            sub_member = pending.pop()
            visited_members.add(sub_member)
            _children, sub_members = referenced_entries(
                bundle, sub_member, parts, primitives)
            pending.extend(m for m in sub_members if m not in visited_members)

    return parent, root


# --- manifest -------------------------------------------------------------

def license_label(raw: str) -> str:
    if "CC BY 2.0 and CC BY 4.0" in raw:
        return "CC-BY-2.0+4.0"
    if "CC BY 4.0" in raw:
        return "CC-BY-4.0"
    if "CCAL" in raw:
        return "CCAL-2.0"
    return ""


def load_manifest(path: Path):
    manifest = json.loads(path.read_text("utf-8"))
    parts = {}
    for entry in manifest["files"]:
        if entry["kind"] != "part":
            continue
        part_id = entry["path"][len("ldraw/parts/"):-4].upper()
        parts[part_id] = (entry["description"], license_label(entry["license"]))
    return parts


# --- main -----------------------------------------------------------------

def build_rows(assets: Path):
    selection_file = assets / "selection.txt"
    manifest_file = assets / "manifest.json"
    archive_file = assets / "library-lite.zip"
    for path in (selection_file, manifest_file, archive_file):
        if not path.exists():
            raise SystemExit(f"missing {path}")

    categories = read_category_expressions(CATEGORY_SOURCE)
    metadata = load_manifest(manifest_file)
    seed_order, seed_category = read_selection(selection_file)
    bundle, parts, primitives = load_library(archive_file)
    parent, root = walk_closure(seed_order, parts, primitives, bundle)

    seeds = set(seed_order)
    closure = sorted(set(parts) - seeds)
    unreached = [part_id for part_id in closure if part_id not in root]
    if unreached:
        raise SystemExit(
            f"{len(unreached)} parts are in the archive but not reachable from "
            f"the seed list: {unreached[:5]}")

    # The manifest is the only source of description and licence, so a part that
    # ships without an entry would otherwise be reported as a bare KeyError.
    undescribed = [part_id for part_id in list(seed_order) + closure
                   if part_id not in metadata]
    if undescribed:
        raise SystemExit(
            f"{len(undescribed)} parts have no manifest entry: "
            f"{undescribed[:5]} — regenerate the manifest")

    rows = []
    for part_id in seed_order:
        description, licence = metadata[part_id]
        panel = [name for name, expression in categories
                 if match_category(description, expression)]
        rows.append({
            "part_id": part_id.lower(),
            "origin": "seed",
            "seed_category": seed_category[part_id],
            "panel_categories": ";".join(panel),
            "category_count": len(panel),
            "introduced_by": "",
            "root_seed": "",
            "description": description,
            "license": licence,
        })
    for part_id in closure:
        description, licence = metadata[part_id]
        panel = [name for name, expression in categories
                 if match_category(description, expression)]
        rows.append({
            "part_id": part_id.lower(),
            "origin": "closure",
            "seed_category": "",
            "panel_categories": ";".join(panel),
            "category_count": len(panel),
            "introduced_by": parent[part_id].lower(),
            "root_seed": root[part_id].lower(),
            "description": description,
            "license": licence,
        })

    stats = {
        "total": len(rows),
        "seed": sum(1 for row in rows if row["origin"] == "seed"),
        "closure": sum(1 for row in rows if row["origin"] == "closure"),
        "uncategorised": sum(1 for row in rows if row["category_count"] == 0),
        "categories": len(Counter(
            name for row in rows for name in row["panel_categories"].split(";") if name)),
    }
    return rows, stats


def write_csv(path: Path, rows) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    # newline="" is required by the csv module; lineterminator="\n" keeps the
    # file LF-only so git does not rewrite it (see .gitattributes * text=auto).
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def read_csv(path: Path):
    with path.open(encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path,
                        default=REPO_ROOT / "assets" / "ldraw-lite")
    parser.add_argument("--output", type=Path, default=OUTPUT_FILE)
    parser.add_argument("--check", action="store_true",
                        help="compare against the committed CSV instead of writing")
    args = parser.parse_args()

    rows, stats = build_rows(args.assets.expanduser())

    print(f"rows: {stats['total']} "
          f"(seed {stats['seed']} + closure {stats['closure']})", file=sys.stderr)
    print(f"panel categories represented: {stats['categories']}", file=sys.stderr)
    print(f"parts matching no category: {stats['uncategorised']}", file=sys.stderr)

    if args.check:
        committed = read_csv(args.output)
        if len(committed) != len(rows):
            print(f"FAIL: {args.output} has {len(committed)} rows, "
                  f"expected {len(rows)}", file=sys.stderr)
            return 1

        # Key on part_id rather than row position: the file is hand-readable, so
        # someone may have reordered rows without changing the content.
        existing = {row["part_id"]: row for row in committed}
        if len(existing) != len(committed):
            print(f"FAIL: {args.output} contains duplicate part_id values",
                  file=sys.stderr)
            return 1

        expected_ids = {row["part_id"] for row in rows}
        missing = sorted(expected_ids - set(existing))
        extra = sorted(set(existing) - expected_ids)
        if missing or extra:
            print(f"FAIL: missing {len(missing)} parts {missing[:5]}, "
                  f"unexpected {len(extra)} parts {extra[:5]}", file=sys.stderr)
            return 1

        differences = 0
        for expected in rows:
            actual = existing[expected["part_id"]]
            for field in FIELDS:
                if str(actual.get(field, "")) != str(expected[field]):
                    print(f"FAIL: {expected['part_id']} {field}: "
                          f"{actual.get(field)!r} != {expected[field]!r}",
                          file=sys.stderr)
                    differences += 1
                    if differences > 10:
                        print("... more differences suppressed", file=sys.stderr)
                        return 1
        if differences:
            return 1
        print(f"OK: {args.output} matches the artefacts", file=sys.stderr)
        return 0

    write_csv(args.output, rows)
    print(f"wrote {args.output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
