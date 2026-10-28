#!/usr/bin/env python3
"""Generate the broken-archive variants used by tst_archive_validation.cpp.

Each variant removes exactly one property from the shipped subset, so the test
can assert that the startup validator notices that specific defect. Generating
them here rather than committing them keeps the fixtures in step with
assets/ldraw-lite/library-lite.zip: if the subset changes, the variants change.

Variants:
  wrongroot.zip   every entry outside ldraw/        -> loader skips them all
  nocfg.zip       no ldraw/ldconfig.ldr             -> colours fall back silently
  dangling.zip    a referenced primitive removed    -> part renders incomplete
  truncated.zip   half the entries dropped          -> count check fails

Usage:
    tests/library/d3/prepare_variants.py
"""

from __future__ import annotations

import shutil
import sys
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parents[2]
SOURCE = REPO_ROOT / "assets" / "ldraw-lite" / "library-lite.zip"
OUTPUT = HERE / "variants"


def load(archive: Path):
    with zipfile.ZipFile(archive) as bundle:
        return [(info, bundle.read(info.filename)) for info in bundle.infolist()]


def write(path: Path, entries) -> None:
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as bundle:
        for info, data in entries:
            bundle.writestr(info, data)


def main() -> int:
    if not SOURCE.exists():
        print(f"missing {SOURCE}", file=sys.stderr)
        return 1

    entries = load(SOURCE)
    print(f"source: {len(entries)} entries", file=sys.stderr)

    if OUTPUT.exists():
        shutil.rmtree(OUTPUT)
    OUTPUT.mkdir(parents=True)

    # 1. Entries hoisted out of ldraw/, which is what a mis-rooted archive looks
    #    like. The loader skips every one and still reports success.
    hoisted = []
    for info, data in entries:
        copy = zipfile.ZipInfo(info.filename[len("ldraw/"):]
                               if info.filename.lower().startswith("ldraw/")
                               else info.filename)
        copy.compress_type = info.compress_type
        copy.date_time = info.date_time
        hoisted.append((copy, data))
    write(OUTPUT / "wrongroot.zip", hoisted)

    # 2. No colour configuration: colours silently fall back to the built-in set.
    write(OUTPUT / "nocfg.zip",
          [(i, d) for i, d in entries if i.filename.lower() != "ldraw/ldconfig.ldr"])

    # 3. A primitive that many parts reference, removed. The parts stay, so the
    #    archive looks complete by file count but renders with holes.
    write(OUTPUT / "dangling.zip",
          [(i, d) for i, d in entries if i.filename.lower() != "ldraw/p/4-4edge.dat"])

    # 4. Half the archive gone: the count check has to catch this.
    write(OUTPUT / "truncated.zip", entries[: len(entries) // 2])

    # 5. ldconfig.ldr present but emptied. The file exists, so an existence-only
    #    check passes; LoadColors() then silently uses the built-in palette and
    #    every part renders in the wrong colour.
    write(OUTPUT / "emptycfg.zip",
          [(i, b"" if i.filename.lower() == "ldraw/ldconfig.ldr" else d)
           for i, d in entries])

    # 6. ldconfig.ldr present with content but no colour records at all.
    write(OUTPUT / "garbagecfg.zip",
          [(i, b"0 comment only, no colour definitions\n"
                if i.filename.lower() == "ldraw/ldconfig.ldr" else d)
           for i, d in entries])

    for path in sorted(OUTPUT.iterdir()):
        print(f"  {path.name}: {path.stat().st_size} bytes", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
