// Verifies the colour theme leaves readable text behind, whichever way it is
// switched.
//
// Reported on device (D5-02 / issue #14): starting in the Dark theme, changing
// the colour theme to System and saving left the main menu text and the current
// colour name invisible. The setting persisted (ColorTheme=1 survived a restart
// with a new PID), so the failure is in how the theme is applied, not in how it
// is stored.
//
// What the desktop could not show: on this platform QApplication::style() has a
// usable object name, so the restore path always found a style. On the HarmonyOS
// platform plugin the name is empty, and QApplication::setStyle("") returns null
// without changing anything — the application kept the style the dark theme had
// installed, took standardPalette() from it, and cleared the dark stylesheet.
// The measurable result is text whose contrast against the surface it is drawn
// on collapses.
//
// These cases drive the real lcApplication::UpdateStyle() and judge the palette
// it produces. Contrast is the user-visible property the device report
// describes, so it is what the assertions use.

#include <QApplication>
#include <QMenuBar>
#include <QPalette>
#include <QStyleFactory>
#include <QtTest>
#include <cmath>

#include "lc_application.h"

namespace
{
// WCAG 2.1 relative luminance and contrast ratio. The threshold used below (4.5)
// is the AA level for normal text; the numbers on a healthy palette are around
// 11 to 21, so anything near the threshold means something is badly wrong
// rather than marginally off.
double RelativeLuminance(const QColor& Colour)
{
	auto Channel = [](double Value)
	{
		Value /= 255.0;
		return Value <= 0.03928 ? Value / 12.92 : std::pow((Value + 0.055) / 1.055, 2.4);
	};

	return 0.2126 * Channel(Colour.red()) + 0.7152 * Channel(Colour.green()) + 0.0722 * Channel(Colour.blue());
}

double ContrastRatio(const QColor& A, const QColor& B)
{
	double Lighter = RelativeLuminance(A);
	double Darker = RelativeLuminance(B);

	if (Lighter < Darker)
		std::swap(Lighter, Darker);

	return (Lighter + 0.05) / (Darker + 0.05);
}

constexpr double MinimumReadableContrast = 4.5;

}

class ThemeReadabilityTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();

	// The device sequence: Dark first, then System. The second step is the one
	// that broke.
	void darkThemeIsReadable();
	void systemAfterDarkIsReadable();

	// A platform whose style has no usable object name — the HarmonyOS case.
	// Written as its own case because this is the difference between "works on
	// the desktop" and "the device cannot read the menu".
	void systemAfterDarkIsReadableWithoutAStyleObjectName();

	// What the two surfaces the report named actually read from.
	void menuAndColourNameTextUseReadableRoles();

private:
	// Sets the theme and applies it through the real code path.
	void ApplyTheme(lcColorTheme Theme);

	// Every text-on-surface pair a menu bar and a colour list depend on, with a
	// description so a failure names the surface.
	void VerifyPaletteReadable(const char* Stage);
};

void ThemeReadabilityTest::initTestCase()
{
	QVERIFY(gApplication);
}

void ThemeReadabilityTest::ApplyTheme(lcColorTheme Theme)
{
	gApplication->mPreferences.mColorTheme = Theme;
	gApplication->mPreferences.SetInterfaceColors(Theme);
	gApplication->UpdateStyle();

	// Let Qt deliver the palette/style changes the way it would before the user
	// looks at the window.
	QCoreApplication::processEvents();
}

void ThemeReadabilityTest::VerifyPaletteReadable(const char* Stage)
{
	const QPalette Palette = QApplication::palette();

	struct Pair
	{
		QPalette::ColorRole Foreground;
		QPalette::ColorRole Background;
		const char* What;
	};

	// Menu bars and the colour list draw text over the window; item views and
	// the parts panel draw over base. Both appear in the device report.
	const Pair Pairs[] =
	{
		{ QPalette::WindowText, QPalette::Window, "menu and window text" },
		{ QPalette::Text, QPalette::Base, "list and panel text" },
		{ QPalette::ButtonText, QPalette::Button, "button text" },
	};

	for (const Pair& Item : Pairs)
	{
		const QColor Foreground = Palette.color(Item.Foreground);
		const QColor Background = Palette.color(Item.Background);
		const double Ratio = ContrastRatio(Foreground, Background);

		QVERIFY2(Ratio >= MinimumReadableContrast,
			qPrintable(QStringLiteral("%1: %2 on %3 has contrast %4, below %5 (after %6)")
				.arg(QLatin1String(Item.What), Foreground.name(), Background.name())
				.arg(Ratio, 0, 'f', 2).arg(MinimumReadableContrast).arg(QLatin1String(Stage))));
	}
}

void ThemeReadabilityTest::darkThemeIsReadable()
{
	ApplyTheme(lcColorTheme::Dark);
	VerifyPaletteReadable("Dark");
}

void ThemeReadabilityTest::systemAfterDarkIsReadable()
{
	// The exact order from the device: dark, then switch to System.
	ApplyTheme(lcColorTheme::Dark);
	ApplyTheme(lcColorTheme::System);

	VerifyPaletteReadable("System after Dark");

	// The stylesheet the dark theme installs must be gone, or its colours would
	// override the palette that was just restored.
	QVERIFY2(qApp->styleSheet().isEmpty(),
		"the dark stylesheet is still installed after switching to System");
}

void ThemeReadabilityTest::systemAfterDarkIsReadableWithoutAStyleObjectName()
{
	// The platform the device reports: style()->objectName() is empty, so the name
	// captured at construction is empty too and setStyle("") cannot restore
	// anything.
	//
	// Two traps this case has to avoid, both found in review:
	//
	//   * On this host the default style is Fusion, which is also what the
	//     fallback picks, so "the style is Fusion afterwards" passes whether or
	//     not the fallback was applied. A distinguishable style has to be in
	//     place first.
	//
	//   * Comparing style class names does not work either. Once the dark
	//     stylesheet is installed Qt wraps the style in QStyleSheetStyle, whose
	//     class name is the same whatever sits underneath, so the comparison is
	//     blind for exactly the window where the switch happens.
	//
	// What is observable regardless of wrapping is the palette the style
	// produces. Windows and Fusion have different standard palettes (measured:
	// Window #d4d0c8 vs #efefef), so the window colour after the switch tells
	// which style is actually feeding it.
	const QStringList KnownStyles = QStyleFactory::keys();

	if (!KnownStyles.contains(QLatin1String("Windows"), Qt::CaseInsensitive))
		QSKIP("needs the Windows style to build a distinguishable state");

	ApplyTheme(lcColorTheme::Dark);

	// Put Windows in place: that is the stale style the fallback must replace.
	QVERIFY2(QApplication::setStyle(QLatin1String("Windows")) != nullptr, "could not install the Windows style");

	// Confirm the injected style really is feeding the palette, otherwise the
	// check at the end would have nothing to distinguish.
	const QColor InjectedWindow = QApplication::style()->standardPalette().color(QPalette::Window);
	QVERIFY2(InjectedWindow != QColor(0xef, 0xef, 0xef),
		qPrintable(QStringLiteral("the injected style already behaves like Fusion (Window %1); the case could not tell an applied fallback from a stale one")
			.arg(InjectedWindow.name())));

	const QString Saved = gApplication->GetDefaultStyleName();
	gApplication->SetDefaultStyleName(QString());

	ApplyTheme(lcColorTheme::System);

	// 1. The captured name must have been backfilled to a key the factory knows.
	//    The earlier version accepted any non-empty name via "!empty || known",
	//    which never checked recognisability.
	const QString Restored = gApplication->GetDefaultStyleName();
	QVERIFY2(!Restored.isEmpty(),
		"the default style name is empty, so switching to System cannot restore any style");
	QVERIFY2(KnownStyles.contains(Restored, Qt::CaseInsensitive),
		qPrintable(QStringLiteral("the backfilled style name \"%1\" is not a style the factory knows").arg(Restored)));

	// 2. The palette must no longer come from the injected style. This is the
	//    assertion the leader's mutation cannot survive: with the fallback
	//    skipped and only the name written back, Windows stays installed and the
	//    window colour stays #d4d0c8.
	const QColor ActiveWindow = QApplication::palette().color(QPalette::Window);
	QVERIFY2(ActiveWindow != InjectedWindow,
		qPrintable(QStringLiteral("the window colour is still %1, the value the injected style produces; the fallback was recorded but not applied")
			.arg(ActiveWindow.name())));

	// 3. And the result must be readable, which is the user-visible property.
	VerifyPaletteReadable("System after Dark, empty default style");

	gApplication->SetDefaultStyleName(Saved);
	ApplyTheme(lcColorTheme::System);
}

void ThemeReadabilityTest::menuAndColourNameTextUseReadableRoles()
{
	// The two surfaces the device report named. A menu bar draws with
	// WindowText over Window; the colour list paints its names with the same
	// pair. Checking the roles directly documents what a future palette edit must
	// keep readable, instead of relying on "the app looked fine".
	ApplyTheme(lcColorTheme::Dark);
	ApplyTheme(lcColorTheme::System);

	const QPalette Palette = QApplication::palette();

	const QColor WindowText = Palette.color(QPalette::WindowText);
	const QColor Window = Palette.color(QPalette::Window);

	QVERIFY2(WindowText != Window, "menu text colour equals the window colour, so nothing would be readable");
	QVERIFY2(ContrastRatio(WindowText, Window) >= MinimumReadableContrast,
		qPrintable(QStringLiteral("menu text %1 on window %2 is below the readable contrast")
			.arg(WindowText.name(), Window.name())));
}

// A custom main rather than QTEST_MAIN: UpdateStyle() lives on lcApplication and
// reads gApplication->mPreferences, so the test needs the real application
// object, not a plain QApplication.
int main(int argc, char** argv)
{
	lcApplication Application(argc, argv);
	ThemeReadabilityTest Test;

	return QTest::qExec(&Test, argc, argv);
}

#include "tst_theme_readability.moc"
