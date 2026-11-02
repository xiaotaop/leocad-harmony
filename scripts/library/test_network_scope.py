#!/usr/bin/env python3
"""Checks that keep the "no network in the first release" claim honest.

The privacy policy and the permissions doc both promise the app never sends a
request and never asks for INTERNET. That promise does not rest on the absence of
network code — common/lc_http.cpp is there and Qt Network is linked — it rests on
every entry point to that code being unreachable. Two things can quietly break it:

  1. the first-release trim stops removing a networked command, or
  2. the update check is left enabled in a build that is not macOS.

Neither shows up as a test failure anywhere else, and both are invisible until a
store reviewer watches the app talk to leocad.org. So they are checked here,
against the source rather than against a running build: the HarmonyOS build flags
are not in this repository, which is exactly why the second check exists.

Run with:
    python3 scripts/library/test_network_scope.py
"""

from __future__ import annotations

import re
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

MAINWINDOW = REPO / "common" / "lc_mainwindow.cpp"
QTMAIN = REPO / "qt" / "qtmain.cpp"
PROFILE = REPO / "common" / "lc_profile.cpp"
PROJECT_FILE = REPO / "leocad.pro"

# Commands that reach lc_http.cpp. Removing them from the menus is what makes the
# offline claim true, so each one has to stay in the trim list.
NETWORKED_COMMANDS = (
    "LC_FILE_RENDER_BLENDER",
    "LC_FILE_RENDER_OPEN_IN_BLENDER",
    "LC_FILE_IMPORT_INVENTORY",
)


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def removed_commands(source: str) -> list[str]:
    """The ids inside lcMainWindow::ApplyFirstReleaseScope()'s array."""

    start = source.find("void lcMainWindow::ApplyFirstReleaseScope()")
    if start == -1:
        return []

    opening = source.find("{", source.find("RemovedCommands", start))
    closing = source.find("};", opening)
    if opening == -1 or closing == -1:
        return []

    body = source[opening:closing]
    body = re.sub(r"//[^\n]*", "", body)  # comments mention ids too
    return re.findall(r"\bLC_[A-Z0-9_]+\b", body)


def qmake_scopes(source: str) -> dict[str, str]:
    """Top-level qmake scope name -> its body text.

    Only enough of qmake to answer "is line N inside macx or inside
    unix:!macx". Anything nested deeper is folded into the enclosing scope, which
    is what the macro check wants.
    """

    scopes: dict[str, str] = {}
    depth = 0
    current: str | None = None
    body: list[str] = []

    for line in source.splitlines():
        stripped = line.strip()

        if depth == 0 and stripped.endswith("{") and "=" not in stripped:
            current = stripped[:-1].strip()
            body = []
            depth = 1
            continue

        if current is not None:
            depth += stripped.count("{") - stripped.count("}")
            if depth <= 0:
                scopes[current] = "\n".join(body)
                current = None
                depth = 0
            else:
                body.append(line)

    return scopes


class NetworkScopeTests(unittest.TestCase):
    def test_networked_commands_are_trimmed(self) -> None:
        """Every command that downloads something must be unreachable."""

        removed = removed_commands(read(MAINWINDOW))
        self.assertTrue(removed, "could not find the first-release trim list")

        missing = [c for c in NETWORKED_COMMANDS if c not in removed]
        self.assertEqual(
            missing,
            [],
            "these commands reach lc_http.cpp but are still in the menus, so the "
            "offline claim in the privacy policy is false: " + ", ".join(missing),
        )

    def test_networked_commands_still_exist(self) -> None:
        """A renamed constant would make the check above pass by accident.

        If LC_FILE_RENDER_BLENDER were renamed tomorrow, the trim list would no
        longer contain it, but neither would anything else — and the first test
        would still fail, which is what we want. What this guards is subtler: the
        id has to keep being a real command, or the trim silently removes nothing.
        """

        commands = read(REPO / "common" / "lc_commands.h")
        self.assertTrue(commands, "could not read lc_commands.h")

        for command in NETWORKED_COMMANDS:
            self.assertIn(
                command,
                commands,
                f"{command} is no longer a command id; if it was renamed, update "
                "NETWORKED_COMMANDS or this check goes blind",
            )

    def test_update_check_needs_the_macro(self) -> None:
        """The startup path must still be behind the macro, not merely trimmed."""

        source = read(QTMAIN)
        self.assertIn(
            "LC_DISABLE_UPDATE_CHECK",
            source,
            "the update check is no longer guarded; the trim list cannot cover a "
            "call made from main()",
        )

        guarded = re.search(
            r"#if\s*!\s*LC_DISABLE_UPDATE_CHECK\s*\n\s*lcDoInitialUpdateCheck\(\)",
            source,
        )
        self.assertIsNotNone(
            guarded,
            "lcDoInitialUpdateCheck() is not inside an #if !LC_DISABLE_UPDATE_CHECK "
            "block any more",
        )

    def test_update_check_default_is_on(self) -> None:
        """Documents why the macro matters: the default is to check.

        If this ever changes to 0, the P0 in the known-issues list is no longer
        accurate and should be revisited rather than left to look scary.
        """

        source = read(PROFILE)
        match = re.search(
            r'lcProfileEntry\("Settings",\s*"CheckUpdates",\s*(\d+)\)', source
        )
        self.assertIsNotNone(match, "could not find the CheckUpdates default")

        default = int(match.group(1))
        self.assertEqual(
            default,
            1,
            "the CheckUpdates default changed; docs/release/已知问题.md §5.1 says "
            "it is 1 (check daily) and that first launch therefore goes online",
        )

    def test_macro_is_not_defined_for_everyone(self) -> None:
        """The macx-only definition is the reason HarmonyOS cannot rely on it."""

        scopes = qmake_scopes(read(PROJECT_FILE))

        macx = scopes.get("macx", "")
        self.assertIn(
            "LC_DISABLE_UPDATE_CHECK",
            macx,
            "macx no longer defines the macro; re-check whether any platform is "
            "left unprotected",
        )

        unix_not_macx = scopes.get("unix:!macx", "")
        self.assertNotIn(
            "DEFINES += LC_DISABLE_UPDATE_CHECK=1",
            unix_not_macx,
            "unix:!macx now defines the macro unconditionally — if that was "
            "deliberate, the P0 can be closed, but the docs must change too",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
