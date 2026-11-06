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
against the source rather than against a running build.

The build flags ARE in this repository: CMakeLists.txt is the project the
HarmonyOS build script drives (scripts/build-harmony-app.ps1 calls qt-cmake on
the repository root), and it defines LC_DISABLE_UPDATE_CHECK=1 for the leocad
target unconditionally, plus LC_DISABLE_NETWORK=1 under OHOS. The qmake project
is the historical path and only defended the macro on macOS, which is why the
CMake target is what these tests assert on. Removing either definition has to
fail here — that is the regression this file exists to catch.

Note what this does NOT establish: the update check being compiled out is one
part of the offline claim. Whether the packaged app requests the INTERNET
permission is a separate manifest question, checked by the release docs, not by
this file.

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
CMAKE_FILE = REPO / "CMakeLists.txt"
HARMONY_BUILD_SCRIPT = REPO / "scripts" / "build-harmony-app.ps1"

# Commands that reach lc_http.cpp. Removing them from the menus is what makes the
# offline claim true, so each one has to stay in the trim list.
NETWORKED_COMMANDS = (
    "LC_FILE_RENDER_BLENDER",
    "LC_FILE_RENDER_OPEN_IN_BLENDER",
    "LC_FILE_IMPORT_INVENTORY",
)

# Commands that leave the app for an external https URL through
# QDesktopServices::openUrl. Not lc_http, so LC_DISABLE_NETWORK does not touch
# them: the browser still opens, and the "no network requests" line in the
# privacy policy would be false. They were in the Help menu with no guard at all
# until the D8 interface freeze review. Kept separate from NETWORKED_COMMANDS
# because the mechanism that has to catch a regression differs — that one is the
# trim list, this one is also the trim list, but for a different reason worth
# naming in the failure message.
EXTERNAL_URL_COMMANDS = (
    "LC_HELP_HOMEPAGE",
    "LC_HELP_BUG_REPORT",
)


# A .ui label with openExternalLinks=true turns an http(s) anchor into a working
# "open this in the browser" control. The trim list cannot see it: the link lives
# in the widget file, not in a menu command. Both of these shipped enabled until
# the D11 release-material review, which is exactly the gap this check closes.
#
# The one file below is excluded because it cannot be shown: lcUpdateDialog is
# only ever constructed from lcDoInitialUpdateCheck(), which the CMake target
# compiles out (LC_DISABLE_UPDATE_CHECK=1) and whose menu command LC_HELP_UPDATES
# is in the trim list. Two independent reasons, both asserted here, so the
# exclusion cannot outlive them.
PRIVACY_POLICY = REPO / "docs" / "release" / "隐私政策.md"
STORE_COPY = REPO / "docs" / "release" / "商店文案.md"
ICON_FREEZE = REPO / "docs" / "release" / "图标冻结.md"

UNREACHABLE_CLICKABLE_LINKS = {
    "common/lc_updatedialog.ui": (
        "lcUpdateDialog has no reachable entry point: LC_HELP_UPDATES is trimmed "
        "and lcDoInitialUpdateCheck() is behind #if !LC_DISABLE_UPDATE_CHECK"
    ),
}

UI_DIRECTORIES = ("common", "qt")


def clickable_link_labels(path: Path) -> list[str]:
    """Text of every label in a .ui file that enables openExternalLinks."""

    source = read(path)
    texts = []

    for match in re.finditer(
        r'<widget class="QLabel" name="[^"]*">(.*?)</widget>', source, re.S
    ):
        body = match.group(1)
        if not re.search(
            r'<property name="openExternalLinks">\s*<bool>true</bool>', body
        ):
            continue
        text = re.search(r"<string[^>]*>(.*?)</string>", body, re.S)
        texts.append(text.group(1) if text else "")

    return texts


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


def target_definitions(source: str) -> dict[str, dict[str, list]]:
    """Compile definitions per CMake target, unconditional vs platform-gated.

    Handles what this CMakeLists.txt actually contains: a
    target_compile_definitions(<target> PRIVATE ...) call whose arguments can
    span several lines and may sit inside a top-level if(...) ... endif(). The
    two categories are kept apart because "always defined" and "defined only
    under if(OHOS)" are different promises — the qmake history this replaced
    confused them, with the macro living in a macx-only block.

    Conditions are recorded verbatim so a test names the branch it expects
    instead of accepting any nesting at all.
    """

    targets: dict[str, dict[str, list]] = {}
    conditions: list[str] = []
    pending: list[str] | None = None

    for line in source.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue

        if pending is not None:
            pending.append(stripped)
            if ")" not in stripped:
                continue
            call_text, pending = " ".join(pending), None
        else:
            opened = re.match(r"if\((.+)\)\s*$", stripped)
            if opened is not None:
                conditions.append(opened.group(1).strip())
                continue
            if stripped.startswith("endif"):
                if conditions:
                    conditions.pop()
                continue
            call = re.match(r"target_compile_definitions\(\s*(\S+)", stripped)
            if call is None:
                continue
            if ")" in stripped:
                call_text = stripped
            else:
                pending = [stripped]
                continue

        target = re.match(r"target_compile_definitions\(\s*(\S+)", call_text).group(1)
        body = call_text
        if "PRIVATE" in body:
            body = body[body.index("PRIVATE") + len("PRIVATE"):]
        body = re.sub(r"\$<[^>]*>", " ", body)
        names = re.findall(r"\bLC_[A-Z0-9_]+=\d+\b", body)

        entry = targets.setdefault(target, {"definitions": [], "conditional": []})
        if conditions:
            entry["conditional"].extend((conditions[-1], name) for name in names)
        else:
            entry["definitions"].extend(names)

    return targets


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

    def test_external_url_commands_are_trimmed(self) -> None:
        """Help entries that open a browser must not ship either.

        LC_HELP_HOMEPAGE and LC_HELP_BUG_REPORT call QDesktopServices::openUrl on
        an https address. That is Qt GUI, not Qt Network, so LC_DISABLE_NETWORK=1
        does not stub it out — the app itself requests nothing, but it launches a
        browser that does, which is still a network action the privacy policy
        says the first release does not take. Both were reachable in the Help
        menu until the D8 freeze; this keeps them out.
        """

        removed = removed_commands(read(MAINWINDOW))
        self.assertTrue(removed, "could not find the first-release trim list")

        missing = [c for c in EXTERNAL_URL_COMMANDS if c not in removed]
        self.assertEqual(
            missing,
            [],
            "these commands open an external https URL through "
            "QDesktopServices and are back in a menu: " + ", ".join(missing),
        )

    def test_external_url_trim_matches_the_actual_call_sites(self) -> None:
        """The list above has to track the code, not the other way round.

        If another QDesktopServices::openUrl(http…) call is added to a menu later,
        the trim list silently stops covering it. This fails on the new call site,
        naming the command, instead of waiting for a reviewer to notice.
        """

        source = read(MAINWINDOW)
        calls = re.findall(r"QDesktopServices::openUrl\(\s*QUrl\(\s*\"https?://", source)
        self.assertEqual(
            len(calls),
            len(EXTERNAL_URL_COMMANDS),
            f"{len(calls)} external-URL call site(s) in lc_mainwindow.cpp but "
            f"{len(EXTERNAL_URL_COMMANDS)} command(s) listed for trimming; every "
            "openUrl(http...) needs its command in EXTERNAL_URL_COMMANDS",
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

    def test_cmake_disables_the_update_check_for_leocad(self) -> None:
        """The real build defines the macro, so the P0 does not ship.

        CMakeLists.txt is what scripts/build-harmony-app.ps1 builds, and it puts
        LC_DISABLE_UPDATE_CHECK=1 on the leocad target outside any generator
        expression or platform branch — the HarmonyOS app therefore cannot call
        lcDoInitialUpdateCheck(). If someone removes that line, the app starts
        contacting leocad.org on first launch and the offline claim in the
        privacy policy becomes false; that is the failure this test must raise.

        Parsed rather than grepped: the same macro name also appears inside the
        OHOS-only and model-test blocks, and a grep would stay green after the
        unconditional definition was deleted, as long as one of those remained.
        """

        source = read(CMAKE_FILE)
        blocks = target_definitions(source)

        self.assertIn(
            "leocad",
            blocks,
            "could not find a target_compile_definitions() call for the leocad target",
        )

        leocad = blocks["leocad"]
        self.assertIn(
            "LC_DISABLE_UPDATE_CHECK=1",
            leocad["definitions"],
            "the leocad CMake target no longer defines LC_DISABLE_UPDATE_CHECK=1 "
            "unconditionally; the HarmonyOS build would perform the startup "
            "update check against leocad.org",
        )
        self.assertNotIn(
            "LC_DISABLE_UPDATE_CHECK=1",
            [name for _, name in leocad["conditional"]],
            "LC_DISABLE_UPDATE_CHECK moved into an OS branch; the guarantee has to "
            "hold for the HarmonyOS target without relying on a condition",
        )

    def test_cmake_harmony_target_also_disables_network(self) -> None:
        """OHOS additionally compiles out the network layer entirely."""

        source = read(CMAKE_FILE)
        leocad = target_definitions(source)["leocad"]

        self.assertIn(
            ("OHOS", "LC_DISABLE_NETWORK=1"),
            leocad["conditional"],
            "LC_DISABLE_NETWORK=1 is no longer defined under if(OHOS); the "
            "HarmonyOS build would link and use Qt Network",
        )

    def test_cmake_disables_the_remaining_first_release_features(self) -> None:
        """The trim is belt and braces: these gates must stay compiled out too."""

        source = read(CMAKE_FILE)
        leocad = target_definitions(source)["leocad"]["definitions"]

        for gate in (
            "LC_DISABLE_RENDER_DIALOG=1",
            "LC_DISABLE_BLENDER=1",
        ):
            self.assertIn(
                gate,
                leocad,
                f"{gate} is no longer compiled into the leocad target; the "
                "first-release scope trim and this macro are meant to agree",
            )

    def test_no_reachable_dialog_has_a_clickable_external_link(self) -> None:
        """A label can open a browser without any command being involved.

        openExternalLinks on a label holding an http anchor is a live link the
        trim list cannot remove, because no command backs it. The About dialog
        and the Sets Database dialog (reachable from the part panel context menu
        through Edit Palettes... -> Import...) both shipped that way until the
        D11 review. Every remaining one has to be justified as unreachable.
        """

        offenders = []

        for directory in UI_DIRECTORIES:
            for path in sorted((REPO / directory).rglob("*.ui")):
                if not clickable_link_labels(path):
                    continue
                if path.relative_to(REPO).as_posix() in UNREACHABLE_CLICKABLE_LINKS:
                    continue
                offenders.append(str(path.relative_to(REPO)))

        self.assertEqual(
            offenders,
            [],
            "these .ui files enable openExternalLinks, so a label in them opens a "
            "browser on click — a network action the privacy policy says the first "
            "release does not take. Make the text plain, or add the file to "
            "UNREACHABLE_CLICKABLE_LINKS with the reason it cannot be shown: "
            + ", ".join(offenders),
        )

    def test_the_excluded_clickable_link_is_still_unreachable(self) -> None:
        """The exemption above must expire when its two reasons do.

        If the update dialog ever becomes reachable again, the allowlist entry
        would silently hide a live external link. Both guards are re-asserted
        here so removing either one fails this test first.
        """

        self.assertIn(
            "LC_HELP_UPDATES",
            removed_commands(read(MAINWINDOW)),
            "LC_HELP_UPDATES is back in a menu, so lcUpdateDialog is reachable and "
            "its clickable link is no longer covered by UNREACHABLE_CLICKABLE_LINKS",
        )

        self.assertRegex(
            read(QTMAIN),
            r"#if\s*!\s*LC_DISABLE_UPDATE_CHECK[\s\S]*?lcDoInitialUpdateCheck\(\)",
            "lcDoInitialUpdateCheck() is no longer compiled out, so the update "
            "dialog can appear at startup and its clickable link ships live",
        )

    def test_privacy_policy_describes_the_clickable_link_rule(self) -> None:
        """The policy has to name the mechanism, not just the outcome.

        The .ui links were found by reading the file, not by any check, and a
        policy that only says "no external URLs" would have been true-looking
        while both links shipped live. This keeps the description attached to the
        check that now enforces it.
        """

        policy = read(PRIVACY_POLICY)
        self.assertTrue(policy, "could not read the privacy policy")

        self.assertIn(
            "openExternalLinks",
            policy,
            "the privacy policy no longer explains that the .ui labels themselves "
            "can open a browser; if that paragraph was dropped, the rule it "
            "describes is undocumented even though the check still runs",
        )

    def test_harmony_build_script_uses_this_cmake_project(self) -> None:
        """The constraint above only protects the app if the app is built here.

        Asserts the script configures with a CMake source of the repository root —
        if it ever gains its own flags file or a separate project, the definitions
        this file guarantees would stop applying to the shipped binary.
        """

        source = read(HARMONY_BUILD_SCRIPT)
        self.assertTrue(source, "could not read scripts/build-harmony-app.ps1")

        configured = re.search(r"\$qtCMake\s+-S\s+(\S+)", source)
        self.assertIsNotNone(
            configured,
            "the HarmonyOS build script no longer runs qt-cmake with -S; re-check "
            "which project it builds before trusting the CMake definitions",
        )

        source_dir = configured.group(1)
        self.assertIn(
            source_dir,
            (
                "$repositoryRoot",
                ".",
                "${repositoryRoot}",
                "$PSScriptRoot",
                "..",
            ),
            f"the script configures CMake from {source_dir!r}, which is not the "
            "repository root — the leocad target definitions may no longer apply",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
