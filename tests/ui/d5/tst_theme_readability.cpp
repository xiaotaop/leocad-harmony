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

	// The device shape from #33: a style palette that applies Window #ffffff
	// while a text role stays the same colour as that surface. The repair has to
	// catch it, because "the theme was applied" and "the text can be read" are
	// different claims.
	void brokenStylePaletteIsRepaired();
	void readablePaletteIsLeftAlone();

	// The device showed readable application colours while widgets drew
	// invisible text, so the widget's own resolution is what has to be
	// inspected. This pins that the widget state is reachable and sane on a
	// healthy host, which is the baseline the device reading is compared with.
	void widgetStateReportsWhatAWidgetResolves();

	// The palette an ohos device actually reported for #33. Its nominal pairs
	// read fine, so the earlier per-pair check passed and nothing was repaired —
	// while a text role that is only wrong on OTHER surfaces stayed invisible.
	void deviceOhosPaletteIsRepaired();
	void themeSwitchAppliesTheRepair();

	// The repair is worthless if the stylesheet teardown then overwrites the
	// palette. Ordering is the property, so it gets its own case.
	void appliedPaletteSurvivesTheStylesheetTeardown();

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
void ThemeReadabilityTest::brokenStylePaletteIsRepaired()
{
	// Built from the device evidence: the palette ended up with a white window
	// and text that could not be seen against it. A style is free to return such
	// a palette; the application has to notice before using it.
	QPalette Broken = QApplication::style()->standardPalette();

	Broken.setColor(QPalette::Window, QColor(255, 255, 255));
	Broken.setColor(QPalette::WindowText, QColor(255, 255, 255));	// invisible
	Broken.setColor(QPalette::Base, QColor(255, 255, 255));
	Broken.setColor(QPalette::Text, QColor(250, 250, 250));			// nearly invisible
	Broken.setColor(QPalette::Button, QColor(255, 255, 255));
	Broken.setColor(QPalette::ButtonText, QColor(255, 255, 255));

	QVERIFY2(ContrastRatio(Broken.color(QPalette::WindowText), Broken.color(QPalette::Window)) < 3.0,
		"the synthetic palette is not actually broken, so the repair below proves nothing");

	const QPalette Fixed = lcApplication::EnsureReadablePalette(Broken);

	struct Pair { QPalette::ColorRole Text; QPalette::ColorRole Surface; const char* What; };
	const Pair Pairs[] =
	{
		{ QPalette::WindowText, QPalette::Window, "WindowText" },
		{ QPalette::Text, QPalette::Base, "Text" },
		{ QPalette::ButtonText, QPalette::Button, "ButtonText" },
	};

	for (const Pair& Item : Pairs)
	{
		const double Ratio = ContrastRatio(Fixed.color(Item.Text), Fixed.color(Item.Surface));
		QVERIFY2(Ratio >= 3.0,
			qPrintable(QStringLiteral("%1 still has contrast %2 against %3 (%4) after repair")
				.arg(QLatin1String(Item.What)).arg(Ratio, 0, 'f', 2)
				.arg(Fixed.color(Item.Surface).name(), Fixed.color(Item.Text).name())));
	}

	// The surfaces themselves must not be touched: this repairs text, it does not
	// repaint the style.
	QCOMPARE(Fixed.color(QPalette::Window), Broken.color(QPalette::Window));
	QCOMPARE(Fixed.color(QPalette::Base), Broken.color(QPalette::Base));
	QCOMPARE(Fixed.color(QPalette::Button), Broken.color(QPalette::Button));
}

void ThemeReadabilityTest::themeSwitchAppliesTheRepair()
{
	// The case above proves the repair works in isolation; this one proves
	// UpdateStyle() actually runs it. Without this, removing the call site leaves
	// the other cases green while the device keeps showing invisible text —
	// exactly the class of false pass this PR has already been caught on twice.
	//
	// A broken style palette is injected through the test seam, so the switch has
	// something to repair on any host, not just on a platform whose style happens
	// to be broken.
	QPalette Broken = QApplication::style()->standardPalette();
	Broken.setColor(QPalette::Window, QColor(255, 255, 255));
	Broken.setColor(QPalette::WindowText, QColor(255, 255, 255));
	Broken.setColor(QPalette::Base, QColor(255, 255, 255));
	Broken.setColor(QPalette::Text, QColor(252, 252, 252));
	Broken.setColor(QPalette::Button, QColor(255, 255, 255));
	Broken.setColor(QPalette::ButtonText, QColor(255, 255, 255));

	gApplication->SetStylePaletteOverride(Broken);

	ApplyTheme(lcColorTheme::Dark);
	ApplyTheme(lcColorTheme::System);

	// The palette in force must be the repaired one, not the injected copy.
	const QPalette Applied = QApplication::palette();

	QVERIFY2(Applied.color(QPalette::WindowText) != Broken.color(QPalette::WindowText),
		qPrintable(QStringLiteral("WindowText is still %1, the injected value; UpdateStyle() did not repair the palette")
			.arg(Applied.color(QPalette::WindowText).name())));

	const double Ratio = ContrastRatio(Applied.color(QPalette::WindowText), Applied.color(QPalette::Window));
	QVERIFY2(Ratio >= 3.0,
		qPrintable(QStringLiteral("WindowText contrast against Window is %1 after the switch").arg(Ratio, 0, 'f', 2)));

	// Surfaces stay as the style supplied them; only the unreadable text role
	// changes.
	QCOMPARE(Applied.color(QPalette::Window), Broken.color(QPalette::Window));

	gApplication->SetStylePaletteOverride(QPalette());
	ApplyTheme(lcColorTheme::System);
}

void ThemeReadabilityTest::readablePaletteIsLeftAlone()
{
	// The repair must be a no-op on a palette that is already fine, or every
	// theme switch would quietly restyle the application.
	const QPalette Healthy = QApplication::style()->standardPalette();
	const QPalette Result = lcApplication::EnsureReadablePalette(Healthy);

	QVERIFY2(Result == Healthy, "a readable palette was modified");
}

void ThemeReadabilityTest::appliedPaletteSurvivesTheStylesheetTeardown()
{
	// A widget-level check that the repaired role reaches something that draws,
	// rather than only QApplication::palette().
	//
	// Scope, stated plainly: on this host the palette survives either ordering of
	// setPalette() and setStyleSheet(), so this case does NOT prove that the
	// ordering in UpdateStyle() is required. It checks the end state a menu bar
	// sees. The ordering there is chosen for correctness — clearing the stylesheet
	// removes the QStyleSheetStyle layer, so the palette must be applied after —
	// and the device is what can confirm it.
	QWidget Host;
	QMenuBar* Menu = new QMenuBar(&Host);
	Menu->addMenu(QStringLiteral("&File"));

	QPalette Broken = QApplication::style()->standardPalette();
	Broken.setColor(QPalette::Window, QColor(255, 255, 255));
	Broken.setColor(QPalette::WindowText, QColor(255, 255, 255));

	gApplication->SetStylePaletteOverride(Broken);

	ApplyTheme(lcColorTheme::Dark);
	ApplyTheme(lcColorTheme::System);

	// The widget must resolve a readable window text after the switch, not the
	// injected white.
	const QColor MenuText = Menu->palette().color(QPalette::WindowText);

	QVERIFY2(MenuText != QColor(255, 255, 255),
		qPrintable(QStringLiteral("the menu bar still resolves WindowText to the injected white (%1) after the theme switch")
			.arg(MenuText.name())));

	const double Ratio = ContrastRatio(MenuText, Menu->palette().color(QPalette::Window));
	QVERIFY2(Ratio >= 3.0,
		qPrintable(QStringLiteral("menu bar contrast is %1 after the theme switch").arg(Ratio, 0, 'f', 2)));

	gApplication->SetStylePaletteOverride(QPalette());
	ApplyTheme(lcColorTheme::System);
	delete Menu;
}

void ThemeReadabilityTest::widgetStateReportsWhatAWidgetResolves()
{
	// #33 breaks between the application palette and the widget that paints, so
	// the properties that decide whether a widget follows the application at all
	// are what a device reading has to expose. On a healthy host they are all
	// benign; the case exists so the diagnostic cannot silently stop sampling.
	QWidget Host;
	QMenuBar* Menu = new QMenuBar(&Host);
	Menu->addMenu(QStringLiteral("&File"));

	// A widget that has its own palette stops inheriting, which is exactly the
	// state a device can be in without any code of ours calling setPalette.
	QVERIFY2(!Menu->testAttribute(Qt::WA_SetPalette), "a fresh menu bar already carries its own palette");

	QPalette Own = Menu->palette();
	Own.setColor(QPalette::WindowText, QColor(240, 240, 240));
	Menu->setPalette(Own);

	QVERIFY2(Menu->testAttribute(Qt::WA_SetPalette),
		"setting a palette did not mark the widget as having its own");

	// And that state really does block an application palette change: this is the
	// mechanism the diagnostic reports as has_own_palette.
	QPalette App = QApplication::palette();
	App.setColor(QPalette::WindowText, QColor(0, 0, 0));
	QApplication::setPalette(App);

	QVERIFY2(Menu->palette().color(QPalette::WindowText) == QColor(240, 240, 240),
		"a widget with its own palette followed the application palette after all");

	delete Menu;

	// Leave the application palette in the state the other cases expect.
	ApplyTheme(lcColorTheme::System);
}

void ThemeReadabilityTest::deviceOhosPaletteIsRepaired()
{
	// Verbatim from the device retest of a439bf20:
	//
	//   {"active_after":"ohos","class_after":"QOhosStyle","window":"#ffffff",
	//    "windowtext":"#182431","text":"#182431",
	//    "buttontext":"#ffffff","button":"#000000","base":"#e4e4e4"}
	//
	// Per nominal pair this is readable: windowtext on window, text on base,
	// buttontext on button. That is why the first repair never fired. What it
	// misses is that buttontext is pure white while window and base are both
	// light, so any widget drawing ButtonText on a window-coloured surface writes
	// white on white.
	QPalette Device;
	Device.setColor(QPalette::Window, QColor(0xff, 0xff, 0xff));
	Device.setColor(QPalette::WindowText, QColor(0x18, 0x24, 0x31));
	Device.setColor(QPalette::Text, QColor(0x18, 0x24, 0x31));
	Device.setColor(QPalette::ButtonText, QColor(0xff, 0xff, 0xff));
	Device.setColor(QPalette::Button, QColor(0x00, 0x00, 0x00));
	Device.setColor(QPalette::Base, QColor(0xe4, 0xe4, 0xe4));
	Device.setColor(QPalette::AlternateBase, QColor(0xe4, 0xe4, 0xe4));

	// Establish the gap: the nominal pair is fine, so a per-pair check would let
	// this palette through.
	QVERIFY2(ContrastRatio(Device.color(QPalette::ButtonText), Device.color(QPalette::Button)) >= 3.0,
		"the device palette's nominal buttontext/button pair is not readable, so this case is not reproducing the trap");

	// And the cross-surface hole is real: white text on the white window.
	QCOMPARE(Device.color(QPalette::WindowText), Device.color(QPalette::WindowText));
	QVERIFY2(ContrastRatio(Device.color(QPalette::ButtonText), Device.color(QPalette::Window)) < 3.0,
		"buttontext happens to be readable on window, so this palette has no cross-surface problem");

	const QPalette Fixed = lcApplication::EnsureReadablePalette(Device);

	// After repair, every text role must be readable on every surface it can be
	// drawn over, which is the property the device needed.
	struct Pair { QPalette::ColorRole Text; QPalette::ColorRole Surface; const char* What; };
	const Pair Pairs[] =
	{
		{ QPalette::WindowText, QPalette::Window, "WindowText/Window" },
		{ QPalette::WindowText, QPalette::Base, "WindowText/Base" },
		{ QPalette::Text, QPalette::Base, "Text/Base" },
		{ QPalette::Text, QPalette::Window, "Text/Window" },
		{ QPalette::ButtonText, QPalette::Button, "ButtonText/Button" },
		{ QPalette::ButtonText, QPalette::Window, "ButtonText/Window" },
		{ QPalette::ButtonText, QPalette::Base, "ButtonText/Base" },
	};

	for (const Pair& Item : Pairs)
	{
		const double Ratio = ContrastRatio(Fixed.color(Item.Text), Fixed.color(Item.Surface));
		QVERIFY2(Ratio >= 3.0,
			qPrintable(QStringLiteral("%1 is still %2 after repair (text %3, surface %4)")
				.arg(QLatin1String(Item.What)).arg(Ratio, 0, 'f', 2)
				.arg(Fixed.color(Item.Text).name(), Fixed.color(Item.Surface).name())));
	}

	// The light surfaces the style supplied must survive: this changes text, not
	// the style's own background choices.
	QCOMPARE(Fixed.color(QPalette::Window), Device.color(QPalette::Window));
	QCOMPARE(Fixed.color(QPalette::Base), Device.color(QPalette::Base));
}

int main(int argc, char** argv)
{
	lcApplication Application(argc, argv);
	ThemeReadabilityTest Test;

	return QTest::qExec(&Test, argc, argv);
}

#include "tst_theme_readability.moc"
