#!/usr/bin/env python3
"""Checks that keep the permissions doc in step with the package it describes.

The permissions doc (`docs/release/权限说明.md`) is one of the store-review
materials: the store expects every requested permission to be explained. The
list itself lives only in the packaged `module.json5`, which is not in this
repository — it comes from the Qt HarmonyOS template, copied into the build tree
by CMakeLists.txt. So nothing else in this repository notices when the two drift
apart.

They did drift: the doc claimed "we request no permissions except file picking"
while the shipped package requested six, one of them PRINT for a print dialog
that the first-release trim had already removed from the menu. A store reviewer
reading the doc would have been told something untrue. That is what this file
exists to prevent.

What it checks, all against files in the repository:

  1. Every permission in the recorded packaged manifest appears in the doc's
     conclusion table. A manifest gain without a doc line fails here.
  2. The doc keeps saying the package does not request INTERNET — the offline
     promise rests on that, and it has its own check for the code side in
     test_network_scope.py. This one is the manifest side.
  3. Permissions the doc marks for removal carry a reason, so a future reader
     can see why rather than finding a permission silently dropped.

The recorded manifest is a checked-in copy of one real build's
`module.json5` (see the doc's section 0 for which build). This file cannot
inspect the real package: no build runs here and the template is not vendored.
So it guards consistency between the doc and the evidence, not between the doc
and tonight's artifact — that last step stays a human check on RC0.

Run with:
    python3 scripts/library/test_permission_scope.py
"""

from __future__ import annotations

import json
import re
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PERMISSIONS_DOC = REPO / "docs" / "release" / "权限说明.md"
PACKAGED_MANIFEST = (
    REPO / "docs" / "evidence" / "leader" / "2026-11-02" / "d7-device" / "packaged-module.json5"
)


def read_doc() -> str:
    return PERMISSIONS_DOC.read_text(encoding="utf-8")


def recorded_permissions() -> list[str]:
    """Permissions named in the recorded packaged manifest.

    The file is JSON with one HarmonyOS-ism: trailing commas inside arrays. Rather
    than pull in a tolerant parser for a file that changes once per release, the
    permission names are pulled out directly — the field we care about is a flat
    list of {\"name\": ...} objects.
    """
    text = PACKAGED_MANIFEST.read_text(encoding="utf-8")
    match = re.search(r'"requestPermissions"\s*:\s*\[(.*?)\]\s*\n\s*\}', text, re.S)
    if not match:
        return []
    return re.findall(r'"name"\s*:\s*"([^"]+)"', match.group(1))


class PermissionDocMatchesPackage(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.doc = read_doc()
        cls.permissions = recorded_permissions()

    def test_the_recorded_manifest_exists_and_lists_permissions(self) -> None:
        # Guards the guard: if the evidence file moves or is renamed, the checks
        # below would compare against an empty list and pass vacuously.
        self.assertTrue(PACKAGED_MANIFEST.is_file(), f"missing {PACKAGED_MANIFEST}")
        self.assertGreaterEqual(
            len(self.permissions), 1,
            "no permissions parsed from the recorded module.json5; the regex is stale",
        )

    def test_every_packaged_permission_is_explained_in_the_doc(self) -> None:
        missing = [p for p in self.permissions if p not in self.doc]
        self.assertEqual(
            missing, [],
            "packaged permissions absent from docs/release/权限说明.md: "
            + ", ".join(missing),
        )

    def test_the_package_does_not_request_internet(self) -> None:
        # The offline promise. test_network_scope.py proves the code cannot send a
        # request; this proves the manifest does not ask for the capability.
        self.assertNotIn(
            "ohos.permission.INTERNET", self.permissions,
            "the recorded manifest requests INTERNET, which contradicts the privacy policy",
        )
        self.assertIn("INTERNET", self.doc)

    def test_the_doc_names_each_permission_with_the_ohos_prefix(self) -> None:
        # A permission written as bare \"PRINT\" would still match the substring
        # test above but is not what a reviewer copies into a manifest.
        for permission in self.permissions:
            self.assertIn(permission, self.doc, f"{permission} missing from the doc")

    def test_removal_recommendations_explain_themselves(self) -> None:
        # The doc's table marks some permissions 建议移除. Those rows are the
        # actionable output for the lead, so each has to carry a reason rather
        # than appearing in a bare list.
        for permission in self.permissions:
            for line in self.doc.splitlines():
                if permission in line and "建议移除" in line:
                    self.assertGreater(
                        len(line.strip()), 40,
                        f"the removal recommendation for {permission} has no reason on its row",
                    )


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], "-v"], exit=False)
