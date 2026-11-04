// Verifies the first-release menu scope removes what it claims, on a real
// lcMainWindow.
//
// Reading ApplyFirstReleaseScope() only shows intent. What matters is the menu
// the user actually sees, so this builds a window and inspects it: a command
// listed for removal but re-added elsewhere, or a command that must stay but is
// removed by a careless edit, fails here instead of shipping.
//
// QTEST_MAIN needs a real platform plugin; run it with QT_QPA_PLATFORM=offscreen
// on a machine without a display.

#include <QAction>
#include <QMenu>
#include <QTabWidget>
#include <QToolBar>
#include <QtTest>

#include "lc_application.h"
#include "lc_commands.h"
#include "lc_library.h"
#include "lc_mainwindow.h"

class MenuScopeTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();

	void removedCommandsAreNotVisible();
	void keptCommandsAreVisible();
	void submodelListIsKept();
	void modelTabBarIsNotClosable();
	void modelTabBarIsNotMovable();

	// Removing every QAction from a submenu leaves the submenu itself behind: an
	// "&Import" or "&Export" entry that opens an empty box. The command checks
	// above cannot see this, because the removed actions are invisible as
	// intended — the container is what is wrong.
	void noVisibleEmptySubmenus();

	// The same failure for toolbars, reported on the Z6 device review. Every
	// command the Time toolbar holds is out of scope, so the scope empties it —
	// and Qt keeps drawing the empty toolbar as a blank strip reserved in the
	// layout. The menu tests above cannot see this: no command is visible that
	// should not be, the toolbar itself is the leftover.
	void noVisibleEmptyToolbars();

	// The toggle for that toolbar has to go with it, otherwise the View menu
	// offers a checkbox that switches an empty strip on and off.
	void emptiedToolbarToggleIsNotOffered();

	// The toolbars that still have commands must survive: hiding every toolbar
	// with no visible action would be an easy overreach.
	void toolbarsWithCommandsStayVisible();

private:
	// True when some visible QAction carrying this command's label exists. The
	// action objects stay allocated after removal (the dock widgets dereference
	// them), so visibility is the signal, not existence.
	bool IsCommandVisible(lcCommandId Command) const;

	// Every menu the user can actually open, in the state CreateMenus() leaves
	// them. Submenus are descended into through their menu action, so a submenu
	// whose action was hidden is not reported as an empty box.
	QList<QMenu*> VisibleMenuTree() const;

	// Titles of the menus in the tree that have no visible entry at all.
	QStringList EmptyMenus(const QList<QMenu*>& Menus) const;

	lcMainWindow* mWindow = nullptr;
};

void MenuScopeTest::initTestCase()
{
	// The window cannot be built on its own: CreateWidgets() reaches into the
	// widgets library through lcGetPiecesLibrary(), which dereferences
	// gApplication. So set up the same two things the real startup does, minus the
	// full Initialize() (no project, no command line, no dialogs):
	//
	//   lcApplication  sets gApplication and owns mLibrary;
	//   LoadPartsLibrary creates the library and loads a subset for thumbnails.
	//
	// QTEST_MAIN has already created the QApplication, so the tests build the
	// window on top of the one QApplication instead of a second one.
	QVERIFY(gApplication);

	if (!gApplication->mLibrary)
	{
		const QString Archive = QFINDTESTDATA("../../../assets/ldraw-lite/library-lite.zip");
		QVERIFY2(!Archive.isEmpty(), "bundled library not found");

		QList<QPair<QString, bool>> Paths;
		Paths += qMakePair(Archive, true);
		QVERIFY2(gApplication->LoadPartsLibrary(Paths, true), "could not load the parts library");
	}

	mWindow = new lcMainWindow();
	QVERIFY(mWindow);

	mWindow->CreateWidgets();
}

bool MenuScopeTest::IsCommandVisible(lcCommandId Command) const
{
	const QString Label = QString::fromLatin1(gCommands[Command].MenuName);

	for (QAction* Action : mWindow->findChildren<QAction*>())
		if (Action->isVisible() && Action->text() == Label)
			return true;

	return false;
}

void MenuScopeTest::removedCommandsAreNotVisible()
{
	// One from each group the scope covers: file output, non-core formats,
	// external renderers, step navigation, find/replace, multi-view, submodel
	// editing, and the online update check.
	const lcCommandId Removed[] =
	{
		LC_FILE_PRINT,
		LC_FILE_PRINT_PREVIEW,
		LC_FILE_INSTRUCTIONS,
		LC_FILE_SAVE_IMAGE,
		LC_FILE_IMPORT_LDD,
		LC_FILE_EXPORT_3DS,
		LC_FILE_EXPORT_COLLADA,
		LC_FILE_EXPORT_CSV,
		LC_FILE_EXPORT_POVRAY,
		LC_FILE_EXPORT_WAVEFRONT,
		LC_FILE_RENDER_POVRAY,
		LC_VIEW_TIME_NEXT,
		LC_EDIT_FIND,
		LC_VIEW_SPLIT_VERTICAL,
		LC_MODEL_NEW,
		LC_PIECE_EDIT_SELECTED_SUBMODEL,
		LC_HELP_UPDATES,

		// Help entries that open a browser on an external https URL. They were
		// reachable until the D8 freeze review: LC_DISABLE_NETWORK only stubs out
		// lc_http, so QDesktopServices still launched the browser and the offline
		// promise in the privacy policy did not hold. Asserted here as well as in
		// the static network-scope check because this one looks at the real menu.
		LC_HELP_HOMEPAGE,
		LC_HELP_BUG_REPORT,

		// The step commands reachable from the timeline panel's context menu.
		// They are a second set of command ids for the same operations as the
		// LC_VIEW_TIME_* entries above, and were missed for exactly that reason:
		// the first version of the scope removed one set and left the other
		// reachable with two clicks.
		LC_TIMELINE_SET_CURRENT,
		LC_TIMELINE_INSERT_BEFORE,
		LC_TIMELINE_INSERT_AFTER,
		LC_TIMELINE_DELETE,
		LC_TIMELINE_MOVE_SELECTION,
		LC_TIMELINE_MOVE_SELECTION_BEFORE,
		LC_TIMELINE_MOVE_SELECTION_AFTER,
	};

	for (const lcCommandId Command : Removed)
		QVERIFY2(!IsCommandVisible(Command),
			qPrintable(QStringLiteral("%1 is still visible in a menu")
				.arg(QString::fromLatin1(gCommands[Command].MenuName))));
}

void MenuScopeTest::keptCommandsAreVisible()
{
	const lcCommandId Kept[] =
	{
		LC_FILE_NEW,
		LC_FILE_OPEN,
		LC_FILE_SAVE,
		LC_EDIT_UNDO,
		LC_EDIT_REDO,
	};

	for (const lcCommandId Command : Kept)
		QVERIFY2(IsCommandVisible(Command),
			qPrintable(QStringLiteral("%1 disappeared from the menus")
				.arg(QString::fromLatin1(gCommands[Command].MenuName))));
}

void MenuScopeTest::submodelListIsKept()
{
	// D1 section 1.3: MPD submodel switching has to survive, and LC_MODEL_LIST is
	// its second entry point. Losing it would leave only the tab bar.
	QVERIFY2(IsCommandVisible(LC_MODEL_LIST),
		"submodel list removed; MPD switching loses a menu entry");
}

void MenuScopeTest::modelTabBarIsNotClosable()
{
	// Take the central widget explicitly rather than finding any QTabWidget: the
	// dock area also produces tab widgets, and a future change could make this
	// test inspect the wrong one.
	QTabWidget* Tabs = qobject_cast<QTabWidget*>(mWindow->centralWidget());
	QVERIFY2(Tabs, "central widget is not the model tab widget");

	// Closing the last tab calls NewProject(), which is not a single-document
	// interaction, so the close box has to be gone.
	QVERIFY(!Tabs->tabsClosable());
}

void MenuScopeTest::modelTabBarIsNotMovable()
{
	QTabWidget* Tabs = qobject_cast<QTabWidget*>(mWindow->centralWidget());
	QVERIFY2(Tabs, "central widget is not the model tab widget");

	// Dragging a tab would reorder the MPD submodels.
	QVERIFY(!Tabs->isMovable());
}

void MenuScopeTest::noVisibleEmptyToolbars()
{
	// Measured via isVisible(), not isHidden(): the window is never shown under
	// QT_QPA_PLATFORM=offscreen, so isVisible() is false for every child widget
	// and would make this test vacuous. isHidden() reflects the explicit hide()
	// the scope performs, which is the thing under test.
	QStringList Blank;

	for (QToolBar* ToolBar : mWindow->findChildren<QToolBar*>())
	{
		if (ToolBar->parentWidget() != mWindow)
			continue;

		bool HasVisibleAction = false;

		for (QAction* Action : ToolBar->actions())
		{
			if (Action->isVisible() && !Action->isSeparator())
			{
				HasVisibleAction = true;
				break;
			}
		}

		if (!HasVisibleAction && !ToolBar->isHidden())
			Blank.append(ToolBar->windowTitle().isEmpty() ? QStringLiteral("<untitled>") : ToolBar->windowTitle());
	}

	QVERIFY2(Blank.isEmpty(),
		qPrintable(QStringLiteral("toolbars left visible with no command in them: %1").arg(Blank.join(", "))));
}

void MenuScopeTest::emptiedToolbarToggleIsNotOffered()
{
	// The Time toolbar is the one the scope empties: all five of its actions
	// (LC_VIEW_TIME_FIRST/PREVIOUS/NEXT/LAST/ADD_KEYS) are removed. Asserting on
	// the concrete toolbar rather than on "whatever ended up empty" keeps this
	// test meaningful if the removal list changes.
	QToolBar* Time = mWindow->findChild<QToolBar*>("TimeToolbar");
	QVERIFY2(Time, "TimeToolbar not found");

	QVERIFY2(Time->isHidden(), "the emptied Time toolbar is still visible as a blank strip");

	// Hidden, not deleted: the dock widgets and UpdateDockWidgetActions()
	// dereference these actions, so the object has to stay alive.
	//
	// Found through the View menu rather than by label across the whole window:
	// the toolbar's own windowTitle() is "Time" too, and the label is what the
	// user reads in the menu, which is the thing under test.
	const QString Label = QString::fromLatin1(gCommands[LC_VIEW_TOOLBAR_TIME].MenuName);
	QAction* Toggle = nullptr;
	QMenu* ToolBarsMenu = nullptr;

	for (QAction* BarAction : mWindow->menuBar()->actions())
	{
		QMenu* Menu = BarAction->menu();
		if (!Menu)
			continue;
		for (QAction* Entry : Menu->actions())
			if (Entry->menu() && Entry->text().contains(QStringLiteral("oolbar")))
				ToolBarsMenu = Entry->menu();
	}

	QVERIFY2(ToolBarsMenu, "the View > Toolbars submenu is gone");
	for (QAction* Entry : ToolBarsMenu->actions())
		if (Entry->text() == Label)
			Toggle = Entry;

	QVERIFY2(Toggle, qPrintable(QStringLiteral("toggle action %1 disappeared; it must stay allocated").arg(Label)));
	QVERIFY2(!Toggle->isVisible(), "the View menu still offers a toggle for an empty toolbar");
	QVERIFY2(!Toggle->isEnabled(), "the toggle for an empty toolbar is still clickable");
}

void MenuScopeTest::toolbarsWithCommandsStayVisible()
{
	// The Standard toolbar is the control: it holds file/edit actions, none of
	// which the scope removes, so it must still be a live toolbar after the
	// sweep. It is also the reference for "should be visible" — the Tools
	// toolbar is hidden by CreateToolBars() itself (lc_mainwindow.cpp:1024) and
	// so cannot serve as one.
	QToolBar* Standard = mWindow->findChild<QToolBar*>("StandardToolbar");
	QVERIFY2(Standard, "StandardToolbar not found");
	QVERIFY2(!Standard->isHidden(), "a toolbar with commands was hidden by the empty-toolbar sweep");

	int Visible = 0;
	for (QAction* Action : Standard->actions())
		if (Action->isVisible() && !Action->isSeparator())
			Visible++;

	QVERIFY2(Visible > 0, "the control toolbar reports no visible action; the test would be vacuous");

	// And the sweep must not have taken the control's toggle away.
	const QString Label = QString::fromLatin1(gCommands[LC_VIEW_TOOLBAR_STANDARD].MenuName);
	QAction* Toggle = nullptr;

	for (QAction* Action : mWindow->findChildren<QAction*>())
		if (Action->text() == Label && !Action->isSeparator())
			Toggle = Action;

	QVERIFY2(Toggle, "the Standard toolbar toggle disappeared");
	QVERIFY2(Toggle->isVisible(), "the sweep hid a toolbar toggle that still has a toolbar behind it");
}

QList<QMenu*> MenuScopeTest::VisibleMenuTree() const
{
	// Start from the menu bar, not from findChildren<QMenu*>(): that also returns
	// menus that were never added to the bar, and menus whose parent menu action
	// is hidden. Walking down from the bar is the same order a user sees.
	QList<QMenu*> Menus;
	QList<QMenu*> Pending;

	QMenuBar* Bar = mWindow->menuBar();

	if (Bar == nullptr)
		return Menus;

	for (QAction* Action : Bar->actions())
		if (QMenu* Menu = Action->menu())
			Pending.append(Menu);

	while (!Pending.isEmpty())
	{
		QMenu* Menu = Pending.takeFirst();
		Menus.append(Menu);

		for (QAction* Action : Menu->actions())
		{
			QMenu* SubMenu = Action->menu();

			// A submenu reached only through a hidden action cannot be opened,
			// so it is not part of what the user sees.
			if (SubMenu && Action->isVisible())
				Pending.append(SubMenu);
		}
	}

	return Menus;
}

QStringList MenuScopeTest::EmptyMenus(const QList<QMenu*>& Menus) const
{
	QStringList Empty;

	for (QMenu* Menu : Menus)
	{
		bool HasVisibleEntry = false;

		for (QAction* Action : Menu->actions())
		{
			if (Action->isVisible() && !Action->isSeparator())
			{
				HasVisibleEntry = true;
				break;
			}
		}

		if (!HasVisibleEntry)
			Empty.append(Menu->title().isEmpty() ? QStringLiteral("<untitled>") : Menu->title());
	}

	return Empty;
}

void MenuScopeTest::noVisibleEmptySubmenus()
{
	const QList<QMenu*> Menus = VisibleMenuTree();
	const QStringList Empty = EmptyMenus(Menus);

	QVERIFY2(Empty.isEmpty(),
		qPrintable(QStringLiteral("submenus with no visible entry: %1").arg(Empty.join(", "))));
}

// Custom main rather than QTEST_MAIN: the window needs the LeoCAD application
// object, because CreateWidgets() reaches the parts library through
// lcGetPiecesLibrary() and that dereferences gApplication. QTEST_MAIN would
// create a plain QApplication and leave gApplication null.
int main(int argc, char** argv)
{
	lcApplication Application(argc, argv);
	MenuScopeTest Test;

	return QTest::qExec(&Test, argc, argv);
}

#include "tst_menu_scope.moc"
