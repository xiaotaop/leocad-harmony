#!/usr/bin/env python3
"""Keeps the three store-facing materials telling the same story.

Three documents go to the store and to users: docs/release/商店文案.md (listing),
docs/release/权限说明.md (permission review) and docs/release/隐私政策.md (user-facing
policy). They overlap on purpose -- the listing says what the app does, the
permission doc explains what it asks for, and the policy tells the user what
happens to their data. Overlap means they can disagree, and a store reviewer
reads all three.

They did disagree. Before D12 the privacy policy never mentioned permissions at
all: grep for the word returned nothing, so the one document a user actually
reads implied the app asks for nothing, while the package declared six
permissions with five of them carrying no reason string whatsoever.

What this checks:

  1. the permission set quoted by the listing and the policy matches the table;
  2. the privacy policy keeps a permission section at all;
  3. any part-count the listing quotes matches the shipped manifest;
  4. none of the documents claims something the project cannot support;
  5. a screenshot marked done has the files to back it up, so the status cannot
     be flipped before anyone actually takes the pictures.

Run with:
    python3 scripts/library/test_release_consistency.py
"""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

STORE_COPY = REPO / "docs" / "release" / "商店文案.md"
PERMISSIONS = REPO / "docs" / "release" / "权限说明.md"
PRIVACY = REPO / "docs" / "release" / "隐私政策.md"
MANIFEST = REPO / "assets" / "ldraw-lite" / "manifest.json"

EXPECTED_PERMISSIONS = (
    "ohos.permission.STORE_PERSISTENT_DATA",
    "ohos.permission.FILE_ACCESS_PERSIST",
    "ohos.permission.PREPARE_APP_TERMINATE",
    "ohos.permission.READ_PASTEBOARD",
    "ohos.permission.GET_FILE_ICON",
    "ohos.permission.PRINT",
)

PERMISSION_PATTERN = re.compile(r"ohos\.permission\.[A-Z_]+")

# Permissions the documents discuss specifically to say they are NOT requested.
# They belong in the prose but not in the declared set.
NOT_REQUESTED = (
    "ohos.permission.INTERNET",
    "ohos.permission.GET_NETWORK_INFO",
)


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


class ReleaseConsistencyTests(unittest.TestCase):
    def test_the_three_documents_agree_on_the_permission_set(self) -> None:
        permissions = set(PERMISSION_PATTERN.findall(read(PERMISSIONS))) - set(NOT_REQUESTED)
        self.assertEqual(
            permissions,
            set(EXPECTED_PERMISSIONS),
            "权限说明.md no longer lists exactly the six declared permissions; "
            "if the package really changed, update EXPECTED_PERMISSIONS here and "
            "the other two documents together",
        )

        for path in (STORE_COPY, PRIVACY):
            quoted = set(PERMISSION_PATTERN.findall(read(path))) - set(NOT_REQUESTED)
            self.assertEqual(
                quoted,
                set(EXPECTED_PERMISSIONS),
                f"{path.name} quotes a different permission set than the "
                "permission doc; the store reviewer reads them side by side",
            )

    def test_the_privacy_policy_keeps_a_permission_section(self) -> None:
        policy = read(PRIVACY)
        self.assertIn(
            "我们申请哪些权限",
            policy,
            "the privacy policy no longer has a permission section; the six "
            "declared permissions are then undocumented where users look",
        )
        self.assertIn(
            "不申请",
            policy,
            "the policy no longer states which permissions are NOT requested",
        )

    def test_quoted_part_count_matches_the_shipped_manifest(self) -> None:
        counts = json.loads(read(MANIFEST))["counts"]
        parts = counts["parts"]
        total = counts["total_files"]

        store = read(STORE_COPY)
        quoted = {
            int(value.replace(",", ""))
            for value in re.findall(r"([\d,]+)\s*件精选零件", store)
        }
        self.assertEqual(
            quoted,
            {parts},
            f"the listing quotes {sorted(quoted)} part(s) but the manifest ships "
            f"{parts}; fix the copy or the library",
        )

        totals = {
            int(value.replace(",", ""))
            for value in re.findall(r"([\d,]+)\s*个文件", store)
        }
        self.assertEqual(
            totals,
            {total},
            f"the listing quotes {sorted(totals)} total file(s) but the archive "
            f"holds {total}",
        )

    def test_no_document_claims_more_than_is_true(self) -> None:
        # Matched on the shortest claim, not the longest phrasing: an earlier
        # version keyed on "完整 LDraw 库" and happily accepted the sentence
        # "它是完整 LDraw 官方库的精简子集，是完整库。" because the words were
        # reordered. What matters is that any completeness claim is negated on
        # the same line.
        forbidden = {
            "完整库": "the shipped library is a 1160-part subset",
            "完整 LDraw": "the shipped library is a 1160-part subset",
            "官方发布": "this is a community port, not an upstream release",
            "已通过审核": "nothing has been submitted yet",
            "已上架": "the app is not on the store",
            "官方授权": "no rights holder has authorised this port",
        }

        for path in (STORE_COPY, PRIVACY):
            text = read(path)
            for phrase, reason in forbidden.items():
                for line in text.splitlines():
                    if phrase not in line:
                        continue
                    self.assertRegex(
                        line,
                        r"不得|不要|禁止|~~|不是|非",
                        f"{path.name} contains {phrase!r} without contradicting "
                        f"it nearby ({reason}): {line.strip()}",
                    )

    def test_screenshots_marked_done_have_files_behind_them(self) -> None:
        store = read(STORE_COPY)
        if "待拍" not in store:
            self.skipTest("every screenshot already delivered; nothing to guard")

        shot_dir = REPO / "docs" / "evidence" / "leader"
        delivered = sorted(
            str(path.relative_to(REPO))
            for path in shot_dir.rglob("*")
            if path.suffix.lower() in (".png", ".jpg", ".jpeg")
            and path.parent.name.startswith("d12")
        )

        pending = re.findall(r"^\| (\d\d) \|.*待拍.*\|$", store, re.M)
        self.assertTrue(
            pending,
            "the screenshot table no longer has [待拍] rows in the expected "
            "format; this check can no longer see the status",
        )

        self.assertEqual(
            delivered,
            [],
            "screenshots exist under docs/evidence/leader/*d12*/ but the store "
            "copy still marks them [待拍]; update the table and checklist "
            "together instead of leaving them out of step: "
            + ", ".join(delivered),
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
