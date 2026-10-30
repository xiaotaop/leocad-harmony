// Verifies the parts panel says something useful when it has nothing to show.
//
// D4 asks for "empty library / damaged library prompts". Before this, the panel
// had neither: a rejected archive produced one line on stderr (invisible on a
// device) and the user saw a panel with far fewer parts than the package
// promises, with no explanation anywhere. A filter that matched nothing looked
// identical to a broken library.
//
// Three states are distinct and must stay distinct:
//
//   library refused  -> banner above the list, and NOT "no matching parts"
//   filter matched nothing -> empty notice naming the search
//   empty category   -> empty notice naming the category
//
// The tests build the real lcPartSelectionWidget over the real curated archive.
// Reading the code only shows intent; what matters is the widget a user gets.

#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QtTest>

#include "lc_application.h"
#include "lc_library.h"
#include "lc_mainwindow.h"
#include "lc_partselectionwidget.h"

class PartsPanelTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();

	void countShowsVisibleParts();
	void healthyLibraryShowsNoBanner();
	void refusedLibraryShowsBannerAndTooltip();
	void refusedLibraryDoesNotBlameTheFilter();
	void filterWithNoMatchShowsSearchNotice();
	void emptyCategoryShowsCategoryNotice();
	void emptyNoticeCoversViewportNotLayout();
	void bannerFollowsTheTheme();

private:
	QLabel* FindLabel(lcPartSelectionWidget* Panel, const char* Name) const;

	lcMainWindow* mWindow = nullptr;
	lcPartSelectionWidget* mPanel = nullptr;
};

void PartsPanelTest::initTestCase()
{
	QVERIFY(gApplication);

	if (!gApplication->mLibrary)
	{
		const QString Archive = QFINDTESTDATA("../../../assets/ldraw-lite/library-lite.zip");
		QVERIFY2(!Archive.isEmpty(), "bundled library not found");

		QList<QPair<QString, bool>> Paths;
		Paths += qMakePair(Archive, true);
		QVERIFY2(gApplication->LoadPartsLibrary(Paths, true), "could not load the parts library");
	}

	// A real window: the panel is only meaningful inside one, and CreateWidgets()
	// is where it is built and populated.
	mWindow = new lcMainWindow();
	mWindow->CreateWidgets();

	mPanel = mWindow->findChild<lcPartSelectionWidget*>();
	QVERIFY2(mPanel, "no parts panel in the main window");
}

QLabel* PartsPanelTest::FindLabel(lcPartSelectionWidget* Panel, const char* Name) const
{
	QLabel* Label = Panel->findChild<QLabel*>(QLatin1String(Name));

	return Label;
}

void PartsPanelTest::countShowsVisibleParts()
{
	QLabel* Count = FindLabel(mPanel, "PartsCount");
	QVERIFY2(Count, "the parts panel has no count label");

	// The curated archive has 1160 parts. The count is of what is listed, and on
	// a fresh window nothing is filtered out.
	QVERIFY2(!Count->text().isEmpty(), "the count label is empty");
	QVERIFY2(Count->text().contains(QLatin1String("1160")),
		qPrintable(QStringLiteral("count label does not report the library size: %1").arg(Count->text())));
}

void PartsPanelTest::healthyLibraryShowsNoBanner()
{
	// A banner on every launch of a healthy install would be noise, so the
	// default has to be hidden. This is the state the shipped package is in.
	mPanel->SetLibraryProblem(QString());

	QLabel* Banner = FindLabel(mPanel, "PartsLibraryProblem");
	QVERIFY2(Banner, "the parts panel has no library-problem label");
	QVERIFY2(Banner->isHidden(), "the library banner is visible with a healthy library");
}

void PartsPanelTest::refusedLibraryShowsBannerAndTooltip()
{
	// The validator's wording is what the entry point reports on stderr. The
	// banner must appear, and the exact reason must stay reachable for a bug
	// report rather than being thrown away.
	const QString Problem = QStringLiteral("missing ldraw/ldconfig.ldr");
	mPanel->SetLibraryProblem(Problem);

	QLabel* Banner = FindLabel(mPanel, "PartsLibraryProblem");
	QVERIFY2(Banner, "the parts panel has no library-problem label");
	QVERIFY2(!Banner->isHidden(), "a refused library produced no visible banner");

	// The banner is for the user, not a log: it must not leak the internal
	// wording into the label text, but the tooltip keeps it.
	QVERIFY2(!Banner->text().contains(QLatin1String("ldconfig")),
		qPrintable(QStringLiteral("the banner shows the internal reason: %1").arg(Banner->text())));
	QCOMPARE(Banner->toolTip(), Problem);

	mPanel->SetLibraryProblem(QString());
}

void PartsPanelTest::refusedLibraryDoesNotBlameTheFilter()
{
	// The failure that motivated this: a library that will not load leaves the
	// list empty, and an empty list used to be indistinguishable from a search
	// that matched nothing. Telling the user to try another keyword when the
	// package is broken sends them looking in the wrong place.
	//
	// Both conditions have to hold at once for this to test anything. With the
	// curated library loaded the list is not empty, so a notice that ignores the
	// library state would still be hidden and the check would pass on a broken
	// build. The filter makes the list genuinely empty.
	QLineEdit* Filter = mPanel->findChild<QLineEdit*>();
	QVERIFY(Filter);

	Filter->setText(QStringLiteral("zzzz-no-such-part-zzzz"));
	mPanel->SetLibraryProblem(QStringLiteral("archive not found"));

	QLabel* Empty = FindLabel(mPanel, "PartsEmptyNotice");
	QVERIFY2(Empty, "the parts panel has no empty notice");
	QVERIFY2(Empty->isHidden(),
		qPrintable(QStringLiteral("the empty notice also fired with a broken library: %1").arg(Empty->text())));

	// The banner is the surface that should carry the explanation.
	QLabel* Banner = FindLabel(mPanel, "PartsLibraryProblem");
	QVERIFY2(!Banner->isHidden(), "a broken library showed no banner while the list was empty");

	Filter->clear();
	mPanel->SetLibraryProblem(QString());
}

void PartsPanelTest::filterWithNoMatchShowsSearchNotice()
{
	QLineEdit* Filter = mPanel->findChild<QLineEdit*>();
	QVERIFY2(Filter, "the parts panel has no filter box");

	// A string no LDraw part description contains. Using a plausible word would
	// make the test depend on the contents of the shipped subset.
	const QString NoMatch = QStringLiteral("zzzz-no-such-part-zzzz");
	Filter->setText(NoMatch);

	QLabel* Empty = FindLabel(mPanel, "PartsEmptyNotice");
	QVERIFY2(Empty, "the parts panel has no empty notice");
	QVERIFY2(!Empty->isHidden(), "a search that matched nothing produced no notice");

	// It has to name the search, not the library: the user typed it and can
	// clear it.
	QVERIFY2(Empty->text().contains(QLatin1String("search"), Qt::CaseInsensitive),
		qPrintable(QStringLiteral("the notice does not mention the search: %1").arg(Empty->text())));

	// And the count has to follow, or the panel contradicts itself.
	QLabel* Count = FindLabel(mPanel, "PartsCount");
	QVERIFY2(Count->text().contains(QLatin1String("0")),
		qPrintable(QStringLiteral("count label still reports parts while the list is empty: %1").arg(Count->text())));

	Filter->clear();
	QVERIFY2(Empty->isHidden(), "the notice stayed after the search was cleared");
}

void PartsPanelTest::emptyCategoryShowsCategoryNotice()
{
	// Sticker is empty by design: the curated subset excludes patterned and
	// sticker parts (D2 section 2.6). Picking it must explain the blank list
	// without pretending a search went wrong.
	QTreeWidget* Tree = mPanel->findChild<QTreeWidget*>();
	QVERIFY2(Tree, "the parts panel has no category tree");

	QTreeWidgetItem* Sticker = nullptr;

	for (int Row = 0; Row < Tree->topLevelItemCount(); Row++)
	{
		QTreeWidgetItem* Item = Tree->topLevelItem(Row);

		if (Item->text(0) == QLatin1String("Sticker"))
		{
			Sticker = Item;
			break;
		}
	}

	QVERIFY2(Sticker, "the Sticker category is missing from the tree");

	Tree->setCurrentItem(Sticker);

	QLabel* Empty = FindLabel(mPanel, "PartsEmptyNotice");
	QVERIFY2(!Empty->isHidden(), "an empty category produced no notice");
	QVERIFY2(!Empty->text().contains(QLatin1String("search"), Qt::CaseInsensitive),
		qPrintable(QStringLiteral("an empty category blames the search: %1").arg(Empty->text())));
}

void PartsPanelTest::emptyNoticeCoversViewportNotLayout()
{
	// D2 section 6.2: the notice is an overlay, not a layout item. A layout item
	// would shrink the list every time it appeared, which is the trap that was
	// already recorded once.
	QLineEdit* Filter = mPanel->findChild<QLineEdit*>();
	Filter->setText(QStringLiteral("zzzz-no-such-part-zzzz"));

	QLabel* Empty = FindLabel(mPanel, "PartsEmptyNotice");
	QVERIFY(!Empty->isHidden());

	// It has to be parented to the viewport, and cover it, or it would not read
	// as a state of the list.
	QListView* View = mPanel->findChild<QListView*>();
	QVERIFY2(View, "the parts panel has no list view");
	QCOMPARE(Empty->parentWidget(), View->viewport());

	// Force a layout pass so the geometry is real rather than the initial one.
	mWindow->resize(1000, 700);
	mWindow->show();
	QVERIFY(QTest::qWaitForWindowExposed(mWindow));

	QVERIFY2(Empty->geometry().size() == View->viewport()->size(),
		qPrintable(QStringLiteral("overlay is %1x%2, viewport is %3x%4")
			.arg(Empty->width()).arg(Empty->height())
			.arg(View->viewport()->width()).arg(View->viewport()->height())));

	Filter->clear();
}

void PartsPanelTest::bannerFollowsTheTheme()
{
	// LeoCAD has a dark theme (lcApplication::UpdateStyle() sets near-black
	// window backgrounds). A banner painted from a fixed light colour would be
	// unreadable there, and nothing on a light desktop build would show it.
	//
	// The banner has to take its colours from the palette, and specifically from
	// a pair that the dark palette overrides.
	QLabel* Banner = FindLabel(mPanel, "PartsLibraryProblem");
	QVERIFY(Banner);

	QVERIFY2(Banner->autoFillBackground(), "the banner has no background of its own; it would blend into the panel");
	QVERIFY2(Banner->styleSheet().isEmpty(),
		qPrintable(QStringLiteral("the banner is styled from a stylesheet, which does not follow the theme: %1").arg(Banner->styleSheet())));

	QVERIFY2(Banner->palette().color(QPalette::Window) == QApplication::palette().color(QPalette::ToolTipBase),
		"the banner background is not the theme's notice colour");
	QVERIFY2(Banner->palette().color(QPalette::WindowText) == QApplication::palette().color(QPalette::ToolTipText),
		"the banner text colour is not the theme's notice text colour");
}

int main(int argc, char** argv)
{
	lcApplication Application(argc, argv);
	PartsPanelTest Test;

	return QTest::qExec(&Test, argc, argv);
}

#include "tst_parts_panel.moc"
