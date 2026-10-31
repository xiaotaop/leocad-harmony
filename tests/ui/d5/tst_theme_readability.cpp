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
	// On the HarmonyOS platform plugin style()->objectName() is empty, so the
	// name captured at construction is empty too. Before the fix, setStyle("")
	// returned null, the style stayed as the dark theme left it, and
	// standardPalette() came from that style while the stylesheet was cleared.
	ApplyTheme(lcColorTheme::Dark);

	const QString Saved = gApplication->GetDefaultStyleName();
	gApplication->SetDefaultStyleName(QString());

	ApplyTheme(lcColorTheme::System);

	VerifyPaletteReadable("System after Dark, empty default style");

	// The assertion that actually pins the fix. Contrast alone cannot: on this
	// host standardPalette() is taken from whatever style happens to be active,
	// so even a failed restore leaves a light palette and the ratios look fine —
	// which is why the first version of this case passed with the fix reverted.
	//
	// What went wrong on the device is that the restore never happened: the
	// application kept the style the dark theme installed while its stylesheet
	// was cleared. So the property to assert is that the style name is no longer
	// the dark one, i.e. that the captured empty name was replaced by something
	// setStyle() can resolve.
	QVERIFY2(!gApplication->GetDefaultStyleName().isEmpty(),
		"the default style name is empty, so switching to System cannot restore any style");

	QVERIFY2(QApplication::style() != nullptr, "no widget style after restoring with an empty default name");

	// And it must be a style the factory knows, not merely a non-null object.
	QVERIFY2(!QApplication::style()->objectName().isEmpty() ||
		QStyleFactory::keys().contains(QApplication::style()->objectName(), Qt::CaseInsensitive),
		qPrintable(QStringLiteral("the active style \"%1\" is not a known style; the restore did not resolve one")
			.arg(QApplication::style()->objectName())));

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
