#!/usr/bin/env python3
"""Verify that every file referenced by a .qrc actually exists, and report the
ones that are generated during the build.

`resources/leocad_zh_CN.qm` and friends do not exist in a fresh checkout: they
are produced by `lrelease` before `rcc` runs (leocad.pro invokes it from the
qmake step). A plain existence check would therefore fail on a clean tree, and a
missing generated file would only surface later as a build error. This script
separates the two cases so a genuinely missing resource cannot hide behind a
generated one.

Usage:
    scripts/library/check_qrc_resources.py
    scripts/library/check_qrc_resources.py --json
"""

from __future__ import annotations

import argparse
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
QRC_FILES = ("leocad.qrc", "resources/stylesheet/stylesheet.qrc")

# Suffixes produced by a build step rather than committed to the repository.
GENERATED_SUFFIXES = (".qm",)


def check(qrc_relative: str) -> dict:
    qrc_path = REPO_ROOT / qrc_relative
    result = {
        "qrc": qrc_relative,
        "entries": 0,
        "missing": [],
        "generated": [],
        "present": 0,
        "prefix": "",
    }
    if not qrc_path.exists():
        result["missing"].append(qrc_relative)
        return result

    root = ET.fromstring(qrc_path.read_text("utf-8"))
    for resource in root.iter("qresource"):
        prefix = resource.get("prefix", "")
        for element in resource.iter("file"):
            text = (element.text or "").strip()
            if not text:
                continue
            result["entries"] += 1
            # Files in a .qrc are relative to the directory holding the .qrc.
            candidate = qrc_path.parent / text
            if candidate.exists():
                result["present"] += 1
                continue
            if text.endswith(GENERATED_SUFFIXES):
                result["generated"].append(f"{prefix}/{text}")
            else:
                result["missing"].append(f"{prefix}/{text}")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--strict", action="store_true",
                        help="treat build-generated files as errors too")
    args = parser.parse_args()

    results = [check(relative) for relative in QRC_FILES]
    failed = False
    for result in results:
        if result["missing"] or (args.strict and result["generated"]):
            failed = True

    if args.json:
        json.dump({"ok": not failed, "results": results}, sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        for result in results:
            print(f"{result['qrc']}: {result['entries']} entries, "
                  f"{result['present']} present, "
                  f"{len(result['generated'])} build-generated, "
                  f"{len(result['missing'])} missing")
            for name in result["generated"]:
                print(f"  generated at build time: {name}")
            for name in result["missing"]:
                print(f"  MISSING: {name}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
