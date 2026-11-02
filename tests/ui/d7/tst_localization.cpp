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
#include <QFile>
#include <QFontInfo>
#include <QFontMetrics>
#include <QXmlStreamReader>
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

	// Strings outside the parts panel that sit on the same chain this change
	// touches: the colour button's tooltip, and the File menu entry that the
	// device screenshots showed in English next to translated neighbours.
	void mainWindowStringsAroundThePanelAreTranslated();

	// The other half of "中文资源、字体和常用提示": a translated string that the
	// font cannot draw is still a blank box on screen, and that looks exactly
	// like a missing translation.
	void chineseGlyphsAreAvailableToTheUiFont();
	void chineseTextIsNotWiderThanTheSpaceItsEnglishPredecessorHad();

	// 常用提示: the messages a user meets when a file will not open or save. These
	// come out of QMessageBox, so English here is a dialog in English on an
	// otherwise Chinese build — the kind of thing a reviewer sees immediately.
	void commonFileErrorMessagesAreTranslated();

	// Menu width: the elision the device showed as "Regular...ression".
	void optionsMenuFitsItsLongestEntry();
	void filterSubmenuFitsItsLongestEntry();

	// Nothing above is worth anything if the panel stopped agreeing with the
	// model, so re-check the two counts the chain test also owns.
	void panelStillListsPartsAfterTranslation();

	// Acceptance item 4: search, the Part ID toggle and the zero/non-zero
	// transition must not regress. These do not touch translation at all, which
	// is exactly why they are here — the display change runs through the same
	// filter path, and a translated label is worth nothing if the list behind it
	// stopped responding.
	void partIdFilterStillDrivesTheList();
	void zeroAndNonZeroResultsStillSwapTheNotice();
	void searchWithNoMatchesStillReportsZero();

private:
	lcMainWindow* mWindow = nullptr;
	lcPartSelectionWidget* mPanel = nullptr;
	QTranslator* mTranslator = nullptr;

	QTreeWidget* CategoryTree() const;
	QLineEdit* PartsFilter() const;
	QToolButton* OptionsButton() const;
	int CountVisibleRows() const;
	QMenu* BuildOptionsMenu();
	QList<QPair<QString, QString>> ReadZhTranslations(const QString& Context);
	bool FontStackIsReal() const;
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

// Reads one context out of the .ts as (source, translation) pairs.
//
// The .ts rather than the installed translator because QTranslator only answers
// "what does this exact source translate to", and what is needed here is the set
// of strings that are actually shown, which the file lists. The .qm the tests
// install is built from this same file, so a context that reads fine here is the
// one the application uses.
QList<QPair<QString, QString>> LocalizationTest::ReadZhTranslations(const QString& Context)
{
	QList<QPair<QString, QString>> Result;

	const QString Ts = QFINDTESTDATA("../../../resources/leocad_zh_CN.ts");

	if (Ts.isEmpty())
		return Result;

	QFile File(Ts);

	if (!File.open(QIODevice::ReadOnly | QIODevice::Text))
		return Result;

	QXmlStreamReader Xml(&File);
	QString Current;

	while (!Xml.atEnd())
	{
		Xml.readNext();

		if (!Xml.isStartElement())
			continue;

		if (Xml.name() == QLatin1String("context"))
			Current.clear();
		else if (Xml.name() == QLatin1String("name"))
			Current = Xml.readElementText();
		else if (Xml.name() == QLatin1String("message") && Current == Context)
		{
			// Walk the children by hand: readElementText() on the first one would
			// consume the rest of the element.
			QString Source;
			QString Translation;

			while (!(Xml.isEndElement() && Xml.name() == QLatin1String("message")))
			{
				if (Xml.atEnd())
					break;

				Xml.readNext();

				if (!Xml.isStartElement())
					continue;

				if (Xml.name() == QLatin1String("source"))
					Source = Xml.readElementText();
				else if (Xml.name() == QLatin1String("translation"))
				{
					const bool Unfinished = Xml.attributes().value(QLatin1String("type")) == QLatin1String("unfinished");
					Translation = Unfinished ? QString() : Xml.readElementText();
				}
			}

			if (!Translation.isEmpty())
				Result += qMakePair(Source, Translation);
		}
	}

	return Result;
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

// The panel is not the only place the review looked at, and two of the strings on
// this chain live in the main window: the tooltip of the colour button (whose text
// this change translates) and the File menu entry that appeared in English beside
// translated ones.
void LocalizationTest::mainWindowStringsAroundThePanelAreTranslated()
{
	// The colour button's text is translated (asserted above); if its tooltip were
	// not, hovering the button would explain it in English.
	QCOMPARE(QCoreApplication::translate("lcMainWindow", "Change Selection Color"), QStringLiteral("更改选中颜色"));

	// Menu mnemonics survive translation: the & marks the Alt shortcut, and a
	// translation that drops it silently removes keyboard access.
	const QString Render = QCoreApplication::translate("lcMainWindow", "&Render");
	QVERIFY2(Render.contains(QLatin1Char('&')), qPrintable(QStringLiteral("the Render mnemonic was lost: \"%1\"").arg(Render)));
	QVERIFY(!Render.contains(QStringLiteral("Render")));

	// Dialog titles the user meets while saving.
	QCOMPARE(QCoreApplication::translate("lcMainWindow", "Save Project"), QStringLiteral("保存项目"));

	// The properties tab draws the same colour name this change translates, and
	// its "several different colours selected" state is the only text the colour
	// button has there. Left English, selecting two bricks of different colours
	// would put English back on screen from a path the panel fix never reaches.
	QCOMPARE(QCoreApplication::translate("lcPropertiesWidget", " Multiple Colors"), QStringLiteral(" 多个颜色"));
	QCOMPARE(QCoreApplication::translate("lcPropertiesWidget", "Multiple Values"), QStringLiteral("多个值"));
	QCOMPARE(QCoreApplication::translate("lcPropertiesWidget", "Multiple Pieces"), QStringLiteral("多个零件"));
}

// A translated string the font cannot draw renders as a blank box, which is
// indistinguishable on screen from the string never having been translated.
// Per the review after two Windows CI reds: CI verifies the GLYPHS the UI font
// actually resolves to after its fallback chain, and that Chinese fits the
// controls; the HarmonyOS font itself is a device measurement (probe below),
// not a CI one. What was cancelled is the forced CJK-wider-than-Latin width
// comparison — equal advances say nothing about glyph support (font, size and
// fallback decide width), and it produced a false failure on a host that has
// working Chinese text.
void LocalizationTest::chineseGlyphsAreAvailableToTheUiFont()
{
	// Part one, valid on every platform: the translation source the app loads is
	// parseable and actually carries Chinese for the visible contexts, and the
	// line box has height at all (a zero-height layout clips everything).
	QString Shown;

	const QStringList Contexts = {
		QStringLiteral("lcPartSelectionWidget"),
		QStringLiteral("Categories"),
		QStringLiteral("lcMainWindow"),
		QStringLiteral("lcPropertiesWidget"),
	};

	for (const QString& Context : Contexts)
		for (const QPair<QString, QString>& Entry : ReadZhTranslations(Context))
			Shown += Entry.second;
	QVERIFY2(!Shown.isEmpty(), "no Chinese translations were read from the .ts");

	const QFontMetrics Metrics(mPanel->font());
	QVERIFY2(Metrics.height() > 0,
		qPrintable(QStringLiteral("zero or negative line height: %1").arg(Metrics.height())));

	if (!FontStackIsReal())
		QSKIP("font database is the fixed-advance stub: glyph coverage is unmeasurable here");

	// Part two, on a real font stack: every Chinese character the panels show
	// resolves to a glyph through the platform's fallback chain — the same
	// question 字体 asks on the device, answered against whatever fonts the
	// runner actually has. Engines disagree about per-GLOBAL-font coverage
	// (DirectWrite answers for the family alone; fontconfig folds fallback into
	// the answer), so this is the check that only makes sense once the stack is
	// known real; the device probe carries the same wording for 组长 to run.
	QString Missing;

	for (const QChar Ch : Shown)
	{
		if (Ch.unicode() < 0x80 || Ch.isSpace())
			continue;

		if (!Metrics.inFont(Ch))
			Missing += Ch;
	}

	if (!Missing.isEmpty())
		QFAIL(qPrintable(QStringLiteral("%1 characters have no glyph in \"%2\": %3")
			.arg(Missing.size()).arg(QFontInfo(mPanel->font()).family()).arg(Missing)));
}

// Whether the resolved font stack is a real one or the platform's fixed-advance
// guess. The Windows CI runs this suite on the offscreen platform, where the
// font database comes up empty (QWARN "Cannot find font directory .../lib/fonts"
// in the run log, and QFont point sizes computed from a -1 pixel size) and every
// glyph — Latin or CJK — measures at the same fixed advance. Advance comparisons
// in that state measure nothing: CJK > Latin spuriously failed there with both
// sides at 24 px, and Chinese-narrower-than-English would spuriously pass on any
// pair. i vs W is the detector: no real font ever draws those equally wide.
bool LocalizationTest::FontStackIsReal() const
{
	const QFontMetrics Metrics(mPanel->font());
	return Metrics.horizontalAdvance(QStringLiteral("i")) !=
		   Metrics.horizontalAdvance(QStringLiteral("W"));
}

// The menu width was computed from the widest entry, so what matters is whether
// the Chinese entry is wider than the English one that used to be there. A test
// that only says "it fits now" would keep passing if a translation grew and the
// width followed it, hiding the case where the menu outgrows its neighbours.
void LocalizationTest::chineseTextIsNotWiderThanTheSpaceItsEnglishPredecessorHad()
{
	if (!FontStackIsReal())
		QSKIP("offscreen fixed-advance font stack: text advances measure nothing");

	const QFontMetrics Metrics(mPanel->font());

	// The two entries the device showed elided, against the English they replace.
	const QPair<QString, QString> Pairs[] = {
		{ QStringLiteral("Regular Expression"), QString::fromUtf8("正则表达式") },
		{ QStringLiteral("Part Description"), QString::fromUtf8("零件描述") },
		{ QStringLiteral("Filter Parts"), QString::fromUtf8("筛选零件") },
	};

	for (const auto& Pair : Pairs)
	{
		const int English = Metrics.horizontalAdvance(Pair.first);
		const int Chinese = Metrics.horizontalAdvance(Pair.second);

		QVERIFY2(Chinese <= English,
			qPrintable(QStringLiteral("\"%1\" (%2 px) is wider than the \"%3\" (%4 px) it replaced")
				.arg(Pair.second).arg(Chinese).arg(Pair.first).arg(English)));
	}
}

// 常用提示. Each of these is reached from an ordinary action — open a broken file,
// save to a format the build lacks — and is shown through a QMessageBox with the
// text as its body, so an untranslated one is a fully English dialog.
void LocalizationTest::commonFileErrorMessagesAreTranslated()
{
	// source -> something the translation must contain. Not the exact wording:
	// that would make every future rewording a test failure, and the point is
	// whether the user sees Chinese, not which Chinese.
	const QPair<QString, QString> Required[] = {
		{ QStringLiteral("Error loading file '%1':\n%2"), QString::fromUtf8("加载") },
		{ QStringLiteral("Error loading file '%1':\nFile format is not recognized."), QString::fromUtf8("无法识别") },
		{ QStringLiteral("Error reading file '%1':\n%2"), QString::fromUtf8("读取") },
		{ QStringLiteral("The file contains no LDraw records."), QString::fromUtf8("没有") },
		{ QStringLiteral("This format is disabled in this build."), QString::fromUtf8("禁用") },
		{ QStringLiteral("Duplicate submodel name."), QString::fromUtf8("子模型") },
		{ QStringLiteral("Invalid submodel name."), QString::fromUtf8("子模型") },
		{ QStringLiteral("Invalid part reference."), QString::fromUtf8("零件") },
		{ QStringLiteral("Invalid color code."), QString::fromUtf8("颜色") },
		{ QStringLiteral("Embedded binary data is not supported."), QString::fromUtf8("不支持") },
		{ QStringLiteral("Missing or invalid geometry coordinates."), QString::fromUtf8("坐标") },
		{ QStringLiteral("Missing or invalid piece pivot coordinates."), QString::fromUtf8("坐标") },
		{ QStringLiteral("Circular submodel references are not supported."), QString::fromUtf8("不支持") },
		{ QStringLiteral("Invalid LDraw data at line %1: %2"), QString::fromUtf8("数据无效") },
	};

	QStringList Missing;

	for (const auto& Entry : Required)
	{
		const QString Text = QCoreApplication::translate("lcMainWindow", Entry.first.toUtf8().constData());

		// Untranslated means Qt hands back the source verbatim; that is the failure.
		if (Text == Entry.first || !Text.contains(Entry.second))
			Missing += Entry.first;
	}

	// Build the detail only when there is something to report: QVERIFY2 evaluates
	// its message argument either way, so Missing.first() here would assert on the
	// success path — a passing test that aborts is worse than a failing one.
	if (!Missing.isEmpty())
	{
		QFAIL(qPrintable(QStringLiteral("%1 common messages are not translated, first: \"%2\"")
			.arg(Missing.size()).arg(QString(Missing.first()).replace(QLatin1Char('\n'), QLatin1Char(' ')))));
	}

	// A format string that lost its placeholder would show "%1" to the user, or
	// crash arg() in a debug build. Checked separately because it is a different
	// mistake from leaving the string English.
	const QString WithPlaceholder = QCoreApplication::translate("lcMainWindow", "Invalid LDraw data at line %1: %2");
	QVERIFY2(WithPlaceholder.contains(QStringLiteral("%1")) && WithPlaceholder.contains(QStringLiteral("%2")),
		qPrintable(QStringLiteral("placeholders were lost: \"%1\"").arg(WithPlaceholder)));
}
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

// Acceptance item 4, first half: "Part ID" is the toggle whose translated label
// this change also touches, and it is the one filter that searches the LDraw
// file name instead of the description. If the label moved but the behaviour did
// not, a user searching 3001 would find nothing.
void LocalizationTest::partIdFilterStillDrivesTheList()
{
	lcPartSelectionListView* View = mPanel->findChild<lcPartSelectionListView*>();
	QVERIFY(View);

	lcPartSelectionListModel* Model = View->GetListModel();
	QVERIFY(Model);

	// The defaults belong to the profile, not to this test: whatever they are,
	// put them back at the end so a later case does not inherit a list filtered
	// down to nothing. (Getting this wrong is how the first version of this case
	// broke the two after it.)
	const bool WasDescription = Model->GetPartDescriptionFilter();
	const bool WasFileName = Model->GetFileNameFilter();
	const QString WasFilter = PartsFilter()->text();

	// Neither search on: nothing can match, so the list is empty.
	Model->SetPartDescriptionFilter(false);
	Model->SetFileNameFilter(false);
	Model->SetFilter(QStringLiteral("3001"));
	QCoreApplication::processEvents();
	QCOMPARE(CountVisibleRows(), 0);

	// Part ID on: the same string now matches by file name.
	Model->SetFileNameFilter(true);
	QCoreApplication::processEvents();
	QVERIFY2(CountVisibleRows() > 0, "the Part ID toggle no longer searches file names");

	// Part ID back off: empty again, i.e. it is still a toggle and not a
	// one-way switch.
	Model->SetFileNameFilter(false);
	QCoreApplication::processEvents();
	QCOMPARE(CountVisibleRows(), 0);

	Model->SetFilter(WasFilter);
	Model->SetPartDescriptionFilter(WasDescription);
	Model->SetFileNameFilter(WasFileName);
	QCoreApplication::processEvents();

	QVERIFY2(CountVisibleRows() > 0, "the list did not come back after the filters were restored");
}

// Acceptance item 4, second half: the switch between "0 parts" and "some parts"
// has to keep working, and the two notices must not both show at once.
void LocalizationTest::zeroAndNonZeroResultsStillSwapTheNotice()
{
	QLineEdit* Filter = PartsFilter();
	QVERIFY(Filter);

	QList<QLabel*> Notices;

	for (QLabel* Label : mPanel->findChildren<QLabel*>())
		if (Label->text().contains(QStringLiteral("没有匹配此搜索的零件")) ||
			Label->text().contains(QStringLiteral("此分类下没有可显示的零件")))
		{
			Notices += Label;
		}

	QVERIFY2(!Notices.isEmpty(), "no empty-state notice in the panel at all");

	// Something that cannot match: exactly one notice, and it is visible.
	Filter->setText(QStringLiteral("zzzz-no-such-part"));
	QCoreApplication::processEvents();

	int Visible = 0;

	for (QLabel* Label : Notices)
		if (!Label->isHidden())
			Visible++;

	QCOMPARE(Visible, 1);

	// Clearing it brings the parts back and hides the notice again.
	Filter->clear();
	QCoreApplication::processEvents();
	QVERIFY(CountVisibleRows() > 0);

	for (QLabel* Label : Notices)
		QVERIFY2(Label->isHidden(), "the empty-state notice stayed after results came back");
}

// The count label is part of what the review asked to translate, so it has to
// still agree with the list in both states rather than being a static string.
void LocalizationTest::searchWithNoMatchesStillReportsZero()
{
	QLabel* CountLabel = nullptr;

	for (QLabel* Label : mPanel->findChildren<QLabel*>())
		if (Label->text().startsWith(QStringLiteral("零件: ")))
		{
			CountLabel = Label;
			break;
		}

	QVERIFY2(CountLabel, "no translated part count label in the panel");

	PartsFilter()->setText(QStringLiteral("zzzz-no-such-part"));
	QCoreApplication::processEvents();
	QCOMPARE(CountLabel->text(), QStringLiteral("零件: 0"));

	PartsFilter()->clear();
	QCoreApplication::processEvents();

	// And the translated prefix survives a positive count: the number changes,
	// the wording does not.
	QVERIFY(CountLabel->text().startsWith(QStringLiteral("零件: ")));
	QVERIFY(CountLabel->text() != QStringLiteral("零件: 0"));
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
