#!/usr/bin/env python3
"""Keeps the D11 release materials attached to the repository they describe.

The D11 deliverables are documents: an icon freeze table, the store copy, the
license inventory, and the source-acquisition note. A document is only worth
freezing if something fails when the repository moves out from under it. Three
ways they drift, all silent without this file:

  1. an icon is re-exported and the freeze table keeps quoting the old hash,
  2. the store copy starts advertising a name or version the build does not set,
  3. a license claim the project is not entitled to make gets written back in
     after being removed (the "version 2 or later" reading).

None of those are compile errors and none of them look wrong in review, because
each file is internally consistent while contradicting another one. So the
comparisons happen here, between files, on every push.

Run with:
    python3 scripts/library/test_release_materials.py
"""

from __future__ import annotations

import hashlib
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

CMAKE_FILE = REPO / "CMakeLists.txt"
ICON_FREEZE = REPO / "docs" / "release" / "图标冻结.md"
STORE_COPY = REPO / "docs" / "release" / "商店文案.md"
LICENSE_INVENTORY = REPO / "docs" / "release" / "第三方组件清单.md"
SOURCE_NOTE = REPO / "docs" / "release" / "源码获取说明.md"
ICON_DIR = REPO / "tools" / "icon"

# The three scripts the source note used to claim could rebuild the parts
# library. They have never existed in this repository; the claim survived because
# nothing read the sentence. Kept here so it cannot come back.
PHANTOM_LIBRARY_SCRIPTS = (
    "build_library.py",
    "generate_selection.py",
    "verify_library.py",
)

# Backtick, spelled out so this file does not need a literal one.
TICK = chr(96)


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


# .gitattributes sets "* text=auto", so a text icon is CRLF in a Windows
# checkout and LF in CI. Hashing the worktree bytes would then make the freeze
# table platform-dependent and fail on the platform it was not recorded on --
# which is exactly what happened the first time this check ran in CI. Binary
# icons are hashed as they are; text ones are normalised to LF first, so the
# recorded value means the same thing everywhere.
TEXT_ICON_SUFFIXES = (".svg",)


def icon_bytes(path: Path) -> bytes:
    data = path.read_bytes()
    if path.suffix.lower() in TEXT_ICON_SUFFIXES:
        data = data.replace(b"\r\n", b"\n")
    return data


def sha256(path: Path) -> str:
    return hashlib.sha256(icon_bytes(path)).hexdigest()


def frozen_icon_rows() -> list[tuple[str, int, str]]:
    """(path, bytes, sha256) for every freeze-document row naming a file."""

    rows = []

    for line in read(ICON_FREEZE).splitlines():
        if not line.startswith("|"):
            continue

        cells = [
            cell.strip().strip(TICK + "*")
            for cell in line.strip().strip("|").split("|")
        ]
        if len(cells) < 3:
            continue

        sha = next((c for c in cells if re.fullmatch(r"[0-9a-f]{64}", c)), None)
        if sha is None:
            continue

        size = next((c for c in cells if re.fullmatch(r"[\d,]+", c)), None)
        if size is None:
            continue

        # A path has no whitespace; the "how it ships" cells mention file names
        # too ("alias resources/leocad.png"), so that is what keeps them apart.
        name = next(
            (
                c
                for c in cells
                if " " not in c and (c.endswith(".png") or c.endswith(".svg"))
            ),
            None,
        )
        if name is None:
            continue

        if not name.startswith("tools/icon/"):
            name = "tools/icon/" + name

        rows.append((name, int(size.replace(",", "")), sha))

    return rows


class ReleaseMaterialTests(unittest.TestCase):
    def test_icon_freeze_hashes_match_the_files_on_disk(self) -> None:
        """A re-exported icon has to fail here, not in the store review.

        The freeze document is the only place pinning what the shipped pixels
        are. If it quietly points at stale hashes it certifies nothing, which is
        worse than not having it: it reads like a verified inventory.
        """

        rows = frozen_icon_rows()
        self.assertGreaterEqual(
            len(rows),
            16,
            f"only {len(rows)} icon row(s) parsed from the freeze document; it is "
            "supposed to list all 16 files under tools/icon",
        )

        seen = set()

        for name, expected_bytes, expected_sha in rows:
            relative = name[len("tools/icon/"):]
            path = ICON_DIR / relative
            self.assertTrue(
                path.is_file(),
                f"{name} is listed in the freeze document but is not on disk",
            )

            actual_bytes = len(icon_bytes(path))
            self.assertEqual(
                actual_bytes,
                expected_bytes,
                f"{name} is {actual_bytes} bytes but the freeze document says "
                f"{expected_bytes}",
            )

            actual_sha = sha256(path)
            self.assertEqual(
                actual_sha,
                expected_sha,
                f"{name} changed. If that was intentional, update its row in "
                "docs/release/图标冻结.md — and for the 64/256/512 sizes also redo "
                "the device check in docs/HAP_ICON_DEVICE_ACCEPTANCE.md — before "
                "landing the new file.",
            )
            seen.add(relative)

        on_disk = {
            str(p.relative_to(ICON_DIR)).replace("\\", "/")
            for p in ICON_DIR.rglob("*")
            if p.is_file()
        }
        self.assertEqual(
            sorted(on_disk - seen),
            [],
            "these files exist under tools/icon but have no row in the freeze "
            "table, so nothing would notice them changing",
        )

    def test_store_copy_identity_matches_the_build(self) -> None:
        """The store form is filled from the copy, the package from CMake."""

        cmake = read(CMAKE_FILE)

        def setting(name: str) -> str:
            match = re.search(name + r'\s+"([^"]+)"', cmake)
            self.assertIsNotNone(
                match, f"{name} is no longer set in CMakeLists.txt"
            )
            return match.group(1)

        app_name = setting("QT_HARMONYOS_APP_NAME")
        version_name = setting("QT_HARMONYOS_APP_VERSION_NAME")
        bundle = setting("QT_HARMONYOS_APP_BUNDLE_NAME")

        store = read(STORE_COPY)
        self.assertTrue(store, "could not read the store copy")

        self.assertIn(
            app_name,
            store,
            f"the store copy never mentions the application name the build sets "
            f"({app_name!r}); the listing and the package would disagree",
        )
        self.assertIn(
            version_name,
            store,
            f"the store copy does not mention versionName {version_name!r} from "
            "CMakeLists.txt, so the version the form asks for is unstated",
        )
        self.assertIn(
            bundle,
            store,
            f"the store copy does not record the bundle name {bundle!r}",
        )

    def test_no_unauthorized_or_later_reading(self) -> None:
        """The project is not allowed to upgrade its own license.

        docs/ui/D1_界面清点与首发范围原则.md section 5.3 rules that "version 2"
        must not be read as "version 2 or later" without the rights holder's
        written consent. The inventory once proposed exactly that as a candidate
        path. Prose may quote the prohibition; it must not propose the reading as
        a course of action, which is what the removed phrasings looked like.
        """

        inventory = read(LICENSE_INVENTORY)
        self.assertTrue(inventory, "could not read the third-party inventory")

        forbidden = (
            "可以作为 or later",
            "可按 or later 处理",
            "补写 or later",
            "自行补充 or later",
        )

        for phrase in forbidden:
            self.assertNotIn(
                phrase,
                inventory,
                f"the inventory proposes {phrase!r}, which D1 section 5.3 "
                "forbids: the version choice belongs to the copyright holder",
            )

        self.assertIn(
            "不把",
            inventory,
            "the inventory no longer records why the or-later reading is refused; "
            "the next editor would have no stated reason not to reintroduce it",
        )

    def test_source_note_does_not_promise_missing_scripts(self) -> None:
        """Promising a rebuild path that does not exist is worse than a gap."""

        note = read(SOURCE_NOTE)
        self.assertTrue(note, "could not read the source-acquisition note")

        present = [
            name
            for name in PHANTOM_LIBRARY_SCRIPTS
            if (REPO / "scripts" / "library" / name).is_file()
        ]
        self.assertEqual(
            present,
            [],
            "these scripts now exist, so the note may describe them as a rebuild "
            "path; update both sides together: " + ", ".join(present),
        )

        paragraphs = re.split(r"\n\s*\n", note)

        for name in PHANTOM_LIBRARY_SCRIPTS:
            for paragraph in paragraphs:
                if name not in paragraph:
                    continue
                self.assertRegex(
                    paragraph,
                    "不存在|未提供|缺口|无法重建|不得|无法生成",
                    f"{name} is cited in a paragraph that never says the script is "
                    "missing, so it reads as a rebuild path that works: "
                    + paragraph.strip()[:200],
                )


if __name__ == "__main__":
    unittest.main(verbosity=2)
