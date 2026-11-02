// Prints what the UI's Chinese actually renders as, so a font problem can be
// told apart from a translation problem.
//
// The D7 row asks for 中文资源、字体和常用提示. Translation is covered by
// tests/ui/d7; this checks the other half. A missing glyph does not show up as
// text at all — Qt draws nothing or a box — so a screenshot can look like a
// translation bug when the string is in fact correct and the font is at fault.
//
// Run it on the device as well as the host. The host result only proves the host
// fonts; the HarmonyOS system font is a different set of files, and a font
// fallback that works here can still leave 缺字 there. Comparing the two outputs
// is the point.

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QString>
#include <QStringList>
#include <QTextStream>

namespace
{

// inFont() answers "does the font this resolves to have a glyph for this
// character"; a false is what draws as an empty box. Latin is skipped because it
// is not what this is looking for and would only dilute the count.
struct Coverage
{
	int Missing = 0;
	QChar FirstMissing;
};

Coverage Measure(const QFont& Font, const QString& Text)
{
	const QFontMetrics Metrics(Font);
	Coverage Result;

	for (const QChar Ch : Text)
	{
		if (Ch.unicode() < 0x80)
			continue;

		if (!Metrics.inFont(Ch))
		{
			if (Result.Missing == 0)
				Result.FirstMissing = Ch;
			Result.Missing++;
		}
	}

	return Result;
}

// The strings below are the ones the device review actually saw, plus the
// long-worded notices. Keeping them in one place means the output can be diffed
// between host and device without re-reading the .ts.
QString SamplePanel()
{
	return QString::fromUtf8("筛选零件筛选类别所有零件使用中子模型收藏夹");
}

QString SampleMenus()
{
	return QString::fromUtf8("编辑零件库显示零件名称显示带图案零件显示零件别名图标筛选");
}

QString SampleFilterMenu()
{
	return QString::fromUtf8("正则表达式区分大小写零件描述");
}

QString SampleColours()
{
	return QString::fromUtf8("实色半透明特殊浅灰色黑色浅蓝灰色主颜色无颜色透明无色乳白");
}

QString SampleEmptyStates()
{
	return QString::fromUtf8("没有匹配此搜索的零件请换一个关键词或清空搜索框此分类下没有可显示的零件");
}

QString SampleBanner()
{
	return QString::fromUtf8("只有少量零件可用因此使用其他零件的模型打开后会缺少零件");
}

void Report(QTextStream& Out, const QString& Label, const QFont& Font)
{
	static const QList<QPair<QString, QString>> Samples = {
		{ QStringLiteral("panel"), SamplePanel() },
		{ QStringLiteral("menus"), SampleMenus() },
		{ QStringLiteral("filter menu"), SampleFilterMenu() },
		{ QStringLiteral("colours"), SampleColours() },
		{ QStringLiteral("empty states"), SampleEmptyStates() },
		{ QStringLiteral("banner"), SampleBanner() },
	};

	int Total = 0;

	for (const auto& Sample : Samples)
	{
		const Coverage Result = Measure(Font, Sample.second);
		Total += Result.Missing;

		if (Result.Missing > 0)
		{
			Out << "  MISSING " << Result.Missing << " in " << Sample.first
				<< ", first U+" << QString::number(Result.FirstMissing.unicode(), 16) << "\n";
		}
	}

	Out << Label << "\tresolved=" << QFontInfo(Font).family() << "\tmissing=" << Total << "\n";
}

} // namespace

int main(int argc, char** argv)
{
	QApplication App(argc, argv);

	QTextStream Out(stdout);
	Out << "Qt " << qVersion() << ", platform " << QGuiApplication::platformName() << "\n";

	const QStringList Families = QFontDatabase::families();

	Out << "\n# Chinese-capable families present (" << Families.size() << " families total)\n";
	int Chinese = 0;

	for (const QString& Family : Families)
	{
		if (!QFontDatabase::families(QFontDatabase::SimplifiedChinese).contains(Family))
			continue;

		Chinese++;
		Out << "  " << Family << "\n";
	}

	Out << Chinese << " of " << Families.size() << " families offer Simplified Chinese\n";

	Out << "\n# Glyph coverage\n";
	Report(Out, QStringLiteral("default"), QApplication::font());

	// The families a HarmonyOS build might resolve to, so the output says whether
	// a fallback exists at all rather than only that the default happens to work.
	const QStringList Candidates = {
		QStringLiteral("HarmonyOS Sans SC"),
		QStringLiteral("HarmonyOS Sans"),
		QStringLiteral("Noto Sans CJK SC"),
		QStringLiteral("Noto Sans SC"),
		QStringLiteral("Source Han Sans SC"),
		QStringLiteral("WenQuanYi Zen Hei"),
		QStringLiteral("DejaVu Sans"),
	};

	for (const QString& Family : Candidates)
		Report(Out, QStringLiteral("  ") + Family, QFont(Family));

	// Line height and width: a CJK glyph is taller than a Latin one, and clipping
	// the top and bottom is a layout bug that no missing-glyph count would catch.
	const QFontMetrics Metrics(QApplication::font());

	Out << "\n# Layout metrics (default font)\n";
	Out << "  height=" << Metrics.height() << " ascent=" << Metrics.ascent()
		<< " descent=" << Metrics.descent() << "\n";
	Out << "  筛选零件 = " << Metrics.horizontalAdvance(QString::fromUtf8("筛选零件")) << " px\n";
	Out << "  Filter Parts = " << Metrics.horizontalAdvance(QStringLiteral("Filter Parts")) << " px\n";
	Out << "  Regular Expression = " << Metrics.horizontalAdvance(QStringLiteral("Regular Expression")) << " px\n";
	Out << "  正则表达式 = " << Metrics.horizontalAdvance(QString::fromUtf8("正则表达式")) << " px\n";

	// The whole point of the menu-width fix was that the Chinese entry is not
	// necessarily narrower than the English one it replaced; if it ever is wider,
	// this is where that shows.
	Out << "\n# Longest menu entry per menu, as drawn\n";
	Out << "  options menu: " << Metrics.horizontalAdvance(QString::fromUtf8("编辑零件库...")) << " px\n";
	Out << "  filter menu:  " << Metrics.horizontalAdvance(QString::fromUtf8("正则表达式")) << " px\n";

	return 0;
}
