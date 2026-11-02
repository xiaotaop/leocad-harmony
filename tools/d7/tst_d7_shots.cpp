// D7 evidence: capture the parts panel as the Chinese first release shows it.
//
// Not a test — a screenshot tool. The device screenshots in
// docs/evidence/leader/2026-10-30/d4-device are what the review complained about
// (English "Filter Parts", "Parts: 4", elided "Regular...ression"), and the fix
// has to be shown, not asserted. The unit tests pin the strings and the menu
// width; this shows the two window sizes the acceptance criteria call for
// ("两种窗口尺寸下选项文字可辨识").
//
// Runs on the wayland platform under WSLg, which is the only one here with a real
// GL context for QOpenGLWidget (offscreen has no FBO and the panel segfaults).
// Screenshots are taken with QWidget::grab(), not an X11 screen grab: WSLg
// composites through Weston, so x11grab returns a black rectangle.
//
// Usage:
//   .devtools/env.sh; QT_QPA_PLATFORM=wayland ./leocad-d7-shots <outdir>

#include <QApplication>
#include <QDir>
#include <QLineEdit>
#include <QMenu>
#include <QPixmap>
#include <QToolButton>
#include <QTest>
#include <QTranslator>
#include <QTreeWidget>
#include <QtGlobal>
#include <cstdio>

#include "lc_application.h"
#include "lc_colorlist.h"
#include "lc_colors.h"
#include "lc_context.h"
#include "lc_library.h"
#include "lc_mainwindow.h"
#include "lc_partselectionwidget.h"
#include "project.h"

namespace
{

void Save(QWidget* Widget, const QString& Path)
{
	if (Widget == nullptr)
	{
		std::fprintf(stderr, "no widget for %s\n", qPrintable(Path));
		return;
	}

	// A menu that was never shown has no size yet; give it one so the capture
	// includes the whole thing rather than a corner.
	if (qobject_cast<QMenu*>(Widget) != nullptr)
		Widget->adjustSize();

	const QPixmap Shot = Widget->grab();

	if (Shot.isNull() || !Shot.save(Path))
		std::fprintf(stderr, "could not save %s\n", qPrintable(Path));
	else
		std::printf("%s  %dx%d\n", qPrintable(Path), Shot.width(), Shot.height());
}

QTreeWidget* CategoryTree(lcPartSelectionWidget* Panel)
{
	return Panel->findChild<QTreeWidget*>();
}

QLineEdit* PartsFilter(lcPartSelectionWidget* Panel)
{
	return Panel->findChild<QLineEdit*>(QStringLiteral("PartsFilter"));
}

QToolButton* OptionsButton(lcPartSelectionWidget* Panel)
{
	for (QToolButton* Button : Panel->findChildren<QToolButton*>())
		if (Button->menu() != nullptr)
			return Button;

	return nullptr;
}

// The empty-state notice is a QLabel over the viewport, hidden while there are
// rows. Capturing the panel is not enough to prove it is translated: it has to
// be made visible by real interaction first.
void ShowSearchEmptyState(lcPartSelectionWidget* Panel)
{
	QLineEdit* Filter = PartsFilter(Panel);

	if (Filter == nullptr)
		return;

	Filter->setText(QStringLiteral("zzzz-no-such-part"));
	QCoreApplication::processEvents();
}

void ClearSearch(lcPartSelectionWidget* Panel)
{
	QLineEdit* Filter = PartsFilter(Panel);

	if (Filter != nullptr)
		Filter->clear();

	QCoreApplication::processEvents();
}

}

int main(int argc, char** argv)
{
	lcApplication Application(argc, argv);

	QString OutDir = QStringLiteral("docs/evidence/member-c/2026-11-02/d7-host");

	if (argc > 1)
		OutDir = QString::fromLocal8Bit(argv[1]);

	QDir().mkpath(OutDir);

	QTranslator Translator;
	if (!Translator.load(QStringLiteral("leocad_zh_CN"), QStringLiteral(":/resources")) &&
		!Translator.load(QStringLiteral("resources/leocad_zh_CN.qm")))
	{
		std::fprintf(stderr, "could not load the zh_CN translation\n");
		return 1;
	}

	QCoreApplication::installTranslator(&Translator);

	if (!gApplication->mLibrary)
	{
		QList<QPair<QString, bool>> Paths;
		Paths += qMakePair(QStringLiteral("assets/ldraw-lite/library-lite.zip"), true);

		if (!gApplication->LoadPartsLibrary(Paths, true))
		{
			std::fprintf(stderr, "could not load the curated library\n");
			return 1;
		}
	}

	lcMainWindow Window;
	Window.CreateWidgets();
	gApplication->SetProject(new Project());

	// Build the shared GL resources before anything tries to draw. The real entry
	// point does this in lcApplication::Initialize(); a harness that constructs
	// the window itself skips that, and the first paint then dereferences the
	// grid texture that InitializeRenderer() would have created — a segfault in
	// lcContext::BindTexture2D, nowhere near the code under test.
	if (!lcContext::InitializeRenderer())
	{
		std::fprintf(stderr, "could not initialise the GL renderer\n");
		return 1;
	}

	lcPartSelectionWidget* Panel = Window.findChild<lcPartSelectionWidget*>();

	if (Panel == nullptr)
	{
		std::fprintf(stderr, "no parts panel\n");
		return 1;
	}

	// Two sizes: the narrow one is where the menu labels used to be elided, the
	// wide one is the reference for the same strings unconstrained.
	struct Size { const char* Name; int Width; int Height; };
	const Size Sizes[] = {
		{ "narrow-1280x800", 1280, 800 },
		{ "wide-1920x1080", 1920, 1080 },
	};

	for (const Size& S : Sizes)
	{
		Window.resize(S.Width, S.Height);
		Window.show();
		QCoreApplication::processEvents();
		QCoreApplication::processEvents();

		const QString Prefix = OutDir + QStringLiteral("/") + QString::fromLatin1(S.Name);

		Save(&Window, Prefix + QStringLiteral("-window.png"));

		// The menu has to be shown for its aboutToShow handler to run: that
		// handler is where the width is set, so a menu captured without it would
		// show the old elision.
		//
		// The menu is emitted rather than popped up. Real popups under Wayland are
		// xdg_popup surfaces, and opening the submenu as a nested popup makes the
		// compositor kill the connection ("xdg_popup was not created on the
		// topmost popup"). QWidget::grab() renders the same widget tree without a
		// surface, which is enough to see the text and measure its width.
		if (QToolButton* Button = OptionsButton(Panel))
		{
			QMenu* Menu = Button->menu();

			QMetaObject::invokeMethod(Menu, "aboutToShow", Qt::DirectConnection);
			QCoreApplication::processEvents();

			Save(Menu, Prefix + QStringLiteral("-options-menu.png"));

			// The filter submenu is the one the device showed as
			// "Regular...ression"; capture it on its own as well. Pick it by its
			// translated title rather than taking the first submenu: the first
			// one is Icons, and capturing that would quietly show the wrong menu.
			const QString FilterTitle = QCoreApplication::translate("lcPartSelectionWidget", "Filter");

			for (QAction* Action : Menu->actions())
			{
				if (Action->menu() == nullptr || Action->text() != FilterTitle)
					continue;

				QMetaObject::invokeMethod(Action->menu(), "aboutToShow", Qt::DirectConnection);
				QCoreApplication::processEvents();
				Save(Action->menu(), Prefix + QStringLiteral("-filter-submenu.png"));
				break;
			}
		}

		// Category tree with translated names.
		if (QTreeWidget* Tree = CategoryTree(Panel))
			Save(Tree, Prefix + QStringLiteral("-categories.png"));

		// Search that matches nothing: the translated empty state.
		ShowSearchEmptyState(Panel);
		Save(Panel, Prefix + QStringLiteral("-search-empty.png"));
		ClearSearch(Panel);
	}

	// The colour list, which is where "Light Grey" showed up in English. Found
	// through lcColorList rather than a dock objectName: the Colours dock is
	// created with a title but no name, and the list is the thing that matters.
	if (QWidget* Colors = Window.findChild<lcColorList*>())
		Save(Colors, OutDir + QStringLiteral("/colours.png"));

	// The selection colour button, which is where the device screenshot showed
	// "Light Grey" in English. Captured on its own because the colour list below
	// it is a grid of swatches with no labels: the button is the only place the
	// colour name is ever drawn.
	if (QToolButton* ColorButton = Window.findChild<QToolButton*>(QStringLiteral("SelectionColorButton")))
	{
		Save(ColorButton, OutDir + QStringLiteral("/colour-button.png"));

		// And with a name that only exists in the .ts, so the capture proves the
		// translation is live rather than the button happening to show a word that
		// is the same in both languages.
		const int Grey = lcGetColorIndex(7);

		if (Grey >= 0)
		{
			Window.SetColorIndex(Grey);
			QCoreApplication::processEvents();
			Save(ColorButton, OutDir + QStringLiteral("/colour-button-light-grey.png"));
			QTest::qWait(0);
		}
	}

	Save(&Window, OutDir + QStringLiteral("/window-final.png"));

	// Tear the GL resources down while the application object is still alive.
	//
	// Left to the C++ runtime, the static members of lcContext are destroyed
	// during exit handlers, after QApplication is gone, and ~QOffscreenSurface()
	// dereferences a current context that no longer exists. That segfault lands
	// after every file has been written, so it never lost a screenshot, but a
	// tool that crashes on success is one nobody trusts.
	//
	// The project is deliberately left alone: gApplication owns it, and clearing
	// it from here frees a document the window still points at.
	lcContext::ShutdownRenderer();

	return 0;
}
