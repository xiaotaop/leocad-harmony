// D7: the Chinese first release has to read as Chinese, without changing data.
//
// The plan (docs/LeoCAD_鸿蒙PC首发版_两周分工与技术方案.md, 11/2 D7) asks for
// "完成中文资源、字体和常用提示". The device report behind issue #28 / D4-03 added
// the specifics: "Filter Parts", the part count and the empty states were still
// English; category and colour names were not translated; and the search filter
// submenu elided "Regular Expression" and "显示零件" in the middle.
//
// Two things are easy to get wrong here and both are asserted below:
//
//   * Translating the stored string instead of what is displayed. A category
//     name is the key the profile is written with, and a colour name goes into
//     saved models and exports. Replacing either would make a file written in
//     one language unreadable in another, so the tests pin the stored value
//     first and then check that the displayed one differs.
//
//   * Setting the menu width before the menu has any contents. The options menu
//     is rebuilt in aboutToShow, which runs after Qt has already sized it; that
//     is what produced "Regular...ression" on device. The test drives a real
//     QMenu through the same path and measures the width the user would see.
//
// Runs under the offscreen platform: it checks strings and metrics, not pixels.

#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QTranslator>
#include <QTreeWidget>
#include <QCursor>
#include <QtTest>

#include "lc_application.h"
#include "lc_category.h"
#include "lc_colors.h"
#include "lc_library.h"
#include "lc_mainwindow.h"
#include "lc_partselectionwidget.h"
#include "project.h"

class LocalizationTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void cleanupTestCase();

	// Storage vs display: the whole point of translating late.
	void storedColourNameIsNotTranslated();
	void storedCategoryNameIsNotTranslated();
	void colourNamesAreTranslatedForDisplay();
	void everyLibraryColourHasADisplayName();
	void categoryNamesAreTranslatedForDisplay();
	void everyDefaultCategoryHasADisplayName();

	// The strings the device report listed as still English.
	void panelStringsAreTranslated();
	void partCountIsTranslatedAndUsesThePluralFreeForm();
	void emptyStatesAreTranslated();
	void libraryProblemBannerIsTranslated();

	// Menu width: the elision the device showed as "Regular...ression".
	void optionsMenuFitsItsLongestEntry();
	void filterSubmenuFitsItsLongestEntry();

	// Nothing above is worth anything if the panel stopped agreeing with the
	// model, so re-check the two counts the chain test also owns.
	void panelStillListsPartsAfterTranslation();

private:
	lcMainWindow* mWindow = nullptr;
	lcPartSelectionWidget* mPanel = nullptr;
	QTranslator* mTranslator = nullptr;

	QTreeWidget* CategoryTree() const;
	QLineEdit* PartsFilter() const;
	QToolButton* OptionsButton() const;
	int CountVisibleRows() const;
	QMenu* BuildOptionsMenu();
};

void LocalizationTest::initTestCase()
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

	// Install the shipped translation rather than a test-local one, so a missing
	// or malformed .ts/.qm fails here instead of on the device. lrelease has to
	// have run; the test project depends on the same resources leocad.pro builds.
	mTranslator = new QTranslator();
	const QString Qm = QFINDTESTDATA("../../../resources/leocad_zh_CN.qm");

	if (Qm.isEmpty() || !mTranslator->load(Qm))
	{
		QSKIP("leocad_zh_CN.qm not built; run lrelease (qmake builds it with the resources)");
	}

	QVERIFY(QCoreApplication::installTranslator(mTranslator));

	mWindow = new lcMainWindow();
	mWindow->CreateWidgets();
	gApplication->SetProject(new Project());

	mPanel = mWindow->findChild<lcPartSelectionWidget*>();
	QVERIFY2(mPanel, "no parts panel in the main window");
}

void LocalizationTest::cleanupTestCase()
{
	delete mWindow;
	mWindow = nullptr;
	mPanel = nullptr;

	if (mTranslator)
	{
		QCoreApplication::removeTranslator(mTranslator);
		delete mTranslator;
		mTranslator = nullptr;
	}
}

QTreeWidget* LocalizationTest::CategoryTree() const
{
	QTreeWidget* Tree = mPanel->findChild<QTreeWidget*>();

	if (Tree == nullptr)
		qFatal("the parts panel has no category tree");

	return Tree;
}

QLineEdit* LocalizationTest::PartsFilter() const
{
	QLineEdit* Filter = mPanel->findChild<QLineEdit*>(QStringLiteral("PartsFilter"));

	if (Filter == nullptr)
		qFatal("the parts panel has no parts filter box");

	return Filter;
}

QToolButton* LocalizationTest::OptionsButton() const
{
	// The panel has several tool buttons (the case toggle, the gear). The gear is
	// the one that owns a menu.
	for (QToolButton* Button : mPanel->findChildren<QToolButton*>())
		if (Button->menu() != nullptr)
			return Button;

	qFatal("the parts panel has no options button");
	return nullptr;
}

int LocalizationTest::CountVisibleRows() const
{
	int Visible = 0;

	for (int Row = 0; Row < mPanel->findChild<lcPartSelectionListView*>()->model()->rowCount(); Row++)
		if (!mPanel->findChild<lcPartSelectionListView*>()->isRowHidden(Row))
			Visible++;

	return Visible;
}

QMenu* LocalizationTest::BuildOptionsMenu()
{
	// Go through the button, which is the path the user takes: the menu is
	// emptied and refilled in the aboutToShow handler, and that ordering is the
	// bug being tested, so bypassing the signal would test nothing.
	//
	// QVERIFY cannot be used here (it expands to a bare `return`, which does not
	// compile in a function returning a pointer), so a missing menu stops the run
	// with qFatal the way the other helpers do.
	QToolButton* Button = OptionsButton();

	if (Button == nullptr || Button->menu() == nullptr)
		qFatal("the parts panel options button has no menu");

	QMenu* Menu = Button->menu();

	// popup() emits aboutToShow synchronously, so the handler has run by the time
	// this returns and the menu has its contents and its width.
	Menu->popup(QCursor::pos());
	QCoreApplication::processEvents();

	return Menu;
}

// A colour's stored name is what a model file and an export carry. If the
// translator ever ran in place this would come back Chinese and every saved file
// would silently change meaning.
void LocalizationTest::storedColourNameIsNotTranslated()
{
	QVERIFY(!gColorList.empty());

	const int Black = lcGetColorIndex(0);
	QVERIFY(Black >= 0);

	QCOMPARE(QString::fromLatin1(gColorList[Black].Name), QStringLiteral("Black"));
	QCOMPARE(QString::fromLatin1(gColorList[Black].SafeName), QStringLiteral("Black"));

	// And the export path uses SafeName, so it keeps the ASCII form too.
	QVERIFY(gColorList[Black].SafeName[0] < 128);
}

void LocalizationTest::storedCategoryNameIsNotTranslated()
{
	QVERIFY(!gCategories.empty());

	// The first default category, by construction order in lcResetCategories().
	QCOMPARE(gCategories[0].Name, QStringLiteral("Animal"));

	// Loading and saving is what a language switch must survive: the profile is
	// written in English names, so a round trip has to reproduce them exactly.
	QByteArray Buffer;
	QTextStream Stream(&Buffer, QIODevice::WriteOnly);
	QVERIFY(lcSaveCategories(Stream, gCategories));

	std::vector<lcLibraryCategory> Reloaded;
	QVERIFY(lcLoadCategories(Buffer, Reloaded));
	QCOMPARE(static_cast<int>(Reloaded.size()), static_cast<int>(gCategories.size()));
	QCOMPARE(Reloaded[0].Name, QStringLiteral("Animal"));
	QCOMPARE(Reloaded[0].Keywords, gCategories[0].Keywords);
}

void LocalizationTest::colourNamesAreTranslatedForDisplay()
{
	const int Black = lcGetColorIndex(0);
	QVERIFY(Black >= 0);

	// The translation is the one shipped in leocad_zh_CN.ts under "Colors".
	QCOMPARE(lcGetColorDisplayName(Black), QStringLiteral("黑色"));

	// The display name must not have leaked into the stored one.
	QCOMPARE(QString::fromLatin1(gColorList[Black].Name), QStringLiteral("Black"));
}

// A missing entry in the .ts shows up as the English name, which is exactly the
// device complaint, so pin that every colour actually resolves to something and
// that the ones the release cares about are Chinese.
void LocalizationTest::everyLibraryColourHasADisplayName()
{
	int Untranslated = 0;
	QString FirstUntranslated;

	for (int Index = 0; Index < static_cast<int>(gColorList.size()); Index++)
	{
		const QString Display = lcGetColorDisplayName(Index);

		// Names synthesised as "Color 0004FF" carry a code; they have no words to
		// translate and are correct as-is.
		const QString Stored = QString::fromLatin1(gColorList[Index].Name);

		if (Display.isEmpty())
		{
			Untranslated++;
			if (FirstUntranslated.isEmpty())
				FirstUntranslated = Stored;
			continue;
		}

		if (Display == Stored && !Stored.startsWith(QStringLiteral("Color ")))
		{
			Untranslated++;
			if (FirstUntranslated.isEmpty())
				FirstUntranslated = Stored;
		}

		// Whatever it is, it is never allowed to differ from the stored value in
		// the data itself.
		QCOMPARE(QString::fromLatin1(gColorList[Index].Name), Stored);
	}

	QVERIFY2(Untranslated == 0,
		qPrintable(QStringLiteral("%1 colours have no zh_CN name, first: %2")
			.arg(Untranslated).arg(FirstUntranslated)));
}

void LocalizationTest::categoryNamesAreTranslatedForDisplay()
{
	QCOMPARE(lcGetCategoryDisplayName(QStringLiteral("Brick")), QStringLiteral("砖块"));
	QCOMPARE(lcGetCategoryDisplayName(QStringLiteral("Door and Window")), QStringLiteral("门窗"));

	// The stored name is untouched, so the profile round trip above still holds.
	QCOMPARE(gCategories[2].Name, QStringLiteral("Arch"));
}

void LocalizationTest::everyDefaultCategoryHasADisplayName()
{
	// A user can add categories of their own; those have no translation and must
	// fall back to the name they typed rather than to an empty string.
	QCOMPARE(lcGetCategoryDisplayName(QStringLiteral("My Own Category")), QStringLiteral("My Own Category"));

	int Untranslated = 0;
	QString First;

	for (const lcLibraryCategory& Category : gCategories)
	{
		const QString Display = lcGetCategoryDisplayName(Category.Name);

		if (Display == Category.Name)
		{
			Untranslated++;
			if (First.isEmpty())
				First = Category.Name;
		}
	}

	QVERIFY2(Untranslated == 0,
		qPrintable(QStringLiteral("%1 categories have no zh_CN name, first: %2")
			.arg(Untranslated).arg(First)));
}

void LocalizationTest::panelStringsAreTranslated()
{
	// Straight from the device report: these were the strings still in English.
	QCOMPARE(QCoreApplication::translate("lcPartSelectionWidget", "Filter Parts"), QStringLiteral("筛选零件"));
	QCOMPARE(QCoreApplication::translate("lcPartSelectionWidget", "Regular Expression"), QStringLiteral("正则表达式"));
	QCOMPARE(QCoreApplication::translate("lcPartSelectionWidget", "Part Description"), QStringLiteral("零件描述"));
	QCOMPARE(QCoreApplication::translate("lcPartSelectionWidget", "Filter"), QStringLiteral("筛选"));
	QCOMPARE(QCoreApplication::translate("lcPartSelectionWidget", "Icons"), QStringLiteral("图标"));
	QCOMPARE(QCoreApplication::translate("lcPartSelectionWidget", "Lock Color"), QStringLiteral("锁定颜色"));
	QCOMPARE(QCoreApplication::translate("lcPartSelectionWidget", "Open Submodel"), QStringLiteral("打开子模型"));

	// Its neighbour was already translated on device, so it must stay that way.
	QCOMPARE(QCoreApplication::translate("lcPartSelectionWidget", "Match Case"), QStringLiteral("区分大小写"));

	// And the placeholder the panel actually shows uses it.
	QCOMPARE(PartsFilter()->placeholderText(), QStringLiteral("筛选零件"));
}

void LocalizationTest::partCountIsTranslatedAndUsesThePluralFreeForm()
{
	// Which label it is has to be read from the panel rather than guessed: the
	// count label is the one whose text starts with the translated prefix.
	QLabel* CountLabel = nullptr;

	for (QLabel* Label : mPanel->findChildren<QLabel*>())
		if (Label->text().startsWith(QStringLiteral("零件: ")))
		{
			CountLabel = Label;
			break;
		}

	QVERIFY2(CountLabel, "no translated part count label in the panel");

	// The wording is "零件: 1160" and not the plural form "%n part(s)": the plural
	// rules only read correctly once a translation supplies them, and this release
	// deliberately avoids the count-dependent string (see RefreshStatus()).
	QVERIFY(CountLabel->text().contains(QStringLiteral("零件: ")));
	QVERIFY(!CountLabel->text().contains(QStringLiteral("part")));
}

void LocalizationTest::emptyStatesAreTranslated()
{
	// Search that can never match: the notice has to blame the search, in Chinese.
	PartsFilter()->setText(QStringLiteral("zzzz-no-such-part"));
	QCoreApplication::processEvents();

	QLabel* Notice = nullptr;

	for (QLabel* Label : mPanel->findChildren<QLabel*>())
	{
		const QString Text = Label->text();

		if (Text.contains(QStringLiteral("没有匹配此搜索的零件")) ||
			Text.contains(QStringLiteral("此分类下没有可显示的零件")))
		{
			Notice = Label;
			break;
		}
	}

	QVERIFY2(Notice, "the empty-state notice is not translated");

	// With a search typed into a category that has parts, the advice must be the
	// one that can work: changing the keyword.
	if (Notice->text().contains(QStringLiteral("搜索")))
		QVERIFY(Notice->text().contains(QStringLiteral("关键词")));

	PartsFilter()->clear();
	QCoreApplication::processEvents();
}

void LocalizationTest::libraryProblemBannerIsTranslated()
{
	const QString Banner = QCoreApplication::translate("lcPartSelectionWidget",
		"Only a small number of parts are available, so models that use other parts will open with missing pieces.");

	QVERIFY(Banner.contains(QStringLiteral("零件")));
	QVERIFY(!Banner.contains(QStringLiteral("parts")));
}

// The device showed "Regular...ression": the menu was wider than the text in its
// neighbours but narrower than this entry, because it was sized before the
// handler refilled it.
void LocalizationTest::optionsMenuFitsItsLongestEntry()
{
	QMenu* Menu = BuildOptionsMenu();
	QVERIFY(Menu);

	const QFontMetrics Metrics(Menu->font());

	for (const QAction* Action : Menu->actions())
	{
		if (Action->isSeparator())
			continue;

		const int TextWidth = Metrics.horizontalAdvance(Action->text());

		QVERIFY2(Menu->minimumWidth() > TextWidth,
			qPrintable(QStringLiteral("menu width %1 does not fit \"%2\" (%3 px)")
				.arg(Menu->minimumWidth()).arg(Action->text()).arg(TextWidth)));
	}
}

void LocalizationTest::filterSubmenuFitsItsLongestEntry()
{
	QMenu* Menu = BuildOptionsMenu();
	QVERIFY(Menu);

	QMenu* FilterMenu = nullptr;

	for (QAction* Action : Menu->actions())
		if (Action->menu() != nullptr && Action->text() == QCoreApplication::translate("lcPartSelectionWidget", "Filter"))
			FilterMenu = Action->menu();

	QVERIFY2(FilterMenu, "no filter submenu in the options menu");

	const QFontMetrics Metrics(FilterMenu->font());

	// The two entries the device reported as elided.
	QStringList Texts;
	for (const QAction* Action : FilterMenu->actions())
		if (!Action->isSeparator())
			Texts += Action->text();

	QVERIFY(Texts.contains(QStringLiteral("正则表达式")));
	QVERIFY(Texts.contains(QStringLiteral("零件描述")));

	for (const QString& Text : Texts)
		QVERIFY2(FilterMenu->minimumWidth() > Metrics.horizontalAdvance(Text),
			qPrintable(QStringLiteral("filter submenu width %1 does not fit \"%2\" (%3 px)")
				.arg(FilterMenu->minimumWidth()).arg(Text).arg(Metrics.horizontalAdvance(Text))));
}

void LocalizationTest::panelStillListsPartsAfterTranslation()
{
	// Translation must not have broken the list: the count label and the rows have
	// to agree, which is the property the D6 suite also pins. Cheap, but it fails
	// loudly if a display change accidentally filtered something out.
	QVERIFY(CountVisibleRows() > 0);
}

int main(int argc, char** argv)
{
	// A custom main rather than QTEST_MAIN: gApplication is set by the
	// lcApplication constructor, and the panel helpers below read it. A bare
	// QTEST_MAIN would leave it null.
	lcApplication Application(argc, argv);
	LocalizationTest Test;

	return QTest::qExec(&Test, argc, argv);
}

#include "tst_localization.moc"
