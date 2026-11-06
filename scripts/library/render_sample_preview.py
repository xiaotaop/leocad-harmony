#!/usr/bin/env python3
"""Draw orthographic elevations of the screenshot sample from real geometry.

This is not LeoCAD and it is not a substitute for the RC1 device screenshots. It
exists for one narrow purpose: the sample model's vertical stacking was wrong in a
way no text review caught, because every part id and colour was valid while the
roof sat BELOW the walls (LDraw Y grows downward, so stacking upward means
decreasing y). Numbers in a test can assert that, but seeing the silhouette is what
makes the mistake obvious.

Elevations are drawn instead of a perspective view on purpose: a flat projection
has no depth ordering to get wrong, so what the image shows is exactly what the
geometry says. Front elevation is x across and -y up; side elevation is z across.

Run with:
    python3 scripts/library/render_sample_preview.py
"""

from __future__ import annotations

import re
import sys
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ARCHIVE = REPO / "assets" / "ldraw-lite" / "library-lite.zip"
SAMPLE = REPO / "docs" / "evidence" / "member-c" / "2026-11-06" / "screenshot-sample.ldr"
OUTPUT = SAMPLE.with_suffix(".preview.png")

NUMBER = re.compile(r"^[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?$")
COLOUR_LINE = re.compile(
    r"^0\s+!COLOUR\s+\S+\s+CODE\s+(\d+)\s+VALUE\s+(#[0-9A-Fa-f]{6})"
)


def number(token):
    if not NUMBER.match(token):
        raise ValueError(token)
    return float(token)


def resolve(names, part):
    normalised = part.lower().replace(chr(92), "/")
    for lowered, original in names.items():
        if lowered.endswith("/" + normalised):
            return original
    return None


def apply(transform, point):
    if transform is None:
        return list(point)
    matrix, translate = transform
    return [
        matrix[0] * point[0] + matrix[1] * point[1] + matrix[2] * point[2] + translate[0],
        matrix[3] * point[0] + matrix[4] * point[1] + matrix[5] * point[2] + translate[1],
        matrix[6] * point[0] + matrix[7] * point[1] + matrix[8] * point[2] + translate[2],
    ]


def collect(archive_file, names, part, transform, depth, out):
    if depth > 12:
        return
    original = resolve(names, part)
    if original is None:
        return
    try:
        text = archive_file.read(original).decode("utf-8", errors="replace")
    except KeyError:
        return

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
            local = (values[3:12], values[0:3])
            if transform is None:
                composed = local
            else:
                pm, pt = transform
                lm, lt = local
                rotated = [
                    pm[0] * lm[0] + pm[1] * lm[3] + pm[2] * lm[6],
                    pm[0] * lm[1] + pm[1] * lm[4] + pm[2] * lm[7],
                    pm[0] * lm[2] + pm[1] * lm[5] + pm[2] * lm[8],
                    pm[3] * lm[0] + pm[4] * lm[3] + pm[5] * lm[6],
                    pm[3] * lm[1] + pm[4] * lm[4] + pm[5] * lm[7],
                    pm[3] * lm[2] + pm[4] * lm[5] + pm[5] * lm[8],
                    pm[6] * lm[0] + pm[7] * lm[3] + pm[8] * lm[6],
                    pm[6] * lm[1] + pm[7] * lm[4] + pm[8] * lm[7],
                    pm[6] * lm[2] + pm[7] * lm[5] + pm[8] * lm[8],
                ]
                moved = [
                    pm[0] * lt[0] + pm[1] * lt[1] + pm[2] * lt[2] + pt[0],
                    pm[3] * lt[0] + pm[4] * lt[1] + pm[5] * lt[2] + pt[1],
                    pm[6] * lt[0] + pm[7] * lt[1] + pm[8] * lt[2] + pt[2],
                ]
                composed = (rotated, moved)
            collect(archive_file, names, tokens[14], composed, depth + 1, out)

        elif head in ("3", "4"):
            tokens = line.split()
            try:
                coords = [number(token) for token in tokens[2:]]
            except ValueError:
                continue
            points = [
                apply(transform, coords[index : index + 3])
                for index in range(0, len(coords) - 2, 3)
            ]
            if head == "3" and len(points) >= 3:
                out.append(points[:3])
            elif head == "4" and len(points) >= 4:
                out.append([points[0], points[1], points[2]])
                out.append([points[0], points[2], points[3]])


def palette():
    colours = {}
    with zipfile.ZipFile(ARCHIVE) as archive_file:
        config = archive_file.read("ldraw/ldconfig.ldr").decode("utf-8", errors="replace")
    for line in config.splitlines():
        match = COLOUR_LINE.match(line)
        if match:
            value = match.group(2)
            colours[int(match.group(1))] = (
                int(value[1:3], 16),
                int(value[3:5], 16),
                int(value[5:7], 16),
            )
    return colours


def main():
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("Pillow is required: pip install pillow", file=sys.stderr)
        return 1

    colours = palette()
    faces = []

    with zipfile.ZipFile(ARCHIVE) as archive_file:
        names = {name.lower(): name for name in archive_file.namelist()}
        for raw in SAMPLE.read_text(encoding="utf-8").splitlines():
            line = raw.strip()
            if not line or line.startswith("0"):
                continue
            tokens = line.split()
            if tokens[0] != "1" or len(tokens) != 15:
                continue
            try:
                colour = int(tokens[1])
                values = [number(token) for token in tokens[2:14]]
            except ValueError:
                continue
            root = (values[3:12], values[0:3])
            out = []
            collect(archive_file, names, tokens[14], root, 0, out)
            for triangle in out:
                faces.append((colour, triangle))

    if not faces:
        print("no geometry extracted", file=sys.stderr)
        return 1

    panel = 620
    pad = 40
    gap = 40

    all_points = [point for _, triangle in faces for point in triangle]
    lowest = max(point[1] for point in all_points)
    highest = min(point[1] for point in all_points)

    def elevation(horizontal, filename_index):
        values = [point[horizontal] for point in all_points]
        left, right = min(values), max(values)
        span_h = (right - left) or 1
        span_v = (lowest - highest) or 1
        scale = min((panel - 2 * pad) / span_h, (panel - 2 * pad) / span_v)

        image = Image.new("RGB", (panel, panel), (250, 250, 250))
        draw = ImageDraw.Draw(image)

        def project(point):
            across = point[horizontal]
            return (
                panel / 2 + (across - (left + right) / 2) * scale,
                panel / 2 - (-point[1] - (-(lowest + highest) / 2)) * scale,
            )

        # Paint far to near on the remaining axis so nearer faces win.
        depth_axis = 2 if horizontal == 0 else 0
        for colour, triangle in sorted(
            faces, key=lambda face: -sum(p[depth_axis] for p in face[1]) / 3.0
        ):
            rgb = colours.get(colour, (150, 150, 150))
            depth = sum(p[depth_axis] for p in triangle) / 3.0
            factor = 0.80 if depth > 0 else 1.0
            shaded = tuple(max(0, min(255, int(channel * factor))) for channel in rgb)
            draw.polygon([project(point) for point in triangle], fill=shaded)

        # Ground line under the lowest point.
        # Same transform as project(): the bottom of the model is the largest
        # LDraw y, and it must land at the BOTTOM of the panel.
        ground_y = panel / 2 - (-lowest - (-(lowest + highest) / 2)) * scale
        draw.line([(pad, ground_y), (panel - pad, ground_y)], fill=(120, 120, 120))
        return image

    front = elevation(0, 0)
    side = elevation(2, 1)

    combined = Image.new("RGB", (panel * 2 + gap, panel), (250, 250, 250))
    combined.paste(front, (0, 0))
    combined.paste(side, (panel + gap, 0))
    combined.save(OUTPUT)
    print("wrote %s" % OUTPUT)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
