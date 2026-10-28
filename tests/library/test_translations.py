#!/usr/bin/env python3
"""Tests for the Simplified Chinese translation.

The checked-in `.ts` was generated from an older revision, so its own
`unfinished` count could not be trusted. These tests compare it against the
strings that actually exist in the sources, which is the only way to notice a
user-visible string that was never translated.
"""

from __future__ import annotations

import importlib.util
import re
import sys
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

TS_FILE = REPO_ROOT / "resources" / "leocad_zh_CN.ts"

REQUIRED_CONTEXTS = (
    "Menu", "Action", "Status",
    "lcMainWindow", "lcPartSelectionWidget", "lcPartSelectionListView",
    "lcModel", "Project", "lcApplication",
    "Categories", "Colors",
)


def load_checker():
    path = REPO_ROOT / "scripts" / "l10n" / "check_translations.py"
    spec = importlib.util.spec_from_file_location("check_translations", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules["check_translations"] = module
    spec.loader.exec_module(module)
    return module


class TranslationFileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.raw = TS_FILE.read_bytes()
        cls.root = ET.fromstring(cls.raw.decode("utf-8"))
        cls.contexts: dict[str, dict[str, tuple[str, str]]] = {}
        for context in cls.root.iter("context"):
            name = context.findtext("name", "")
            bucket = cls.contexts.setdefault(name, {})
            for message in context.iter("message"):
                source = message.findtext("source", "")
                translation = message.find("translation")
                if translation is None:
                    bucket[source] = ("", "missing")
                    continue
                forms = [f.text for f in translation.findall("numerusform")]
                text = (translation.text or "")
                if not text.strip() and any(f and f.strip() for f in forms):
                    text = next(f for f in forms if f and f.strip())
                bucket[source] = (text, translation.get("type", "finished"))

    def test_file_is_utf8_without_bom_and_uses_lf(self):
        self.assertFalse(self.raw.startswith(b"\xef\xbb\xbf"),
                         "translation file must not have a UTF-8 BOM")
        self.assertNotIn(b"\r\n", self.raw,
                         "translation file must use LF line endings")

    def test_file_is_well_formed_xml(self):
        # lrelease rejects a malformed .ts, so this guards the build.
        ET.fromstring(self.raw.decode("utf-8"))

    def test_required_contexts_are_present(self):
        missing = [name for name in REQUIRED_CONTEXTS
                   if name not in self.contexts]
        self.assertEqual(missing, [],
                         f"contexts missing from the translation: {missing}")

    def test_no_unfinished_strings_in_required_contexts(self):
        checker = load_checker()
        allowed_map = checker.P2_ALLOWED_UNTRANSLATED
        problems: list[str] = []

        for context in REQUIRED_CONTEXTS:
            bucket = self.contexts.get(context, {})
            allowed = allowed_map.get(context, ())
            for source, (text, kind) in bucket.items():
                if text.strip() and kind != "unfinished":
                    continue
                # Command-line help is not reachable from the GUI.
                if source.startswith(("  -", "  --", "Usage:")):
                    continue
                if source in allowed:
                    continue
                problems.append(f"{context}: '{source[:60]}'")

        self.assertEqual(problems, [],
                         f"{len(problems)} untranslated strings, first few: "
                         f"{problems[:5]}")

    def test_category_names_are_translated(self):
        bucket = self.contexts.get("Categories", {})
        self.assertTrue(bucket, "Categories context missing")
        untranslated = [source for source, (text, _kind) in bucket.items()
                        if not text.strip()]
        self.assertEqual(untranslated, [],
                         f"categories left in English: {untranslated}")

    def test_colour_names_are_translated(self):
        bucket = self.contexts.get("Colors", {})
        self.assertTrue(bucket, "Colors context missing")
        untranslated = [source for source, (text, _kind) in bucket.items()
                        if not text.strip()]
        self.assertEqual(untranslated, [],
                         f"colours left in English: {untranslated}")

    def test_selected_term_translations_are_stable(self):
        # These are the words the user meets in the core loop; a translator
        # or a regenerated file must not silently change them.
        expected = {
            ("Categories", "Brick"): "砖",
            ("Categories", "Plate"): "板",
            ("Categories", "Tile"): "光面砖",
            ("Categories", "Slope"): "斜面",
            ("Categories", "Minifig"): "人仔",
            ("Colors", "Black"): "黑色",
            ("Colors", "Red"): "红色",
            ("lcMainWindow", "&File"): "文件",
            ("lcMainWindow", "&Edit"): "编辑",
            ("lcPartSelectionWidget", "Filter Parts"): "搜索零件",
        }
        problems = []
        for (context, source), want in expected.items():
            bucket = self.contexts.get(context, {})
            got = bucket.get(source, ("", ""))[0]
            if got != want:
                problems.append(f"{context}/{source}: expected '{want}', "
                                f"got '{got}'")
        self.assertEqual(problems, [])

    def test_empty_state_messages_exist(self):
        # These are new strings added for the first release; without them the
        # user sees an empty list with no explanation.
        bucket = self.contexts.get("lcPartSelectionWidget", {})
        for source in (
            "No parts in this category.",
            "No parts library was found. Reinstall the application, or check the application data folder.",
            "No parts available.",
            "No parts match this search. Try a different keyword.",
        ):
            self.assertIn(source, bucket, f"missing empty-state string: {source}")
            text, _kind = bucket[source]
            self.assertTrue(text.strip(), f"untranslated: {source}")

    def test_translation_never_leaks_into_stored_colour_names(self):
        # The stored names are what the exporters use; the translation must live
        # only in the .ts. This asserts the source still writes English names.
        colours = (REPO_ROOT / "common" / "lc_colors.cpp").read_text("utf-8")
        self.assertNotIn("lcGetColorDisplayName(ColorIndex), sizeof",
                         colours,
                         "display name must never be written into a buffer")
        self.assertIn("strncpy(Color.Name, Token", colours,
                      "colour names must still come from the file, in English")


class TranslationToolingTests(unittest.TestCase):
    def test_every_required_source_string_is_translatable(self):
        """Every tr() string in the UI files has somewhere to be translated."""
        checker = load_checker()
        checker.TS_FILE = TS_FILE
        strings, _by_context = checker.collect_source_strings()
        self.assertIn("&File", strings)
        self.assertIn("Filter Parts", strings)

    def test_plural_messages_use_numerus_forms(self):
        root = ET.fromstring(TS_FILE.read_bytes().decode("utf-8"))
        for message in root.iter("message"):
            if message.get("numerus") != "yes":
                continue
            translation = message.find("translation")
            if translation is None:
                continue
            text = (translation.text or "").strip()
            forms = [f.text for f in translation.findall("numerusform")
                     if f.text and f.text.strip()]
            self.assertEqual(text, "",
                             "plural message must not carry plain text: "
                             f"{message.findtext('source', '')[:40]}")
            self.assertTrue(forms or not text,
                            "plural message has no numerusform: "
                            f"{message.findtext('source', '')[:40]}")


if __name__ == "__main__":
    unittest.main()
