// Verifies the first-release preferences scope: which settings are hidden, and
// which must survive.
//
// Reported on device (docs/D4_DEFECTS.md D4-02, issue #14): View > Preferences
// > General still offered the update check, POV-Ray, LGEO and the external parts
// library path, and the Rendering page still offered the Blender addon settings.
// Removing the menu entries that reach those features does not remove the
// settings that configure them.
//
// Why this does not build the dialog: its constructor calls
// gMainWindow->GetActiveView()->GetWidget()->context()->functions() to query the
// GL line-width range, and on an offscreen platform that context is null, so the
// dialog segfaults before any assertion runs. That is upstream behaviour, not
// something this change should paper over. What can be checked without a GPU is
// the scope itself, so the dialog exposes its two name lists and this test reads
// them against the real .ui.
//
// The two failure modes pull in opposite directions:
//
//   too little hidden -> a setting for a feature that cannot run
//   too much hidden   -> a setting the release still needs, silently gone
//
// The second is the easier one to ship by accident: CustomPathsGroup holds the
// parts library path (out of scope) *and* the colour and minifig configuration
// files (in scope), so hiding the group looks tidy and takes two needed rows.

#include <QDialog>
#include <QFile>
#include <QSet>
#include <QXmlStreamReader>
#include <QtTest>

#include "lc_qpreferencesdialog.h"
#include "ui_lc_qpreferencesdialog.h"

class PreferencesScopeTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();

	// Every name the dialog hides must exist in the .ui, or the hide() is a
	// no-op and the setting stays on screen.
	void hiddenWidgetsExistInTheUi();
	void keptWidgetsExistInTheUi();

	// The two lists must not overlap: hiding and keeping the same widget is a
	// contradiction that would make the guarantee meaningless.
	void listsDoNotOverlap();

	// The specific things the device reported, named one by one so a failure
	// says which setting came back.
	void deviceReportedSettingsAreInScope();

	// The trap: the colour and minifig rows share a group box with the hidden
	// parts-library path.
	void keptRowsInThePathsGroupAreNotHidden();

	// Runs the real hiding code against a real widget tree built from the .ui.
	// Checking the name lists against the file cannot catch a name that resolves
	// at review time and not at runtime — which is exactly the review finding.
	void scopeActuallyHidesOnARealTree();

	// The review finding itself: on Windows/macOS the constructor deletes
	// ui->povrayLabel before the scope runs, so a scope that requires every name
	// to resolve aborts there. This pins the ordering requirement.
	void scopeMustRunBeforeThePlatformCleanup();

private:
	// Widget names present in the .ui, read directly rather than through uic:
	// the point is to check the names the dialog looks up at runtime.
	QSet<QString> UiWidgetNames() const;

	static bool Contains(const char* const* List, int Count, const char* Name);

	QString mUiPath;
};

void PreferencesScopeTest::initTestCase()
{
	mUiPath = QFINDTESTDATA("../../../qt/lc_qpreferencesdialog.ui");
	QVERIFY2(!mUiPath.isEmpty(), "preferences .ui not found");
}

bool PreferencesScopeTest::Contains(const char* const* List, int Count, const char* Name)
{
	for (int Index = 0; Index < Count; Index++)
	{
		if (QLatin1String(List[Index]) == QLatin1String(Name))
			return true;
	}

	return false;
}

QSet<QString> PreferencesScopeTest::UiWidgetNames() const
{
	QSet<QString> Names;

	QFile File(mUiPath);

	if (!File.open(QIODevice::ReadOnly))
		return Names;

	QXmlStreamReader Reader(&File);

	while (!Reader.atEnd())
	{
		Reader.readNext();

		if (Reader.isStartElement() && Reader.name() == QLatin1String("widget"))
		{
			const QString Name = Reader.attributes().value(QLatin1String("name")).toString();

			if (!Name.isEmpty())
				Names.insert(Name);
		}
	}

	return Names;
}

void PreferencesScopeTest::hiddenWidgetsExistInTheUi()
{
	const QSet<QString> Names = UiWidgetNames();
	QVERIFY2(!Names.isEmpty(), "no widgets found in the .ui");

	QStringList Missing;

	for (int Index = 0; Index < lcPreferencesDialog::ScopedWidgets::HiddenCount(); Index++)
	{
		const QString Name = QLatin1String(lcPreferencesDialog::ScopedWidgets::Hidden()[Index]);

		if (!Names.contains(Name))
			Missing.append(Name);
	}

	QVERIFY2(Missing.isEmpty(),
		qPrintable(QStringLiteral("hidden names not present in the .ui, so hide() does nothing: %1")
			.arg(Missing.join(QLatin1String(", ")))));
}

void PreferencesScopeTest::keptWidgetsExistInTheUi()
{
	const QSet<QString> Names = UiWidgetNames();
	QVERIFY2(!Names.isEmpty(), "no widgets found in the .ui");

	QStringList Missing;

	for (int Index = 0; Index < lcPreferencesDialog::ScopedWidgets::KeptCount(); Index++)
	{
		const QString Name = QLatin1String(lcPreferencesDialog::ScopedWidgets::Kept()[Index]);

		if (!Names.contains(Name))
			Missing.append(Name);
	}

	// A kept name that does not exist would make the in-scope guarantee vacuous.
	QVERIFY2(Missing.isEmpty(),
		qPrintable(QStringLiteral("kept names not present in the .ui: %1").arg(Missing.join(QLatin1String(", ")))));
}

void PreferencesScopeTest::listsDoNotOverlap()
{
	QStringList Both;

	for (int Index = 0; Index < lcPreferencesDialog::ScopedWidgets::KeptCount(); Index++)
	{
		const char* Name = lcPreferencesDialog::ScopedWidgets::Kept()[Index];

		if (Contains(lcPreferencesDialog::ScopedWidgets::Hidden(),
			lcPreferencesDialog::ScopedWidgets::HiddenCount(), Name))
		{
			Both.append(QLatin1String(Name));
		}
	}

	QVERIFY2(Both.isEmpty(),
		qPrintable(QStringLiteral("widgets both hidden and kept: %1").arg(Both.join(QLatin1String(", ")))));
}

void PreferencesScopeTest::deviceReportedSettingsAreInScope()
{
	// Named individually so a regression reports the setting that came back
	// rather than just "a list changed".
	struct Entry { const char* Widget; const char* What; };
	const Entry Reported[] =
	{
		{ "checkForUpdates", "the update check" },
		{ "partsLibrary", "the external parts library path" },
		{ "povrayExecutable", "the POV-Ray executable" },
		{ "lgeoPath", "the LGEO path" },
		{ "BlenderAddonSettingsButton", "the Blender addon settings" },
	};

	for (const Entry& Item : Reported)
	{
		QVERIFY2(Contains(lcPreferencesDialog::ScopedWidgets::Hidden(),
			lcPreferencesDialog::ScopedWidgets::HiddenCount(), Item.Widget),
			qPrintable(QStringLiteral("%1 is still shown in Preferences (D4-02 / issue #14)")
				.arg(QLatin1String(Item.What))));
	}
}

void PreferencesScopeTest::keptRowsInThePathsGroupAreNotHidden()
{
	// These live inside CustomPathsGroup together with the parts-library path.
	// Hiding the group would take them with it, so they are checked as kept
	// explicitly.
	const char* const InTheSameGroup[] =
	{
		"ColorConfigEdit", "ColorConfigBrowseButton",
		"MinifigSettingsEdit", "MinifigSettingsBrowseButton",
	};

	for (const char* Name : InTheSameGroup)
	{
		QVERIFY2(Contains(lcPreferencesDialog::ScopedWidgets::Kept(),
			lcPreferencesDialog::ScopedWidgets::KeptCount(), Name),
			qPrintable(QStringLiteral("%1 is not kept, but it is a local settings file the app reads")
				.arg(QLatin1String(Name))));
	}
}

namespace
{
// A dialog-shaped host holding the real preference widgets, built by the same
// uic-generated setupUi() the dialog uses. This is the container the review
// asked for: it exercises the actual object tree without the dialog
// constructor's GL query.
class PreferencesHost : public QDialog
{
public:
	PreferencesHost()
	{
		mUi.setupUi(this);
	}

	QWidget* Widget(const char* Name) const
	{
		return findChild<QWidget*>(QLatin1String(Name));
	}

private:
	Ui_lcPreferencesDialog mUi;
};
}

void PreferencesScopeTest::scopeActuallyHidesOnARealTree()
{
	PreferencesHost Host;

	// Record which kept widgets were already hidden before the scope runs, so
	// the check below is about what the function changed rather than about the
	// host's initial state.
	QSet<QString> BeforeHidden;

	for (int Index = 0; Index < lcPreferencesDialog::ScopedWidgets::KeptCount(); Index++)
	{
		QWidget* Widget = Host.Widget(lcPreferencesDialog::ScopedWidgets::Kept()[Index]);

		if (Widget && Widget->isHidden())
			BeforeHidden.insert(QLatin1String(lcPreferencesDialog::ScopedWidgets::Kept()[Index]));
	}

	// The real hiding path, no assertions bypassed.
	const int Missing = lcPreferencesDialog::HideFirstReleaseWidgets(&Host);

	QVERIFY2(Missing == 0,
		qPrintable(QStringLiteral("%1 hidden names did not resolve on the real tree").arg(Missing)));

	// Every hidden name must actually be hidden now.
	QStringList StillShown;

	for (int Index = 0; Index < lcPreferencesDialog::ScopedWidgets::HiddenCount(); Index++)
	{
		const QString Name = QLatin1String(lcPreferencesDialog::ScopedWidgets::Hidden()[Index]);
		QWidget* Widget = Host.Widget(lcPreferencesDialog::ScopedWidgets::Hidden()[Index]);

		if (!Widget || !Widget->isHidden())
			StillShown.append(Name);
	}

	QVERIFY2(StillShown.isEmpty(),
		qPrintable(QStringLiteral("still visible after hiding: %1").arg(StillShown.join(QLatin1String(", ")))));

	// And the kept list must still be visible on the same tree, so this proves
	// the hiding ran rather than the widgets being absent for another reason.
	//
	// Tab pages other than the current one are hidden by QTabWidget itself, so
	// they are not evidence of anything. Comparing against the state before the
	// call is the honest test: what matters is that this function did not change
	// them, not that they happen to be visible in a freshly built host.
	QStringList HiddenByMistake;

	for (int Index = 0; Index < lcPreferencesDialog::ScopedWidgets::KeptCount(); Index++)
	{
		QWidget* Widget = Host.Widget(lcPreferencesDialog::ScopedWidgets::Kept()[Index]);

		if (Widget && Widget->isHidden() && !BeforeHidden.contains(QLatin1String(lcPreferencesDialog::ScopedWidgets::Kept()[Index])))
			HiddenByMistake.append(QLatin1String(lcPreferencesDialog::ScopedWidgets::Kept()[Index]));
	}

	QVERIFY2(HiddenByMistake.isEmpty(),
		qPrintable(QStringLiteral("in-scope widgets were hidden: %1").arg(HiddenByMistake.join(QLatin1String(", ")))));
}

void PreferencesScopeTest::scopeMustRunBeforeThePlatformCleanup()
{
	// Reproduces the review finding. On Windows and macOS the constructor does
	// "delete ui->povrayLabel" during platform cleanup; if the scope ran after
	// that, this is the state it would see — the name no longer resolves.
	PreferencesHost Host;

	QWidget* PovrayLabel = Host.Widget("povrayLabel");
	QVERIFY2(PovrayLabel, "povrayLabel missing from the .ui; the test below would be vacuous");
	delete PovrayLabel;

	const int Missing = lcPreferencesDialog::HideFirstReleaseWidgets(&Host);

	// Run after the cleanup, one name legitimately does not resolve. The point of
	// the case is that the count reports it instead of aborting: with the dialog
	// asserting on a non-zero count, running the scope in this order would abort
	// every Debug build on those platforms, which is what review reproduced.
	QVERIFY2(Missing == 1,
		qPrintable(QStringLiteral("expected exactly the deleted name to be reported, got %1").arg(Missing)));

	// The constructor must therefore call the scope before the platform cleanup.
	// Checked against the source rather than by constructing the dialog, which
	// needs a GL context.
	QFile Source(QFINDTESTDATA("../../../qt/lc_qpreferencesdialog.cpp"));
	QVERIFY2(Source.open(QIODevice::ReadOnly), "cannot read lc_qpreferencesdialog.cpp");

	const QString Text = QString::fromUtf8(Source.readAll());
	const int ScopeIndex = Text.indexOf(QLatin1String("ApplyFirstReleasePreferencesScope();"));
	const int DeleteIndex = Text.indexOf(QLatin1String("delete ui->povrayLabel;"));

	QVERIFY2(ScopeIndex >= 0, "the constructor no longer calls ApplyFirstReleasePreferencesScope()");
	QVERIFY2(DeleteIndex >= 0, "the platform cleanup no longer deletes ui->povrayLabel");

	QVERIFY2(ScopeIndex < DeleteIndex,
		"ApplyFirstReleasePreferencesScope() runs after the platform cleanup, so povrayLabel is already deleted and Debug builds abort");
}

int main(int argc, char** argv)
{
	QApplication Application(argc, argv);
	PreferencesScopeTest Test;

	return QTest::qExec(&Test, argc, argv);
}

#include "tst_preferences_scope.moc"
