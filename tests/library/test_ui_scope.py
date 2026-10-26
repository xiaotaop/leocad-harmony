#!/usr/bin/env python3
"""Tests that the first-release UI scope stays enforced.

Removing menu entries is easy to undo by accident: a later merge that adds an
action back, or an action added from a second menu, would silently re-expose a
feature the release does not support. These tests pin the scope down.

The static check reads the sources; `scripts/dev/verify_menu_scope.py` covers the
runtime half by comparing a screenshot of the running window.
"""

from __future__ import annotations

import re
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

MAINWINDOW_CPP = REPO_ROOT / "common" / "lc_mainwindow.cpp"
MAINWINDOW_H = REPO_ROOT / "common" / "lc_mainwindow.h"
COMMANDS_H = REPO_ROOT / "common" / "lc_commands.h"

# Command ids that must not be reachable in the first release. Kept in sync with
# the RemovedCommands[] table in ApplyFirstReleaseScope().
FORBIDDEN_COMMANDS = (
    "LC_FILE_PRINT",
    "LC_FILE_PRINT_PREVIEW",
    "LC_FILE_INSTRUCTIONS",
    "LC_FILE_EXPORT_POVRAY",
    "LC_FILE_EXPORT_3DS",
    "LC_FILE_EXPORT_COLLADA",
    "LC_FILE_EXPORT_HTML",
    "LC_FILE_RENDER_POVRAY",
    "LC_FILE_RENDER_BLENDER",
    "LC_VIEW_SPLIT_HORIZONTAL",
    "LC_VIEW_SPLIT_VERTICAL",
    "LC_VIEW_TIME_INSERT_BEFORE",
    "LC_VIEW_TIME_ADD_KEYS",
    "LC_PIECE_ARRAY",
    "LC_PIECE_MINIFIG_WIZARD",
    "LC_HELP_UPDATES",
)

# Commands the core modelling loop depends on and that must remain reachable.
REQUIRED_COMMANDS = (
    "LC_FILE_NEW",
    "LC_FILE_OPEN",
    "LC_FILE_SAVE",
    "LC_FILE_SAVEAS",
    "LC_EDIT_UNDO",
    "LC_EDIT_REDO",
    "LC_EDIT_ACTION_SELECT",
    "LC_EDIT_ACTION_MOVE",
    "LC_EDIT_ACTION_ROTATE",
    "LC_EDIT_ACTION_DELETE",
    "LC_EDIT_ACTION_PAINT",
    "LC_VIEW_ZOOM_EXTENTS",
    "LC_PIECE_INSERT",
)


def read(path: Path) -> str:
    return path.read_text("utf-8")


def function_body(source: str, signature: str) -> str:
    """Extract a function body by brace matching."""
    start = source.find(signature)
    if start < 0:
        return ""
    brace = source.find("{", start)
    if brace < 0:
        return ""
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    return ""


class MenuScopeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.cpp = read(MAINWINDOW_CPP)
        cls.create_menus = function_body(cls.cpp, "void lcMainWindow::CreateMenus()")
        cls.scope = function_body(cls.cpp,
                                  "void lcMainWindow::ApplyFirstReleaseScope()")

    def test_scope_function_exists_and_is_called(self):
        self.assertTrue(self.create_menus, "CreateMenus() not found")
        self.assertTrue(self.scope, "ApplyFirstReleaseScope() not found")
        self.assertIn("ApplyFirstReleaseScope();", self.create_menus,
                      "CreateMenus() must call ApplyFirstReleaseScope()")

    def test_forbidden_commands_are_listed_for_removal(self):
        missing = [command for command in FORBIDDEN_COMMANDS
                   if command not in self.scope]
        self.assertEqual(missing, [],
                         f"not removed by ApplyFirstReleaseScope(): {missing}")

    def test_required_commands_remain_in_the_core_menus(self):
        # These must still be added: dropping one breaks the modelling loop.
        missing = [command for command in REQUIRED_COMMANDS
                   if command not in self.create_menus]
        self.assertEqual(missing, [],
                         f"core commands no longer added to a menu: {missing}")

    def test_ctrl_n_and_save_shortcuts_survive(self):
        commands = read(COMMANDS_H)
        self.assertIn("LC_FILE_SAVE", commands)
        # The scope function must not remove the save/open/new trio.
        for command in ("LC_FILE_NEW", "LC_FILE_OPEN", "LC_FILE_SAVE"):
            self.assertNotIn(f"\t\t{command},\n", self.scope.split(
                "const lcCommandId RemovedCommands[]")[1].split("};")[0],
                f"{command} must not be removed")

    def test_dock_widget_pointers_are_not_nulled(self):
        """Removing a dock by nulling its pointer would crash later.

        UpdateDockWidgetActions() dereferences every dock pointer
        unconditionally, so the scope function must only remove menu entries.
        """
        self.assertNotIn("= nullptr", self.scope.replace("mActions", ""),
                         "ApplyFirstReleaseScope must not null pointers")
        self.assertNotIn("delete ", self.scope,
                         "ApplyFirstReleaseScope must not delete widgets")

        update_dock = function_body(self.cxx_get("UpdateDockWidgetActions"),
                                    "void lcMainWindow::UpdateDockWidgetActions()")
        for dock in ("mPartsToolBar", "mColorsToolBar", "mPropertiesToolBar",
                     "mTimelineToolBar", "mPreviewToolBar"):
            self.assertIn(dock, update_dock,
                          f"{dock} referenced by UpdateDockWidgetActions; "
                          "it must stay valid")

    def cxx_get(self, name: str) -> str:
        return self.cpp

    def test_scope_runs_after_tab_widget_exists(self):
        """Guard the ordering bug found during development.

        ApplyFirstReleaseScope() used to touch mModelTabWidget, which is created
        after CreateMenus(); that dereferenced a null pointer and crashed on
        startup with a parts library loaded.
        """
        create_widgets = function_body(self.cpp, "void lcMainWindow::CreateWidgets()")
        self.assertTrue(create_widgets, "CreateWidgets() not found")
        self.assertNotIn("mModelTabWidget", self.scope,
                         "ApplyFirstReleaseScope runs before mModelTabWidget "
                         "exists; move tab-bar setup into CreateWidgets()")

    def test_geometry_restore_is_guarded(self):
        """A restored geometry must be checked against existing screens."""
        self.assertIn("QGuiApplication::screens()", self.cpp,
                      "no screen check when restoring window geometry")
        self.assertIn("restoreGeometry", self.cpp)


class PreferencesScopeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ui = read(REPO_ROOT / "qt" / "lc_qpreferencesdialog.ui")
        cls.cpp = read(REPO_ROOT / "qt" / "lc_qpreferencesdialog.cpp")

    def test_language_selector_keeps_simplified_chinese(self):
        self.assertIn("\\u7b80\\u4f53\\u4e2d\\u6587", self.cpp,
                      "Simplified Chinese missing from the language list")

    def test_preferences_case_files_exist(self):
        for name in ("lc_qpreferencesdialog.ui", "lc_qpreferencesdialog.cpp"):
            self.assertTrue((REPO_ROOT / "qt" / name).exists())

    def test_removed_ui_widgets_have_no_cpp_references(self):
        """A widget removed from the .ui must not still be used in the .cpp.

        uic generates the header from the .ui, so a leftover `ui->name` becomes a
        compile error rather than a runtime one -- but only if the two stay in
        step. This catches the mismatch before a build.

        Layouts count too: they are generated as members and can be `delete`d in
        the .cpp (povrayLayout is), so they must be included in the known set.
        """
        widget_names = set(
            re.findall(r'<widget class="\w+" name="(\w+)"', self.ui))
        layout_names = set(
            re.findall(r'<layout class="\w+" name="(\w+)"', self.ui))
        known = widget_names | layout_names

        used = set(re.findall(r"ui->(\w+)", self.cpp))
        # Names that belong to the generated class itself rather than the .ui.
        allowed = {"setupUi", "retranslateUi"}
        undefined = sorted(used - known - allowed)
        self.assertEqual(undefined, [],
                         f"ui-> references with no widget or layout in the .ui: "
                         f"{undefined[:8]}")


if __name__ == "__main__":
    unittest.main()


class NetworkScopeTests(unittest.TestCase):
    """The first release must not reach the network.

    The update check is the one code path that would try: qtmain.cpp calls
    lcDoInitialUpdateCheck() unless LC_DISABLE_UPDATE_CHECK is defined, and that
    function requests https://www.leocad.org/updates.txt. leocad.pro only defines
    the macro for the macOS build, so a HarmonyOS build without it would attempt a
    network request at startup and break both the "no INTERNET permission" claim
    and the privacy policy.
    """

    @classmethod
    def setUpClass(cls) -> None:
        cls.qtmain = read(REPO_ROOT / "qt" / "qtmain.cpp")
        cls.pro = read(REPO_ROOT / "leocad.pro")
        cls.updatedialog = read(REPO_ROOT / "common" / "lc_updatedialog.cpp")

    def test_update_check_is_guarded_by_a_macro(self):
        self.assertIn("#if !LC_DISABLE_UPDATE_CHECK", self.qtmain,
                      "initial update check is no longer behind the macro; "
                      "verify how it is disabled on HarmonyOS")

    def test_update_dialog_contacts_the_network(self):
        """Documents why the macro matters, so the finding is not forgotten."""
        self.assertIn("https://", self.updatedialog,
                      "update dialog no longer downloads anything; "
                      "the permissions note may need updating")

    def test_macro_is_not_yet_defined_for_harmonyos(self):
        """Known gap, tracked in docs/release/权限说明.md section 3.1.

        This test asserts the current state so that when the team lead adds the
        define for the HarmonyOS build, it fails loudly and is tightened into a
        positive assertion instead of silently passing.
        """
        macx_block = function_body(self.pro.replace("{", " {"), "macx")
        self.assertIn("LC_DISABLE_UPDATE_CHECK", self.pro)

        # Count definitions: exactly one today, inside the macx block.
        definitions = self.pro.count("LC_DISABLE_UPDATE_CHECK=1")
        if definitions == 1:
            self.skipTest(
                "LC_DISABLE_UPDATE_CHECK=1 is only defined for macOS; the "
                "HarmonyOS build must define it too (see docs/release/权限说明.md)")
        self.assertGreaterEqual(
            definitions, 2,
            "expected the HarmonyOS build to define LC_DISABLE_UPDATE_CHECK=1")


class PreferencesTrimTests(unittest.TestCase):
    """The preferences dialog must actually hide the out-of-scope options.

    This was written after finding that docs/ui/界面清单与精简范围.md described the
    trim while lc_qpreferencesdialog.cpp still showed every option. A document
    claiming a behaviour the code does not implement is worse than no document,
    so the claim is asserted here.
    """

    # Widgets that must be hidden, with the reason they are out of scope.
    MUST_HIDE = {
        "checkForUpdates": "release must not reach the network",
        "partsLibrary": "library path is fixed in a packaged build",
        "partsLibraryBrowse": "library path is fixed in a packaged build",
        "partsArchiveBrowse": "library path is fixed in a packaged build",
        "ColorConfigEdit": "colour configuration travels with the package",
        "ColorConfigBrowseButton": "colour configuration travels with the package",
        "MinifigSettingsEdit": "templates are packaged",
        "MinifigSettingsBrowseButton": "templates are packaged",
        "povrayExecutable": "external renderers are out of scope",
        "povrayExecutableBrowse": "external renderers are out of scope",
        "lgeoPath": "external renderers are out of scope",
        "lgeoPathBrowse": "external renderers are out of scope",
        "BlenderAddonSettingsButton": "Blender is out of scope",
    }

    @classmethod
    def setUpClass(cls) -> None:
        cls.cpp = read(REPO_ROOT / "qt" / "lc_qpreferencesdialog.cpp")
        cls.ui = read(REPO_ROOT / "qt" / "lc_qpreferencesdialog.ui")
        cls.body = function_body(
            cls.cpp, "lcPreferencesDialog::lcPreferencesDialog")

    def test_trim_block_exists(self):
        self.assertTrue(self.body, "preferences dialog constructor not found")
        self.assertIn("RemovedPreferenceWidgets", self.cpp,
                      "no scope trim in the preferences dialog")

    @classmethod
    def trim_array(cls) -> str:
        """The RemovedPreferenceWidgets[] initialiser, not the whole constructor.

        Checking the whole constructor would pass even when a widget is dropped
        from the trim, because the same widget is also referenced by the code that
        reads and writes its value.
        """
        marker = "RemovedPreferenceWidgets[]"
        start = cls.body.find(marker)
        if start < 0:
            return ""
        brace = cls.body.find("{", start)
        depth = 0
        for index in range(brace, len(cls.body)):
            if cls.body[index] == "{":
                depth += 1
            elif cls.body[index] == "}":
                depth -= 1
                if depth == 0:
                    return cls.body[brace:index + 1]
        return ""

    def test_trim_array_is_populated(self):
        array = self.trim_array()
        self.assertTrue(array, "RemovedPreferenceWidgets[] not found")
        self.assertGreater(array.count("ui->"), 10,
                           "trim array looks empty; nothing would be hidden")

    def test_every_out_of_scope_widget_is_hidden(self):
        array = self.trim_array()
        missing = [name for name, reason in self.MUST_HIDE.items()
                   if f"ui->{name}," not in array]
        self.assertEqual(
            missing, [],
            "preferences options not hidden: " + ", ".join(
                f"{name} ({self.MUST_HIDE[name]})" for name in missing))

    def test_labels_are_hidden_with_their_fields(self):
        """Hiding a field in a QFormLayout leaves a floating caption behind."""
        array = self.trim_array()
        for label in ("label_5", "label_9", "label_10", "label_17", "label_18",
                      "label_39"):
            self.assertIn(f"ui->{label},", array,
                          f"{label} left visible next to a hidden field")

    def test_scope_options_kept_visible(self):
        """The options the release does support must not be caught in the trim."""
        for widget in ("Language", "ColorTheme", "authorName", "fixedDirectionKeys",
                       "ShadingMode", "antiAliasing", "studStyleCombo"):
            self.assertNotIn(f"ui->{widget},\n", self.body,
                             f"{widget} is in scope and must stay visible")

    def test_hidden_widgets_all_exist_in_the_ui(self):
        """A typo here would be a compile error; catch it before the build."""
        import re
        declared = set(
            re.findall(r'<widget class="\w+" name="(\w+)"', self.ui))
        referenced = set(re.findall(r"ui->(label_\d+|\w+),", self.body))
        unknown = sorted(referenced - declared)
        self.assertEqual(unknown, [],
                         f"ui-> references to non-existent widgets: {unknown}")


class EmptyStateOverlayTests(unittest.TestCase):
    """The parts panel's empty-state message must overlay, not displace, the list.

    Two defects were found here after the message was first written:
      1. the label was added to the panel's vertical layout, so showing it shrank
         the list and left the (empty) list visible underneath;
      2. it was parented to the QListView rather than its viewport, so it was
         painted outside the scrolling content area and did not cover the rows.

    Both are easy to reintroduce, so the structure is asserted directly.
    """

    @classmethod
    def setUpClass(cls) -> None:
        cls.cpp = read(REPO_ROOT / "common" / "lc_partselectionwidget.cpp")
        cls.ctor = function_body(cls.cpp, "lcPartSelectionWidget::lcPartSelectionWidget")

    def test_label_is_parented_to_the_viewport(self):
        # QListView is a QAbstractScrollArea; only viewport() children paint over
        # the rows and follow scrolling.
        self.assertIn("new QLabel(mPartsWidget->viewport())", self.ctor,
                      "empty-state label must be a child of the list viewport")

    def test_label_is_not_added_to_the_vertical_layout(self):
        self.assertNotIn("PartsLayout->addWidget(mEmptyStateLabel)", self.ctor,
                         "adding the label to the layout shrinks the list instead "
                         "of covering it")

    def test_geometry_follows_the_viewport(self):
        self.assertIn("mEmptyStateLabel->setGeometry(mPartsWidget->viewport()->rect())",
                      self.cpp,
                      "overlay geometry must track the viewport, not the view")

    def test_geometry_is_updated_on_resize_and_on_show(self):
        resize_body = function_body(self.cpp, "void lcPartSelectionWidget::resizeEvent")
        update_body = function_body(self.cpp, "void lcPartSelectionWidget::UpdateEmptyState")
        for name, body in (("resizeEvent", resize_body),
                           ("UpdateEmptyState", update_body)):
            self.assertIn("setGeometry", body,
                          f"{name} must reposition the overlay")

    def test_label_does_not_steal_focus_or_clicks_when_hidden(self):
        # A hidden widget receives neither, but it must actually be hidden on the
        # success path; otherwise it would swallow clicks on the rows.
        update_body = function_body(self.cpp, "void lcPartSelectionWidget::UpdateEmptyState")
        self.assertIn("mEmptyStateLabel->hide()", update_body,
                      "overlay must be hidden when parts are visible")

    def test_all_three_empty_cases_have_distinct_messages(self):
        update_body = function_body(self.cpp, "void lcPartSelectionWidget::UpdateEmptyState")
        # Library missing, category empty, filter matched nothing.
        for marker in ("No parts library was found",
                       "No parts in this category",
                       "No parts match this search"):
            self.assertIn(marker, update_body,
                          f"missing empty-state branch: {marker}")
