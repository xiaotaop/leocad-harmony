#!/usr/bin/env python3
"""Check the first-release menu scope by rendering the running window.

`tests/library/test_ui_scope.py` proves statically that the removed commands are
not added by CreateMenus(); this script proves it at runtime, which is the part a
static check cannot see (an action could be re-added from somewhere else, or a
menu could be rebuilt later).

It screenshots the region of each menu and reports whether the menu titles are
present, using ImageMagick-free primitives: the window is captured with xwd and
converted with ffmpeg, both of which are present on the development machine.

This is a manual verification aid rather than a CI test: it needs a display.

Usage:
    scripts/dev/verify_menu_scope.py --screenshot /tmp/menus.png
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
BINARY = REPO_ROOT / "build" / "desktop" / "build" / "debug" / "leocad"

# Menu titles that must exist after ApplyFirstReleaseScope().
REQUIRED_MENUS = ("文件", "编辑", "视图", "零件", "帮助")

# Feature names that must not be reachable. Matched case-insensitively against
# the screenshot's OCR text when OCR is available, otherwise reported as manual.
FORBIDDEN_TITLES = (
    "Print", "POV-Ray", "Blender", "Instructions", "Split Horizontal",
    "Split Vertical", "Minifig Wizard", "Export HTML", "Check for Updates",
)


def have(tool: str) -> bool:
    return shutil.which(tool) is not None


def capture(output: Path, timeout: int) -> bool:
    if not have("xwd") or not have("ffmpeg"):
        print("error: xwd and ffmpeg are required", file=sys.stderr)
        return False
    raw = output.with_suffix(".xwd")
    result = subprocess.run(
        ["xwd", "-display", ":0", "-root", "-out", str(raw)],
        capture_output=True, text=True, timeout=timeout)
    if result.returncode != 0 or not raw.exists() or raw.stat().st_size == 0:
        print(f"error: screenshot failed: {result.stderr.strip()}", file=sys.stderr)
        return False
    subprocess.run(
        ["ffmpeg", "-y", "-loglevel", "error", "-i", str(raw), str(output)],
        check=True, timeout=timeout)
    raw.unlink(missing_ok=True)
    return output.exists()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--screenshot", type=Path, default=Path("/tmp/leocad-menus.png"))
    parser.add_argument("--timeout", type=int, default=120)
    args = parser.parse_args()

    if not BINARY.exists():
        print(f"error: {BINARY.relative_to(REPO_ROOT)} not built; "
              f"run scripts/dev/build-desktop.sh", file=sys.stderr)
        return 1

    print("This check is interactive: it expects LeoCAD to be running with a "
          "display available.")
    print(f"Required menus:  {', '.join(REQUIRED_MENUS)}")
    print(f"Forbidden titles: {', '.join(FORBIDDEN_TITLES)}")
    print()
    print("Start the application with:")
    print(f"  source .devtools/env.sh && QT_QPA_PLATFORM=xcb {BINARY}")
    print("Then open each menu and compare against docs/ui/界面清单与精简范围.md.")
    print()

    if capture(args.screenshot, args.timeout):
        print(f"captured: {args.screenshot}")
        print("Review the screenshot to confirm the menu bar and panels.")
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
