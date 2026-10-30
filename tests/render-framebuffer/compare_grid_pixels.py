#!/usr/bin/env python3
"""Compare grid-on/off captures of the fixed red/blue two-brick model (Pillow)."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageFilter


def compare(directory, phase):
    records = []
    for size in (0, 1):
        on = Image.open(directory / f"size{size}-gridon.png").convert("RGB")
        off = Image.open(directory / f"size{size}-gridoff.png").convert("RGB")
        if on.size != off.size:
            raise ValueError("Grid on/off capture sizes differ")
        mask = Image.new("L", off.size)
        mask.putdata([255 if ((r > 60 and r > g*1.5 and r > b*1.5)
                             or (b > 60 and b > r*1.5 and b > g*1.1)) else 0
                      for r, g, b in off.getdata()])
        mask = mask.filter(ImageFilter.MinFilter(5))
        pixels = changed = 0
        for a, b, inside in zip(on.getdata(), off.getdata(), mask.getdata()):
            if inside:
                pixels += 1
                changed += max(abs(x-y) for x, y in zip(a, b)) > 8
        if not pixels:
            raise ValueError("No red/blue model pixels found")
        records.append(dict(phase=phase, size=size, pixels=on.size,
                            model_interior_pixels=pixels, changed_model_pixels=changed))
    return records


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", required=True, type=Path)
    parser.add_argument("--after", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    results = compare(args.before, "before") + compare(args.after, "after")
    args.output.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    if not all(r["changed_model_pixels"] > 0 for r in results[:2]):
        raise SystemExit("Negative control did not reproduce grid overdraw")
    if not all(r["changed_model_pixels"] == 0 for r in results[2:]):
        raise SystemExit("Grid still changes model interior pixels")
    print("PASS: negative controls reproduce overdraw; repaired interiors unchanged")
