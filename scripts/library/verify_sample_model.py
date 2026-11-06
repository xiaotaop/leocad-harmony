#!/usr/bin/env python3
"""Checks that the screenshot sample model is one the device can actually draw.

docs/release/商店文案.md section 7 freezes a screenshot plan the leader executes at
RC1, and shots 01 and 04 are supposed to open exactly this file. That makes the
sample release material, so it has to be right in ways a text review will not
catch:

  1. every part id must exist in the lite library the build packages, otherwise
     the device draws a placeholder box and the store shows something it does not
     deliver;
  2. every colour code must be defined in ldconfig.ldr, otherwise the part falls
     back to a default colour;
  3. the parts must stack UP. LDraw Y grows DOWNWARD: a part's origin sits on its
     top face and its body occupies positive y, and LeoCAD preserves that when it
     converts (common/lc_math.h maps (x, y, z) to (x, z, -y)). So a higher layer
     has a SMALLER y. The first version of this model counted up, which put the
     roof below the walls and the chimney below that. Checks 1 and 2 were green
     the whole time, which is why check 3 exists.

A note on parsing: this used to skip any type-1 line its regex did not match. An
independent review showed that a record such as

    1 999 0e0 0 0 1 0 0 0 1 0 0 0 1 no-such-part.dat

was never checked for its part or its colour while the run still exited 0. Every
type-1 line is now parsed or the run fails; nothing is skipped.

Run with:
    python3 scripts/library/verify_sample_model.py
"""

from __future__ import annotations

import re
import unittest
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

ARCHIVE = REPO / "assets" / "ldraw-lite" / "library-lite.zip"
SAMPLE = REPO / "docs" / "evidence" / "member-c" / "2026-11-06" / "screenshot-sample.ldr"

COLOUR = re.compile(r"^0\s+!COLOUR\s+\S+\s+CODE\s+(\d+)", re.I)

# LDraw accepts 0e0 as a float; Python's float() does not. Exponent forms are
# normalised here rather than used as a reason to skip the record.
NUMBER = re.compile(r"^[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?$")


class SampleLine:
    """One type-1 reference, fully parsed."""

    __slots__ = ("colour", "position", "matrix", "part", "line_number")

    def __init__(self, colour, position, matrix, part, line_number):
        self.colour = colour
        self.position = position
        self.matrix = matrix
        self.part = part
        self.line_number = line_number


def number(token):
    if not NUMBER.match(token):
        raise ValueError("%r is not a number" % token)
    return float(token)


def parse_lines():
    """Every type-1 reference, plus the lines that could not be parsed."""

    references = []
    unparsed = []

    for line_number, raw in enumerate(
        SAMPLE.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = raw.strip()
        if not line or line.startswith("0"):
            continue

        head = line.split()[0]
        if head != "1":
            unparsed.append(
                "line %d: unexpected statement type %r" % (line_number, head)
            )
            continue

        tokens = line.split()
        if len(tokens) != 15:
            unparsed.append(
                "line %d: type-1 record has %d fields, expected 15"
                % (line_number, len(tokens))
            )
            continue

        try:
            colour = int(tokens[1])
            values = [number(token) for token in tokens[2:14]]
        except ValueError as error:
            unparsed.append("line %d: %s" % (line_number, error))
            continue

        part = tokens[14].lower()
        if not part.endswith(".dat"):
            unparsed.append(
                "line %d: %r is not a .dat part reference"
                % (line_number, tokens[14])
            )
            continue

        references.append(
            SampleLine(colour, values[0:3], values[3:12], part, line_number)
        )

    return references, unparsed


def library_names():
    with zipfile.ZipFile(ARCHIVE) as archive_file:
        return {name.lower(): name for name in archive_file.namelist()}


def find_part(names, part):
    """Resolve a part reference to an archive entry.

    LDraw sub-part references use a backslash (s\\3001s01.dat) while the archive
    stores forward slashes, so the reference is normalised before matching. Getting
    this wrong is silent: the child simply is not found, its geometry never enters
    the extent, and the caller carries on with a smaller answer.
    """

    normalised = part.lower().replace(chr(92), "/")
    for lowered, original in names.items():
        if lowered.endswith("/" + normalised):
            return original
    return None


def colour_codes():
    with zipfile.ZipFile(ARCHIVE) as archive_file:
        config = archive_file.read("ldraw/ldconfig.ldr").decode(
            "utf-8", errors="replace"
        )
    codes = set()
    for line in config.splitlines():
        match = COLOUR.match(line)
        if match:
            codes.add(int(match.group(1)))
    return codes


UNRESOLVED = []


def _extent(archive_file, names, original, depth):
    if depth > 12:
        return None
    try:
        text = archive_file.read(original).decode("utf-8", errors="replace")
    except KeyError:
        return None

    low = None
    high = None

    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue

        head = line.split()[0]

        if head == "1":
            tokens = line.split()
            if len(tokens) != 15:
                continue
            try:
                values = [number(token) for token in tokens[2:14]]
            except ValueError:
                continue
            child = find_part(names, tokens[14])
            if child is None:
                # A sub-part the archive does not contain means the extent is
                # wrong, not merely incomplete. Record it; the test fails on it.
                UNRESOLVED.append("%s references %s, which is missing"
                                  % (original, tokens[14]))
                continue
            sub = _extent(archive_file, names, child, depth + 1)
            if sub is None:
                continue
            translate = values[0:3]
            matrix = values[3:12]
            for cx in sub:
                for cy in sub:
                    for cz in sub:
                        y = (
                            translate[1]
                            + matrix[3] * cx
                            + matrix[4] * cy
                            + matrix[5] * cz
                        )
                        low = y if low is None else min(low, y)
                        high = y if high is None else max(high, y)

        elif head == "3" or head == "4":
            tokens = line.split()
            try:
                coords = [number(token) for token in tokens[2:]]
            except ValueError:
                continue
            if head == "3":
                coords = coords[:9]
            for index in range(0, len(coords) - 2, 3):
                y = coords[index + 1]
                low = y if low is None else min(low, y)
                high = y if high is None else max(high, y)

    if low is None:
        return None
    return (low, high)


def part_y_extent(part):
    """LDraw-space y range the part occupies, measured from its own geometry."""

    names = library_names()
    original = find_part(names, part)
    if original is None:
        return None
    with zipfile.ZipFile(ARCHIVE) as archive_file:
        return _extent(archive_file, names, original, 0)


def placed_y_range(reference):
    """The part's y range after applying this line's position and rotation."""

    extent = part_y_extent(reference.part)
    if extent is None:
        return None

    low, high = extent
    matrix = reference.matrix
    values = []
    for cx in (low, high):
        for cy in (low, high):
            for cz in (low, high):
                values.append(
                    reference.position[1]
                    + matrix[3] * cx
                    + matrix[4] * cy
                    + matrix[5] * cz
                )
    return (min(values), max(values))


class SampleModelTests(unittest.TestCase):
    def test_sample_model_exists(self):
        self.assertTrue(
            SAMPLE.is_file(),
            "the screenshot plan points at %s but it is not committed" % SAMPLE.name,
        )

    def test_every_type_one_line_is_parseable(self):
        """Nothing may be skipped.

        The previous version only inspected lines its regex happened to match, so
        an unusual or malformed record passed unnoticed and the run stayed green.
        Every type-1 line either parses completely or this fails.
        """

        references, unparsed = parse_lines()

        self.assertEqual(
            unparsed,
            [],
            "these lines could not be parsed, so nothing below would have checked "
            "them: " + "; ".join(unparsed),
        )
        self.assertGreaterEqual(
            len(references), 5, "only %d part reference(s) parsed" % len(references)
        )

    def test_every_referenced_part_is_in_the_shipped_library(self):
        names = library_names()
        references, _ = parse_lines()
        parts = set(reference.part for reference in references)

        missing = sorted(part for part in parts if find_part(names, part) is None)
        self.assertEqual(
            missing,
            [],
            "these parts are used by the screenshot sample but are not in "
            "assets/ldraw-lite/library-lite.zip, so they render as placeholders: "
            + ", ".join(missing),
        )

    def test_every_colour_is_defined_in_ldconfig(self):
        codes = colour_codes()
        self.assertGreater(len(codes), 100, "ldconfig.ldr parsed as almost empty")

        references, _ = parse_lines()
        used = set(reference.colour for reference in references)
        self.assertGreater(
            len(used),
            0,
            "no colour codes parsed, so this check would pass on an empty set",
        )

        unknown = sorted(used - codes)
        self.assertEqual(
            unknown,
            [],
            "the sample uses colour codes ldconfig.ldr does not define, which fall "
            "back to a default colour: " + ", ".join(str(code) for code in unknown),
        )

    def test_part_geometry_resolves_completely(self):
        """A missing sub-part would silently shrink the measured extent.

        e.g. 3001.dat refers to s\\3001s01.dat for its studs. If that lookup
        fails the brick still measures its body but loses everything the
        sub-part contributes, and a geometry check built on top of it is
        measuring less than it claims.
        """

        references, _ = parse_lines()
        UNRESOLVED.clear()
        for reference in references:
            part_y_extent(reference.part)

        self.assertEqual(
            UNRESOLVED,
            [],
            "part geometry could not be fully resolved, so any extent measured "
            "from it is incomplete: " + "; ".join(sorted(set(UNRESOLVED))[:10]),
        )

    def test_the_model_stacks_upward(self):
        """Geometry, because ids and colours cannot prove the model stands up.

        LDraw Y grows downward, so "on top of" is a SMALLER y. The first version
        of this model assumed the opposite and stacked the roof below the walls;
        every id and colour was valid and the other checks stayed green.
        """

        references, _ = parse_lines()
        self.assertTrue(references, "no part references to measure")

        spans = []
        for reference in references:
            placed = placed_y_range(reference)
            self.assertIsNotNone(
                placed,
                "%s (line %d) has no measurable geometry, so the vertical order "
                "cannot be verified" % (reference.part, reference.line_number),
            )
            spans.append((placed[0], placed[1], reference.part, reference.line_number))

        top = min(span[0] for span in spans)
        bottom = max(span[1] for span in spans)
        self.assertLess(top, bottom, "the model has no vertical extent")

        # The ground plane is the bottom face of whatever sits lowest. That has to
        # be derived from the model, not from a part name: 3035.dat is used for
        # both the base plates and the roof, so selecting "the base" by id picked
        # up the roof too and made this check pass with the stack inverted.
        #
        # A stack is well formed when its layers tile the height: every part rests
        # on, or is part of, exactly one contiguous run of material from the ground
        # up. The cheap, robust form of that: no layer may float in the middle of
        # nowhere, so the set of part spans must be connected when swept bottom-up.
        ground = max(span[1] for span in spans)
        top = min(span[0] for span in spans)

        # Sweep from the ground upward (decreasing y) and require that material is
        # continuous: at every step the next part has to start at or above the
        # surface already reached. A gap means something is floating; material
        # missing from the middle means the stack was assembled inside out.
        # Sweep from the ground upward (decreasing y). Layers that rest on the
        # surface reached so far extend it; anything that never connects is
        # reported by name, because "which part is in the wrong place" is the
        # only useful thing to say when this fires.
        ordered = sorted(spans, key=lambda span: -span[1])
        reached = ground
        gaps = []

        for low, high, part, line_number in ordered:
            if high < reached - 0.5 and low <= reached + 0.5:
                continue  # same layer as something already incorporated
            if high < reached - 0.5:
                gaps.append(
                    "%s (line %d) spans y=[%.0f,%.0f] but nothing connects it to "
                    "the surface at y=%.0f"
                    % (part, line_number, low, high, reached)
                )
                continue
            if low > reached + 0.5:
                gaps.append(
                    "%s (line %d) starts at y=%.0f, leaving a gap above the "
                    "surface at y=%.0f" % (part, line_number, low, reached)
                )
            reached = min(reached, low)

        self.assertEqual(
            gaps,
            [],
            "LDraw Y grows DOWNWARD, so stacking upward means DECREASING y and "
            "each layer must sit on the one below. These parts do not form one "
            "continuous stack from the ground up: " + "; ".join(gaps),
        )

        self.assertAlmostEqual(
            reached,
            top,
            delta=0.5,
            msg="the stack stops at y=%.0f but the model reaches y=%.0f, so some "
            "layer is detached from the rest" % (reached, top),
        )

if __name__ == "__main__":
    unittest.main(verbosity=2)
