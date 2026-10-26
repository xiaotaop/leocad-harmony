#!/usr/bin/env python3
"""Generate assets/ldraw-lite/selection.txt from the official LDraw library.

Why a generator instead of a hand-written list: LeoCAD's parts tree is driven by
the category expressions in ``common/lc_category.cpp`` matching against part
descriptions, not by the (sparse) ``!CATEGORY`` metadata. A hand-picked list
drifts away from those expressions and silently leaves categories empty. This
script reads the expressions straight out of the C++ source, so the seed list
and the tree can never disagree.

Selection rules, applied per category:
  * the part must be an official top-level part with a redistributable licence;
  * the description must match the category expression (same semantics as
    lcMatchCategory: ``%`` = begins the description, ``^`` = whole word);
  * patterned (``~``) parts, aliases (``=``), obsolete/moved parts and stickers
    are skipped — the first release only ships plain building parts;
  * within a category, parts are ordered by (description length, id) so the
    output is stable and prefers the canonical variants over decorated ones.

Usage:
    scripts/library/generate_selection.py            # write selection.txt
    scripts/library/generate_selection.py --dry-run  # report counts only
"""

from __future__ import annotations

import argparse
import re
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CATEGORY_SOURCE = REPO_ROOT / "common" / "lc_category.cpp"
SELECTION_FILE = REPO_ROOT / "assets" / "ldraw-lite" / "selection.txt"
DEFAULT_CACHE = Path.home() / ".cache" / "leocad-harmony" / "ldraw"

# How many seeds to take per category. Categories that are conceptually large
# (Brick, Plate, Tile, Slope) get more room; the closure adds subparts and
# primitives on top, so the final part count stays inside 500-1500.
CATEGORY_BUDGET = {
    "Brick": 150,
    "Plate": 130,
    "Tile": 45,
    "Slope": 70,
    "Technic": 60,
    "Round": 55,
    "Door and Window": 45,
    "Tyre and Wheel": 40,
    "Vehicle": 35,
    "Minifig": 45,
    "Panel": 25,
    "Hinge and Bracket": 30,
    "Container": 25,
    "Bar": 20,
    "Arch": 18,
    "Baseplate": 15,
    "Wedge": 25,
    "Windscreen": 18,
    "Wing": 15,
    "Plant": 18,
    "Animal": 15,
    "Antenna": 10,
    "Boat": 12,
    "Electric": 20,
    "Hose": 15,
    "Sign and Flag": 15,
    "Support": 12,
    "Train": 15,
    "Miscellaneous": 25,
    "Other": 10,
    "Sticker": 0,          # stickers are out of first-release scope
}
DEFAULT_BUDGET = 15

LICENSE_OK = ("CC BY 4.0", "CC BY 2.0 and CC BY 4.0",
              "CCAL version 2.0")

# Substrings that mark a part as out of scope for the first release.
EXCLUDE_DESCRIPTION = re.compile(
    r"\b(sticker|obsolete|moved\b|renamed|duplicate)", re.IGNORECASE)


def read_category_expressions(path: Path) -> list[tuple[str, str]]:
    """Parse the DefaultCategories[] table out of lc_category.cpp."""
    if not path.exists():
        raise SystemExit(f"missing {path}")
    text = path.read_text("utf-8")
    marker = "const char DefaultCategories[]"
    if marker not in text:
        raise SystemExit(f"{path}: DefaultCategories[] not found")
    block = text[text.index(marker):]
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
    """Port of lcMatchCategory() from common/lc_category.cpp.

    Supports the same grammar the C++ implementation does: ``|`` (or),
    ``&`` (and), ``!`` (not), parentheses, ``^`` whole word and ``%`` at the
    beginning of the description. Case-insensitive throughout.
    """
    return _match(description, expression)


def _match(description: str, expression: str) -> bool:
    expression = expression.strip()
    if not expression:
        return False

    # Split on the first top-level operator, mirroring the C++ scan order.
    depth = 0
    for index, char in enumerate(expression):
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        elif depth == 0 and char == "!":
            return not _match(description, expression[index + 1:])
        elif depth == 0 and char in "|&":
            left = _match(description, expression[:index])
            right = _match(description, expression[index + 1:])
            return left or right if char == "|" else left and right

    if "(" in expression:
        # Evaluate the first parenthesised group as a whole expression.
        start = expression.index("(")
        depth = 0
        for index in range(start, len(expression)):
            if expression[index] == "(":
                depth += 1
            elif expression[index] == ")":
                depth -= 1
                if depth == 0:
                    inner = expression[start + 1:index]
                    return _match(description, inner)
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
        # Allow a leading decoration marker, as the C++ code does.
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


@dataclass
class Candidate:
    part_id: str
    description: str
    ldraw_org: str


def load_candidates(archive: Path) -> list[Candidate]:
    if not archive.exists():
        raise SystemExit(
            f"missing official library archive: {archive}\n"
            f"run scripts/library/fetch_ldraw.py first")

    candidates: list[Candidate] = []
    with zipfile.ZipFile(archive) as bundle:
        names = [name for name in bundle.namelist()
                 if name.startswith("ldraw/parts/")
                 and name.lower().endswith(".dat")
                 and "/s/" not in name.lower()]
        for name in sorted(names):
            raw = bundle.read(name)[:1500]
            header = raw.decode("utf-8", "replace")
            lines = header.splitlines()
            if not lines:
                continue
            description = lines[0][2:].strip() if lines[0].startswith("0 ") else ""
            if not description:
                continue
            licence = ""
            ldraw_org = ""
            for line in lines[:40]:
                if line.startswith("0 !LICENSE"):
                    licence = line.split("!LICENSE", 1)[1].strip()
                elif line.startswith("0 !LDRAW_ORG"):
                    ldraw_org = line.split("!LDRAW_ORG", 1)[1].strip()
            if not any(token in licence for token in LICENSE_OK):
                continue
            # Skip patterned (~), aliases (=), and moved/obsolete parts.
            if description.startswith(("~", "=")):
                continue
            if EXCLUDE_DESCRIPTION.search(description):
                continue
            if re.search(r"\b(Moved|Obsolete)\b", ldraw_org):
                continue
            candidates.append(Candidate(
                part_id=name.rsplit("/", 1)[-1].lower(),
                description=description,
                ldraw_org=ldraw_org))
    return candidates


def part_shape_rank(description: str) -> tuple:
    """Prefer canonical, plain variants: short descriptions, no extras."""
    has_dimension = bool(re.search(r"\d", description))
    has_special = bool(re.search(
        r"\b(Printed|Pattern|with|Decorated|Logo|Text|Sticker)\b", description,
        re.IGNORECASE))
    return (has_special, not has_dimension, len(description), description.lower())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, default=DEFAULT_CACHE)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    cache = args.cache.expanduser().resolve() / "complete.zip"
    categories = read_category_expressions(CATEGORY_SOURCE)
    candidates = load_candidates(cache)
    print(f"{len(candidates)} candidate parts, {len(categories)} categories",
          file=sys.stderr)

    # Assign each candidate to the first category it matches (categories are
    # checked in table order, which is how the tree presents them).
    buckets: dict[str, list[Candidate]] = {name: [] for name, _ in categories}
    uncategorised = 0
    for candidate in candidates:
        for name, expression in categories:
            if match_category(candidate.description, expression):
                buckets[name].append(candidate)
                break
        else:
            uncategorised += 1

    lines: list[str] = [
        "# Curated LDraw seed list for the first HarmonyOS release.",
        "# Generated by scripts/library/generate_selection.py -- do not hand edit;",
        "# change the budget table in that script and regenerate instead.",
        "#",
        "# One part id per line, grouped by LeoCAD category. Category headings in",
        "# [brackets] are informational: the tree itself comes from",
        "# common/lc_category.cpp. Referenced subparts and primitives are added",
        "# automatically by build_library.py.",
        "",
    ]
    total = 0
    empty: list[str] = []
    for name, _expression in categories:
        bucket = sorted(buckets[name], key=lambda item: part_shape_rank(item.description))
        budget = CATEGORY_BUDGET.get(name, DEFAULT_BUDGET)
        chosen = bucket[:budget]
        if not chosen:
            empty.append(name)
        lines.append(f"[{name}]")
        if not chosen:
            lines.append("# (no plain official parts match this category)")
        for candidate in chosen:
            lines.append(candidate.part_id)
        lines.append("")
        total += len(chosen)
        print(f"  {name:20s} {len(chosen):4d} of {len(bucket):5d}", file=sys.stderr)

    print(f"seeds: {total}; candidates matching no category: {uncategorised}",
          file=sys.stderr)
    if empty:
        print(f"categories with no seed: {', '.join(empty)}", file=sys.stderr)
    if total < 500:
        print(f"ERROR: only {total} seeds; raise CATEGORY_BUDGET", file=sys.stderr)
        return 1

    if args.dry_run:
        print("dry run: selection.txt not written", file=sys.stderr)
        return 0

    SELECTION_FILE.parent.mkdir(parents=True, exist_ok=True)
    SELECTION_FILE.write_text("\n".join(lines), "utf-8")
    print(f"wrote {SELECTION_FILE.relative_to(REPO_ROOT)} ({total} seeds)",
          file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
