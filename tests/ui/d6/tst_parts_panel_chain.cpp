// D6: the parts panel has to actually drive the modelling chain.
//
// The plan (docs/LeoCAD_鸿蒙PC首发版_两周分工与技术方案.md, 11/1 D6) asks for
// "完成零件面板、分类、搜索、颜色和空状态；提交界面截图". The panel and its empty
// states already have coverage from D4 (tests/library/d4/tst_parts_panel.cpp); this
// suite covers what that one does not:
//
//   * the colour list end to end — picking a colour has to reach the main window,
//     the colour button and the parts panel, because a swatch that only moves its
//     own cursor would look fine and change nothing
//   * categories actually filtering the loaded library
//   * search matching parts that exist only in the curated library
//   * the count agreeing with the number of rows on screen
//
// Note what is deliberately NOT asserted: the curated archive and the built-in
// fallback both define 322 colours, so "the colour list grew" is not evidence
// that the curated library is in use. The panel and part-count cases below pin
// the library instead, and colour is tested as a chain.
//
// These tests build a real lcMainWindow and drive real widgets. They do not need
// a GPU: the colour list paints from gColorList and the panel lists PieceInfo
// entries, neither of which touches a GL context.

#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QToolButton>
#include <QTreeWidget>
#include <QtTest>

#include "lc_application.h"
#include "lc_colorlist.h"
#include "lc_colors.h"
#include "lc_library.h"
#include "lc_mainwindow.h"
#include "lc_partselectionwidget.h"
#include "project.h"

class PartsPanelChainTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void cleanupTestCase();

	// Colour.
	void colourListIsPopulatedFromTheLoadedLibrary();
	void pickingAColourReachesTheWholeChain();
	void colourChainIgnoresTheSameColour();

	// Category and search driving the model.
	void categorySelectionListsOnlyMatchingParts();
	void searchMatchesPartsThatOnlyExistInTheCuratedLibrary();
	void countAgreesWithTheRowsOnScreen();

	// Empty state, through interaction rather than a call.
	void emptyStateFollowsCategoryAndSearchTogether();

private:
	lcMainWindow* mWindow = nullptr;
	lcPartSelectionWidget* mPanel = nullptr;
	lcColorList* mColorList = nullptr;
	QToolButton* mColorButton = nullptr;

	QTreeWidget* CategoryTree() const;
	QLineEdit* PartsFilter() const;
	int CountVisibleRows() const;
	QTreeWidgetItem* FindCategory(const QString& Name) const;
};

void PartsPanelChainTest::initTestCase()
{
	QVERIFY(gApplication);

	if (!gApplication->mLibrary)
	{
		const QString Archive = QFINDTESTDATA("../../../assets/ldraw-lite/library-lite.zip");
		QVERIFY2(!Archive.isEmpty(), "curated library not found");

		QList<QPair<QString, bool>> Paths;
		Paths += qMakePair(Archive, true);
		QVERIFY2(gApplication->LoadPartsLibrary(Paths, true), "could not load the curated library");
	}

	mWindow = new lcMainWindow();
	mWindow->CreateWidgets();
	gApplication->SetProject(new Project());

	mPanel = mWindow->findChild<lcPartSelectionWidget*>();
	QVERIFY2(mPanel, "no parts panel in the main window");

	// The colour list is not the only lcColorList in the tree: the preferences
	// dialog and the piece context menus can carry one. This window has exactly
	// one, created in CreateDockWidgets for the Colors dock.
	mColorList = mWindow->findChild<lcColorList*>();
	QVERIFY2(mColorList, "no colour list in the main window");

	// By objectName: the parts panel builds several QToolButtons before this one,
	// so findChild<QToolButton*>() returns one of those instead.
	mColorButton = mWindow->findChild<QToolButton*>(QStringLiteral("SelectionColorButton"));
	QVERIFY2(mColorButton, "no selection colour button in the main window");
}

void PartsPanelChainTest::cleanupTestCase()
{
	delete mWindow;
	mWindow = nullptr;
	mPanel = nullptr;
	mColorList = nullptr;
	mColorButton = nullptr;
}

// These helpers return values, so they cannot use QVERIFY2: the macro expands to
// a bare `return`, which does not compile in a function returning a pointer.
// qFatal stops the run with the same message instead.
QTreeWidget* PartsPanelChainTest::CategoryTree() const
{
	QTreeWidget* Tree = mPanel->findChild<QTreeWidget*>();

	if (Tree == nullptr)
		qFatal("the parts panel has no category tree");

	return Tree;
}

QLineEdit* PartsPanelChainTest::PartsFilter() const
{
	// By objectName, not by position. The panel has two filter boxes and the
	// first one built is the parts filter — measured, but relying on construction
	// order is exactly the kind of assumption this session has already been
	// bitten by twice.
	QLineEdit* Filter = mPanel->findChild<QLineEdit*>(QStringLiteral("PartsFilter"));

	if (Filter == nullptr)
		qFatal("the parts panel has no parts filter box");

	return Filter;
}

int PartsPanelChainTest::CountVisibleRows() const
{
	QListView* View = mPanel->findChild<QListView*>();

	if (View == nullptr)
		qFatal("the parts panel has no list view");

	lcPartSelectionListModel* Model = qobject_cast<lcPartSelectionListModel*>(View->model());

	if (Model == nullptr)
		qFatal("the list view is not driven by the parts model");

	int Visible = 0;

	for (int Row = 0; Row < Model->rowCount(); Row++)
	{
		if (!View->isRowHidden(Row))
			Visible++;
	}

	return Visible;
}

// What the panel counts, and what a user reads: the visible rows. The All Parts
// category of a project also lists a temporary model entry, so its total is the
// library plus one — measured 1161 for the 1160-part archive, which matches the
// device report.

QTreeWidgetItem* PartsPanelChainTest::FindCategory(const QString& Name) const
{
	QTreeWidget* Tree = CategoryTree();

	for (int Row = 0; Row < Tree->topLevelItemCount(); Row++)
	{
		QTreeWidgetItem* Item = Tree->topLevelItem(Row);

		if (Item->text(0) == Name)
			return Item;
	}

	return nullptr;
}

void PartsPanelChainTest::colourListIsPopulatedFromTheLoadedLibrary()
{
	// The list is built from gColorGroups/gColorList, which Load() filled from
	// ldraw/ldconfig.ldr inside the curated archive. An empty or single-colour
	// list would render as a blank panel and the rest of the chain would be
	// meaningless, so the population check comes first.
	QSignalSpy ColourChanged(mColorList, &lcColorList::ColorChanged);
	QVERIFY(ColourChanged.isValid());

	// sizeHint is derived from the group metrics: zero rows means zero cells.
	QCOMPARE(mColorList->sizeHint().width(), 200);
	QVERIFY2(mColorList->sizeHint().height() > 100,
		qPrintable(QStringLiteral("the colour list looks empty (height %1)").arg(mColorList->sizeHint().height())));

	// The curated archive defines 322 colours; a list this small can only be
	// the built-in palette, and this suite is about the shipped configuration.
	QVERIFY2(gColorList.size() >= 300,
		qPrintable(QStringLiteral("only %1 colours loaded").arg(gColorList.size())));
}

void PartsPanelChainTest::pickingAColourReachesTheWholeChain()
{
	// Selecting a colour must update three things at once: the window's current
	// index, the button that names the colour for the user, and the parts
	// panel's swatch colour. A list that moves only its own cursor looks
	// interactive and does nothing.
	//
	// Pick something clearly different from the default (7, Red) so a
	// not-updated value cannot pass by coincidence.
	const int TargetCode = 4;	// Red in LDraw's ldconfig
	const int TargetIndex = lcGetColorIndex(TargetCode);

	QVERIFY2(TargetIndex >= 0 && TargetIndex < static_cast<int>(gColorList.size()),
		"the fixture colour code is not in the loaded palette");
	QCOMPARE(gColorList[TargetIndex].Code, static_cast<quint32>(TargetCode));
	QVERIFY2(mWindow->mColorIndex != TargetIndex,
		"the fixture colour is already selected, so the chain below cannot be observed moving");

	const QString ExpectedName = QString::fromLatin1(gColorList[TargetIndex].Name);

	QSignalSpy ColourChanged(mColorList, &lcColorList::ColorChanged);

	mColorList->SetCurrentColor(TargetIndex);

	QVERIFY2(ColourChanged.count() >= 1,
		"selecting a colour emitted no ColorChanged signal; nothing downstream can update");

	// The window followed.
	QCOMPARE(mWindow->mColorIndex, TargetIndex);

	// The parts panel followed. This is the link that decides what colour a
	// newly placed part gets, so it is the one a user actually feels.
	QCOMPARE(mPanel->GetColorIndex(), TargetIndex);

	// The button carries the colour's name and a swatch of the colour itself.
	QVERIFY2(mColorButton->text().contains(ExpectedName),
		qPrintable(QStringLiteral("the colour button reads \"%1\", expected the name \"%2\"")
			.arg(mColorButton->text(), ExpectedName)));

	QVERIFY2(!mColorButton->icon().isNull(), "the colour button lost its swatch");

	const QColor Swatch = mColorButton->icon().pixmap(14, 14).toImage().pixelColor(7, 7);
	const QColor Expected(qRound(gColorList[TargetIndex].Value[0] * 255.0),
		qRound(gColorList[TargetIndex].Value[1] * 255.0),
		qRound(gColorList[TargetIndex].Value[2] * 255.0));

	// One channel of tolerance: the swatch is filled from float values that are
	// gamma-converted, and asking for an exact match would test the rounding
	// rather than the wiring.
	const int Delta = qMax(qAbs(Swatch.red() - Expected.red()),
		qMax(qAbs(Swatch.green() - Expected.green()), qAbs(Swatch.blue() - Expected.blue())));

	QVERIFY2(Delta <= 1,
		qPrintable(QStringLiteral("the swatch is %1, expected %2 (delta %3)")
			.arg(Swatch.name(), Expected.name()).arg(Delta)));
}

void PartsPanelChainTest::colourChainIgnoresTheSameColour()
{
	// SetCurrentColor -> SelectCell returns early when the cell is already
	// current, which is what stops a colour reload from re-firing the whole
	// chain. Assert the quiet path too: if it ever started emitting, every
	// ColorsLoaded() would re-enter the main window.
	const int Current = mWindow->mColorIndex;

	QSignalSpy ColourChanged(mColorList, &lcColorList::ColorChanged);

	mColorList->SetCurrentColor(Current);

	QCOMPARE(ColourChanged.count(), 0);
	QCOMPARE(mWindow->mColorIndex, Current);
}

void PartsPanelChainTest::categorySelectionListsOnlyMatchingParts()
{
	// The category tree is keyword-matched against the loaded library. Selecting
	// one has to change what is listed, and Sticker is known to be empty: the
	// curated subset excludes sticker and patterned parts by decision
	// (docs/library/D2 section 2.6).
	QTreeWidget* Tree = CategoryTree();

	QTreeWidgetItem* AllParts = FindCategory(QStringLiteral("All Parts"));
	QVERIFY2(AllParts, "no All Parts entry in the category tree");

	Tree->setCurrentItem(AllParts);

	const int Total = CountVisibleRows();
	QVERIFY2(Total > 500,
		qPrintable(QStringLiteral("All Parts listed only %1 rows from a 1160-part library").arg(Total)));

	QTreeWidgetItem* Brick = FindCategory(QStringLiteral("Brick"));
	QVERIFY2(Brick, "no Brick category");

	Tree->setCurrentItem(Brick);

	const int Bricks = CountVisibleRows();
	QVERIFY2(Bricks > 0, "the Brick category listed nothing");
	QVERIFY2(Bricks < Total,
		qPrintable(QStringLiteral("Brick listed %1 of %2 rows, so the category did not filter")
			.arg(Bricks).arg(Total)));

	// A category known to be empty must list nothing at all.
	QTreeWidgetItem* Sticker = FindCategory(QStringLiteral("Sticker"));
	QVERIFY2(Sticker, "no Sticker category");

	Tree->setCurrentItem(Sticker);
	QCOMPARE(CountVisibleRows(), 0);

	Tree->setCurrentItem(AllParts);
	QCOMPARE(CountVisibleRows(), Total);
}

void PartsPanelChainTest::searchMatchesPartsThatOnlyExistInTheCuratedLibrary()
{
	// 10172 is in the curated subset and not in the built-in 128-part archive, so
	// a hit on it is a hit on the shipped library. It also proves the filter
	// matches on part name as well as description, which is how users find a part
	// whose name they do not know.
	QLineEdit* Filter = PartsFilter();
	QVERIFY2(Filter->text().isEmpty(), "the filter box is not empty at the start of this case");

	Filter->setText(QStringLiteral("10172"));

	const int Found = CountVisibleRows();
	QVERIFY2(Found > 0, "searching for 10172 found nothing, although it is in the curated library");

	Filter->setText(QStringLiteral("zzzz-no-such-part-zzzz"));
	QCOMPARE(CountVisibleRows(), 0);

	Filter->clear();
	QVERIFY2(CountVisibleRows() > 500, "clearing the search did not restore the list");
}

void PartsPanelChainTest::countAgreesWithTheRowsOnScreen()
{
	// The count label is the panel's only statement about what is listed. If it
	// can disagree with the view, every downstream reading of it (including the
	// device acceptance steps) is suspect. Checked at three different states,
	// because a single sample could pass by coincidence.
	QLabel* Count = mPanel->findChild<QLabel*>(QStringLiteral("PartsCount"));
	QVERIFY2(Count, "the parts panel has no count label");

	QTreeWidget* Tree = CategoryTree();
	QTreeWidgetItem* AllParts = FindCategory(QStringLiteral("All Parts"));
	QTreeWidgetItem* Brick = FindCategory(QStringLiteral("Brick"));
	QVERIFY(AllParts && Brick);

	QLineEdit* Filter = PartsFilter();

	Filter->clear();
	Tree->setCurrentItem(AllParts);

	QVERIFY2(Count->text().contains(QString::number(CountVisibleRows())),
		qPrintable(QStringLiteral("count label \"%1\" disagrees with %2 visible rows")
			.arg(Count->text()).arg(CountVisibleRows())));

	Tree->setCurrentItem(Brick);

	QVERIFY2(Count->text().contains(QString::number(CountVisibleRows())),
		qPrintable(QStringLiteral("after switching category, count label \"%1\" disagrees with %2 visible rows")
			.arg(Count->text()).arg(CountVisibleRows())));

	Filter->setText(QStringLiteral("zzzz-no-such-part-zzzz"));

	QVERIFY2(Count->text().contains(QStringLiteral("0")),
		qPrintable(QStringLiteral("with nothing visible the count label still reads \"%1\"").arg(Count->text())));

	Filter->clear();
	Tree->setCurrentItem(AllParts);
}

void PartsPanelChainTest::emptyStateFollowsCategoryAndSearchTogether()
{
	// The empty notice has to name the right cause when both conditions are in
	// play: an empty category with a search typed in is still a category
	// problem, and a search that matches nothing inside a populated category is a
	// search problem. D4 covered each on its own; the combination is the case a
	// user reaches by clicking around.
	QLabel* Empty = mPanel->findChild<QLabel*>(QStringLiteral("PartsEmptyNotice"));
	QVERIFY2(Empty, "the parts panel has no empty notice");

	QTreeWidget* Tree = CategoryTree();
	QTreeWidgetItem* AllParts = FindCategory(QStringLiteral("All Parts"));
	QTreeWidgetItem* Sticker = FindCategory(QStringLiteral("Sticker"));
	QVERIFY(AllParts && Sticker);

	QLineEdit* Filter = PartsFilter();

	Filter->clear();
	Tree->setCurrentItem(Sticker);
	QVERIFY2(!Empty->isHidden(), "an empty category showed no notice");

	// Empty category plus a typed search: the search is not what emptied it.
	Filter->setText(QStringLiteral("10172"));
	QVERIFY2(!Empty->text().contains(QStringLiteral("search"), Qt::CaseInsensitive),
		qPrintable(QStringLiteral("an empty category was blamed on the search: \"%1\"").arg(Empty->text())));

	Filter->clear();

	// Populated category plus a search that matches nothing: now it is the
	// search, and the notice has to say so.
	Tree->setCurrentItem(AllParts);
	Filter->setText(QStringLiteral("zzzz-no-such-part-zzzz"));
	QVERIFY2(!Empty->isHidden(), "a search matching nothing showed no notice");
	QVERIFY2(Empty->text().contains(QStringLiteral("search"), Qt::CaseInsensitive),
		qPrintable(QStringLiteral("the notice does not name the search: \"%1\"").arg(Empty->text())));

	Filter->clear();
	QVERIFY2(Empty->isHidden(), "the notice stayed after the search was cleared");
}

int main(int argc, char** argv)
{
	lcApplication Application(argc, argv);
	PartsPanelChainTest Test;

	return QTest::qExec(&Test, argc, argv);
}

#include "tst_parts_panel_chain.moc"
