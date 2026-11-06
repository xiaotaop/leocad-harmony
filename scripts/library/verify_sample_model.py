#!/usr/bin/env python3
"""Checks that the screenshot sample model only uses parts and colours we ship.

docs/release/商店文案.md section 7 freezes a screenshot plan that the leader will
execute at RC1. To make shot 01 and 04 reproducible without depending on someone
inventing a model on the device, this check guards the sample committed under
docs/evidence/member-c/. It is deliberately narrow: it does not test that the
model looks good, only that every line resolves against the same lite library the
build packages. A part id that exists in full LDraw but not in our subset would
render as a placeholder box on the device, and the screenshot would be showing
something the store listing does not deliver.

Run with:
    python3 scripts/library/verify_sample_model.py
"""

from __future__ import annotations

import re
import sys
import unittest
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

ARCHIVE = REPO / "assets" / "ldraw-lite" / "library-lite.zip"
SAMPLE = REPO / "docs" / "evidence" / "member-c" / "2026-11-06" / "screenshot-sample.ldr"

# A type-1 LDraw line: 1 <colour> <x> <y> <z> <a b c d e f g h i> <part>
REFERENCE = re.compile(r"^1\s+(\d+)\s+(?:-?[\d.]+\s+){12}(\S+\.dat)\s*$", re.I)
COLOUR = re.compile(r"^0\s+!COLOUR\s+\S+\s+CODE\s+(\d+)", re.I)


def archive_names() -> set[str]:
    with zipfile.ZipFile(ARCHIVE) as archive:
        return {name.lower() for name in archive.namelist()}


def colour_codes() -> set[int]:
    with zipfile.ZipFile(ARCHIVE) as archive:
        config = archive.read("ldraw/ldconfig.ldr").decode("utf-8", errors="replace")
    return {int(match.group(1)) for match in map(COLOUR.match, config.splitlines()) if match}


class SampleModelTests(unittest.TestCase):
    def test_sample_model_exists(self) -> None:
        self.assertTrue(
            SAMPLE.is_file(),
            f"the screenshot plan points at {SAMPLE.name} but it is not committed",
        )

    def test_every_referenced_part_is_in_the_shipped_library(self) -> None:
        names = archive_names()
        parts = set()

        for line in SAMPLE.read_text(encoding="utf-8").splitlines():
            match = REFERENCE.match(line.strip())
            if match:
                parts.add(match.group(2).lower())

        self.assertGreaterEqual(
            len(parts), 5, f"only {len(parts)} distinct parts found in the sample"
        )

        missing = sorted(
            part
            for part in parts
            if not any(name.endswith("/" + part) for name in names)
        )
        self.assertEqual(
            missing,
            [],
            "these parts are used by the screenshot sample but are not in "
            "assets/ldraw-lite/library-lite.zip, so they would render as "
            "placeholders on the device: " + ", ".join(missing),
        )

    def test_every_colour_is_defined_in_ldconfig(self) -> None:
        codes = colour_codes()
        self.assertGreater(len(codes), 100, "ldconfig.ldr parsed as almost empty")

        used = set()
        for line in SAMPLE.read_text(encoding="utf-8").splitlines():
            match = REFERENCE.match(line.strip())
            if match:
                used.add(int(match.group(1)))

        self.assertGreater(
            len(used),
            0,
            "no type-1 lines parsed from the sample, so the colour check above "
            "would pass on an empty set and certify nothing",
        )

        unknown = sorted(used - codes)
        self.assertEqual(
            unknown,
            [],
            "the sample uses colour codes that ldconfig.ldr does not define, which "
            "would fall back to a default colour: " + ", ".join(map(str, unknown)),
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
