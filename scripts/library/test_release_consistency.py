#!/usr/bin/env python3
"""Keeps the three store-facing materials telling the same story.

Three documents go to the store and to users: docs/release/商店文案.md (listing),
docs/release/权限说明.md (permission review) and docs/release/隐私政策.md (user-facing
policy). They overlap on purpose -- the listing says what the app does, the
permission doc explains what it asks for, and the policy tells the user what
happens to their data. Overlap means they can disagree, and a store reviewer
reads all three.

The existing privacy policy already listed six permissions. D12 makes their
full identifiers explicit and checks that the three documents stay consistent.

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

import base64
import json
import re
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

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

    def assertScreenshotEvidence(self, store: str, repo: Path) -> None:
        rows = re.findall(r"^\|\s*(\d{2})\s*\|[^|]*\|[^|]*\|\s*([^|]+)\|\s*$", store, re.M)
        self.assertEqual([number for number, _ in rows], ["01", "02", "03", "04", "05", "06"])
        for number, status in rows:
            if status.strip() == "`[待拍]`":
                continue
            link = re.fullmatch(r"\[已完成\]\(([^)]+)\)", status.strip())
            self.assertIsNotNone(link, f"截图 {number} 须为待拍或链接到图片的 [已完成](相对路径)")
            image = (repo / "docs" / "release" / link.group(1)).resolve()
            leader = (repo / "docs" / "evidence" / "leader").resolve()
            self.assertTrue(image.is_relative_to(leader), f"截图 {number} 必须位于组长证据目录")
            self.assertEqual(image.parent.name, "store-shots")
            self.assertRegex(image.name, rf"^leocad-harmony-.+-{number}-.+\.png$")
            self.assertTrue(image.is_file(), f"缺少截图 {number}: {image}")
            self.assertTrue(image.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"), f"截图 {number} 必须为 PNG")
            manifest = image.parent / "manifest.json"
            self.assertTrue(manifest.is_file(), f"截图 {number} 缺少 manifest.json")
            self.assertTrue(json.loads(read(manifest)), f"截图 {number} 清单不能为空")

    def test_screenshots_marked_done_have_files_behind_them(self) -> None:
        self.assertScreenshotEvidence(read(STORE_COPY), REPO)

    def test_completed_screenshots_require_evidence(self) -> None:
        table = "\n".join(f"| {i:02} | 场景 | 要点 | `[待拍]` |" for i in range(1, 7))
        with TemporaryDirectory() as directory:
            repo = Path(directory)
            self.assertScreenshotEvidence(table, repo)
            with self.assertRaises(AssertionError):
                self.assertScreenshotEvidence(table.replace("待拍", "已完成"), repo)
            with self.assertRaises(AssertionError):
                self.assertScreenshotEvidence(table.split("\n", 1)[1], repo)
            partial = table.replace("`[待拍]`",
                "[已完成](../evidence/leader/2026-11-07/store-shots/leocad-harmony-0.1.0-01-main.png)", 1)
            with self.assertRaises(AssertionError):
                self.assertScreenshotEvidence(partial, repo)
            shots = repo / "docs/evidence/leader/2026-11-07/store-shots"
            shots.mkdir(parents=True)
            # Known 1x1 PNG used only to exercise this file-presence guard.
            png = base64.b64decode(
                "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aB9sAAAAASUVORK5CYII=")
            (shots / "leocad-harmony-0.1.0-01-main.png").write_bytes(png)
            with self.assertRaises(AssertionError):
                self.assertScreenshotEvidence(partial, repo)
            (shots / "manifest.json").write_text('{"files":["leocad-harmony-0.1.0-01-main.png"]}', encoding="utf-8")
            self.assertScreenshotEvidence(partial, repo)
            complete = "\n".join(
                f"| {i:02} | 场景 | 要点 | [已完成](../evidence/leader/2026-11-07/store-shots/leocad-harmony-0.1.0-{i:02}-main.png) |"
                for i in range(1, 7))
            with self.assertRaises(AssertionError):
                self.assertScreenshotEvidence(complete, repo)
            files = [f"leocad-harmony-0.1.0-{i:02}-main.png" for i in range(1, 7)]
            for filename in files:
                (shots / filename).write_bytes(png)
            (shots / "manifest.json").write_text(json.dumps({"files": files}), encoding="utf-8")
            self.assertScreenshotEvidence(complete, repo)


if __name__ == "__main__":
    unittest.main(verbosity=2)
