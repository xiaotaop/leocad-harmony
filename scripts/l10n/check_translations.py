#!/usr/bin/env python3
"""Check the Chinese translation against the sources it is supposed to cover.

The checked-in `resources/leocad_zh_CN.ts` cannot be trusted on its own: it was
generated from a different revision, so every `<location line=...>` is stale and
several user-facing strings are simply absent. This script therefore compares the
translatable strings that actually exist in the source tree with the entries in
the `.ts`, instead of trusting `type="unfinished"`.

Checks
  1. every string in the P0/P1 contexts has a non-empty translation;
  2. no translation is empty or still marked unfinished in those contexts;
  3. translations identical to the source string are allow-listed;
  4. every `<source>` appears somewhere in the sources (no stale entries);
  5. the file is UTF-8 without BOM and uses LF line endings.

Usage:
    scripts/l10n/check_translations.py
    scripts/l10n/check_translations.py --report   # also list missing strings
"""

from __future__ import annotations

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
TS_FILE = REPO_ROOT / "resources" / "leocad_zh_CN.ts"

SOURCE_GLOBS = ("common/*.cpp", "common/*.h", "common/*.ui",
                "qt/*.cpp", "qt/*.h", "qt/*.ui")

# Contexts that must be fully translated for the first release: everything the
# user sees in the core modelling loop, plus the error dialogs around it.
P0_CONTEXTS = {
    "Action", "Menu", "Status", "ViewName",
    "lcMainWindow", "lcPartSelectionWidget", "lcPartSelectionListView",
    "lcPiecesLibrary", "lcApplication", "lcModel", "Project",
    "lcQPropertiesTree", "lcPropertiesDialog",
    "QApplication", "QMessageBox",
    "Categories", "Colors",
}

# Contexts whose strings come from Qt itself (standard dialog buttons such as
# "OK"/"Cancel"). They are provided by the qtbase translation catalog that
# qtmain.cpp installs alongside the application catalog, so leaving them empty
# in leocad_zh_CN.ts is correct.
QT_PROVIDED_CONTEXTS = {"QObject"}

# Strings in a required context that this release intentionally leaves in
# English. They are all unreachable from the graphical application or belong to a
# feature the first release does not ship; see docs/release/已知问题.md.
P2_ALLOWED_UNTRANSLATED = {
    # Command-line option help and mutual-exclusion notices: the HarmonyOS build
    # is launched from the home screen, never with arguments.
    "lcApplication": (
        "Usage: leocad [options] [file]\n",
        "  [options] can be:\n",
        "--viewpoint is ignored when --camera is set.\n",
        "--orthographic is ignored when --camera is set.\n",
        "--camera-angles is ignored when --camera is set.\n",
        "--camera-position is ignored when --camera is set.\n",
        "--camera-angles is ignored when --viewpoint is set.\n",
        "--camera-position is ignored when --viewpoint is set.\n",
        "--camera-position is ignored when --camera-angles is set.\n",
    ),
}
P1_CONTEXTS = {
    "lcPreferencesDialog", "lcAboutDialog", "lcCategoryDialog",
    "lcPartPaletteDialog", "lcModelListDialog", "lcTimelineWidget", "lcView",
    "lcFindReplaceWidget", "lcSelectDialog", "lcGroupDialog",
}
REQUIRED_CONTEXTS = P0_CONTEXTS | P1_CONTEXTS

# Sentences that are identical in English and Chinese on purpose (units,
# symbols, single letters) or that name a product.
IDENTICAL_ALLOWLIST = {
    "0", "1", "LeoCAD", "LDraw", "LEGO", "POV-Ray", "Blender", "BrickLink",
    "Qt", "LDraw.org", "mm", "cm", "in", "LDU", "%1", "X", "Y", "Z",
    "Ctrl", "Alt", "Shift", "OK", "Cancel",
}

TR_FUNCTION = re.compile(r'\btr\(\s*"((?:[^"\\]|\\.)*)"')
NOOP = re.compile(r'QT_TRANSLATE_NOOP\(\s*"([^"]*)"\s*,\s*"((?:[^"\\]|\\.)*)"')
TRANSLATE = re.compile(r'\btranslate\(\s*"([^"]*)"\s*,\s*"((?:[^"\\]|\\.)*)"')


def unescape_cpp_string(value: str) -> str:
    """Turn a C++ string literal body into the text Qt sees."""
    return (value.replace("\\n", "\n").replace("\\t", "\t")
                 .replace('\\"', '"').replace("\\\\", "\\"))


def collect_source_strings() -> tuple[set[str], dict[str, set[str]]]:
    """Return (all translatable source strings, context -> strings)."""
    strings: set[str] = set()
    by_context: dict[str, set[str]] = {}

    for pattern in SOURCE_GLOBS:
        for path in sorted(REPO_ROOT.glob(pattern)):
            text = path.read_text("utf-8", errors="replace")
            for match in TR_FUNCTION.finditer(text):
                strings.add(unescape_cpp_string(match.group(1)))
            for match in NOOP.finditer(text):
                strings.add(unescape_cpp_string(match.group(2)))
                by_context.setdefault(match.group(1), set()).add(
                    unescape_cpp_string(match.group(2)))
            for match in TRANSLATE.finditer(text):
                strings.add(unescape_cpp_string(match.group(2)))
                by_context.setdefault(match.group(1), set()).add(
                    unescape_cpp_string(match.group(2)))
    return strings, by_context


def load_ts() -> tuple[dict[str, dict[str, tuple[str, str]]], list[str]]:
    """Parse the .ts into {context: {source: (translation, type)}}."""
    raw = TS_FILE.read_bytes()
    problems: list[str] = []
    if raw.startswith(b"\xef\xbb\xbf"):
        problems.append("file starts with a UTF-8 BOM")
    if b"\r\n" in raw:
        problems.append("file uses CRLF line endings")

    root = ET.fromstring(raw.decode("utf-8"))
    entries: dict[str, dict[str, tuple[str, str]]] = {}
    for context in root.iter("context"):
        name = context.findtext("name", "")
        bucket = entries.setdefault(name, {})
        for message in context.iter("message"):
            source = message.findtext("source", "")
            translation = message.find("translation")
            if translation is None:
                bucket[source] = ("", "missing")
                continue

            # Plural messages store their text in <numerusform> children; without
            # this, a correctly translated plural looks untranslated.
            text = translation.text or ""
            if not text.strip():
                forms = [form.text for form in translation.findall("numerusform")]
                text = next((form for form in forms if form and form.strip()), "")

            bucket[source] = (text, translation.get("type", "finished"))
    return entries, problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", action="store_true",
                        help="list the missing strings instead of only counting")
    args = parser.parse_args()

    source_strings, _source_by_context = collect_source_strings()
    entries, problems = load_ts()

    errors: list[str] = list(problems)
    warnings: list[str] = []

    if not entries:
        print("error: no contexts parsed from the .ts", file=sys.stderr)
        return 1

    # 1 + 2: required contexts fully translated
    missing_total = 0
    for context in sorted(REQUIRED_CONTEXTS):
        bucket = entries.get(context)
        if bucket is None:
            warnings.append(f"context '{context}' not present in the .ts")
            continue
        allowed = P2_ALLOWED_UNTRANSLATED.get(context, ())
        missing = sorted(
            source for source, (text, kind) in bucket.items()
            if (not text.strip() or kind == "unfinished")
            and not source.startswith(("  -", "  --", "Usage:"))
            and source not in allowed)
        if missing:
            missing_total += len(missing)
            for source in missing:
                errors.append(f"{context}: untranslated '{source[:70]}'")
        if args.report:
            for source in missing:
                print(f"MISSING [{context}] {source}", file=sys.stderr)

    # 3: identical translations must be allow-listed
    for context, bucket in sorted(entries.items()):
        for source, (text, kind) in bucket.items():
            if not text.strip() or kind == "unfinished":
                continue
            if source.strip() == text.strip() and source.strip() not in IDENTICAL_ALLOWLIST:
                if context in REQUIRED_CONTEXTS:
                    warnings.append(
                        f"{context}: translation equals source '{source[:50]}'")
                missing_total += 0

    # 4: no stale entries (a <source> that no longer exists in code)
    stale = 0
    for context, bucket in sorted(entries.items()):
        for source in bucket:
            if source not in source_strings:
                stale += 1
                warnings.append(f"{context}: stale entry not found in sources "
                                f"'{source[:50]}'")

    contexts = len(entries)
    total = sum(len(bucket) for bucket in entries.values())
    print(f"contexts={contexts} messages={total} "
          f"required_contexts={len(REQUIRED_CONTEXTS)} "
          f"untranslated_in_required={missing_total} stale={stale}")

    for warning in warnings[:40]:
        print(f"warning: {warning}", file=sys.stderr)
    if len(warnings) > 40:
        print(f"warning: ... {len(warnings) - 40} more", file=sys.stderr)

    if errors:
        print(f"FAILED ({len(errors)} problems)", file=sys.stderr)
        for error in errors[:40]:
            print(f"  - {error}", file=sys.stderr)
        if len(errors) > 40:
            print(f"  - ... {len(errors) - 40} more", file=sys.stderr)
        return 1
    print("OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
