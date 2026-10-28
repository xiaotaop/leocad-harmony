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

private:
	// True when some visible QAction carrying this command's label exists. The
	// action objects stay allocated after removal (the dock widgets dereference
	// them), so visibility is the signal, not existence.
	bool IsCommandVisible(lcCommandId Command) const;

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
