// D10 performance baseline probe (member C, 2026-11-05).
//
// Measures on the host desktop build what startup and the parts panel actually
// cost, and records the spread rather than a summary number: the whole point of
// this probe is that host timings are noisy, so hiding the spread would invite
// exactly the wrong conclusion.
//
// This is a probe, not a test. Nothing here asserts a timing; the numbers are
// machine-dependent and the evidence file is where they are interpreted.
//
// Modes (argv[1]):
//   core   parts library load, window construction, per-keystroke filter cost,
//          and a synthetic scaling curve for the filter. No GL, no show().
//   show   the startup the real binary performs, in stages, including the
//          offscreen GL context and show().
//
// See perf/README.md for how this file is put in place, built and run.

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLineEdit>
#include <QListView>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>

#include <algorithm>
#include <cstring>
#include <vector>

#include "lc_application.h"
#include "lc_context.h"
#include "pieceinf.h"
#include "project.h"
#include "lc_library.h"
#include "lc_mainwindow.h"
#include "lc_partselectionwidget.h"
#include "lc_shortcuts.h"

namespace
{

QTextStream& Out()
{
	static QTextStream Stream(stdout);
	return Stream;
}

QTextStream& Err()
{
	static QTextStream Stream(stderr);
	return Stream;
}

struct Samples
{
	QVector<double> Values;

	void Add(double Ms)
	{
		Values.append(Ms);
	}

	double Min() const
	{
		if (Values.isEmpty())
			return 0.0;
		return *std::min_element(Values.begin(), Values.end());
	}

	double Max() const
	{
		if (Values.isEmpty())
			return 0.0;
		return *std::max_element(Values.begin(), Values.end());
	}

	double Median() const
	{
		if (Values.isEmpty())
			return 0.0;
		QVector<double> Sorted = Values;
		std::sort(Sorted.begin(), Sorted.end());
		return Sorted[Sorted.size() / 2];
	}

	double Mean() const
	{
		if (Values.isEmpty())
			return 0.0;
		double Total = 0.0;
		for (double V : Values)
			Total += V;
		return Total / Values.size();
	}

	void Print(const char* Key) const
	{
		Out() << Key << "_min=" << QString::number(Min(), 'f', 3) << '\n';
		Out() << Key << "_median=" << QString::number(Median(), 'f', 3) << '\n';
		Out() << Key << "_mean=" << QString::number(Mean(), 'f', 3) << '\n';
		Out() << Key << "_max=" << QString::number(Max(), 'f', 3) << '\n';
		Out() << Key << "_n=" << Values.size() << '\n';
		Out().flush();
	}
};

double NowMs(const QElapsedTimer& Timer)
{
	return Timer.nsecsElapsed() / 1.0e6;
}

void PrintHeader(const QString& Mode, const QString& Archive)
{
	Out() << "# D10 perf probe, mode=" << Mode << '\n';
	Out() << "# qt=" << QT_VERSION_STR << " platform=" << QGuiApplication::platformName() << '\n';
	Out() << "# archive=" << Archive << '\n';
	Out() << "# archive_bytes=" << QFileInfo(Archive).size() << '\n';
	Out() << "# cache=" << QStandardPaths::writableLocation(QStandardPaths::CacheLocation) << '\n';
	Out() << "# ideal_threads=" << QThread::idealThreadCount() << '\n';
	Out().flush();
}

QString ArchivePath()
{
	return QString::fromUtf8(qgetenv("LEOCAD_PERF_ARCHIVE"));
}

lcPartSelectionWidget* FindPanel(lcMainWindow* Window)
{
	return Window->findChild<lcPartSelectionWidget*>();
}

QLineEdit* FindFilter(lcPartSelectionWidget* Panel)
{
	QLineEdit* Filter = Panel->findChild<QLineEdit*>(QStringLiteral("PartsFilter"));
	return Filter ? Filter : Panel->findChild<QLineEdit*>();
}

lcPartSelectionListView* FindPartsView(lcPartSelectionWidget* Panel)
{
	return Panel ? Panel->findChild<lcPartSelectionListView*>() : nullptr;
}

// The keystroke path the user actually drives: textChanged -> FilterChanged ->
// SetFilter (O(n) setRowHidden per row) -> VisibleRowsChanged -> RefreshStatus,
// which walks every row again to recount. Timing SetFilter() on its own would
// report half of what a keystroke costs.
double TimeKeystroke(QLineEdit* Filter, const QString& Text)
{
	QElapsedTimer Timer;
	Timer.start();
	Filter->setText(Text);
	return NowMs(Timer);
}

// Synthetic scaling curve. This changes nothing in the product: it points the
// same lcPartSelectionListModel::SetFilter() at wider and wider mParts vectors
// so the cost is visible as a function of library size — the question the
// shipped 1160-part subset cannot answer on its own, and the one behind the
// "known limit" recorded in the release notes.
void ReportScalingCurve(lcPiecesLibrary* Library, lcPartSelectionListView* View)
{
	if (!Library || !View)
	{
		Out() << "# scaling: no parts view, curve skipped\n";
		Out().flush();
		return;
	}

	lcPartSelectionListModel* Model = View->GetListModel();

	std::vector<PieceInfo*> AllParts;
	Library->GetParts(AllParts);

	if (AllParts.empty())
	{
		Out() << "# scaling: library empty, curve skipped\n";
		Out().flush();
		return;
	}

	const int Sizes[] = { 1160, 5000, 20000, 50000, 100000 };
	const QStringList Steps = { QStringLiteral("b"), QStringLiteral("bri"), QStringLiteral("brick") };

	Out() << "# scaling: synthetic widths over the loaded library's parts.\n";
	Out() << "# scaling: rows repeat beyond the real count on purpose; only the\n";
	Out() << "# scaling: per-row cost is being measured, not the contents.\n";

	for (int Size : Sizes)
	{
		std::vector<std::pair<PieceInfo*, std::string>> Parts;
		Parts.reserve((size_t)Size);

		for (int Index = 0; Index < Size; Index++)
			Parts.emplace_back(AllParts[(size_t)Index % AllParts.size()], std::string());

		QElapsedTimer BuildTimer;
		BuildTimer.start();
		// SetCustomParts() rebuilds mParts and then re-runs the current filter.
		Model->SetCustomParts(Parts, 0);
		Out() << "scaling_build=" << Size << ',' << QString::number(NowMs(BuildTimer), 'f', 3) << '\n';

		for (const QString& Step : Steps)
		{
			QElapsedTimer Timer;
			Timer.start();
			Model->SetFilter(Step);
			Out() << "scaling_row=" << Size << ',' << Step << ',' << QString::number(NowMs(Timer), 'f', 3) << '\n';
		}

		Out().flush();
	}

	// Put the model back onto a real category so nothing downstream measures a
	// synthetic list by accident.
	View->SetCategory(lcPartCategoryType::AllParts, 0);
	Out().flush();
}

QList<QPair<QString, bool>> OnePath(const QString& Archive)
{
	QList<QPair<QString, bool>> Paths;
	Paths += qMakePair(Archive, true);
	return Paths;
}

int RunCore(lcApplication& Application, const QString& Archive)
{
	QElapsedTimer Whole;
	Whole.start();

	PrintHeader(QStringLiteral("core"), Archive);

	Samples LibraryWarm, Keystroke;

	// --- cold library load: this process's first load, into an empty cache dir
	{
		QElapsedTimer Timer;
		Timer.start();
		const bool Loaded = Application.LoadPartsLibrary(OnePath(Archive), true);
		const double Ms = NowMs(Timer);

		Out() << "library_cold_loaded=" << (Loaded ? 1 : 0) << '\n';
		Out() << "library_cold_pieces=" << (Application.mLibrary ? Application.mLibrary->GetPieceCount() : 0) << '\n';
		Out() << "library_cold_ms=" << QString::number(Ms, 'f', 3) << '\n';
		Out().flush();
	}

	if (!Application.mLibrary || Application.mLibrary->GetPieceCount() == 0)
	{
		Err() << "the parts library did not load; nothing to measure\n";
		return 1;
	}

	// --- warm loads: same process, archive already parsed once and the
	// description index already written to the cache directory
	for (int Round = 0; Round < 5; Round++)
	{
		QElapsedTimer Timer;
		Timer.start();
		Application.LoadPartsLibrary(OnePath(Archive), true);
		const double Ms = NowMs(Timer);
		Out() << "library_warm_ms=" << QString::number(Ms, 'f', 3) << '\n';
		LibraryWarm.Add(Ms);
	}

	// --- window construction, never shown
	{
		QElapsedTimer Timer;
		Timer.start();
		gMainWindow = new lcMainWindow();
		lcLoadDefaultKeyboardShortcuts();
		lcLoadDefaultMouseShortcuts();
		const double Ms = NowMs(Timer);
		Out() << "window_ms=" << QString::number(Ms, 'f', 3) << '\n';

		Timer.restart();
		gMainWindow->CreateWidgets();
		Out() << "createwidgets_ms=" << QString::number(NowMs(Timer), 'f', 3) << '\n';

		Timer.restart();
		Application.SetProject(new Project());
		Out() << "project_ms=" << QString::number(NowMs(Timer), 'f', 3) << '\n';

		Timer.restart();
		gMainWindow->GetPartSelectionWidget()->SetDefaultPart();
		Out() << "setdefaultpart_ms=" << QString::number(NowMs(Timer), 'f', 3) << '\n';
		Out().flush();
	}

	// --- per-keystroke cost on the shipped library
	{
		lcPartSelectionWidget* Panel = FindPanel(gMainWindow);
		QLineEdit* Filter = Panel ? FindFilter(Panel) : nullptr;

		if (!Filter)
		{
			Err() << "no filter box in the parts panel\n";
			return 1;
		}

		// The panel starts on the Brick category (Initialize() calls
		// SetDefaultPart()), which is a small fraction of the library. Measuring
		// keystrokes there would understate the cost by an order of magnitude and
		// the resulting number would not describe "a 1160-part library" at all.
		// Switch to All Parts first, and report the row count actually filtered so
		// the number cannot be read against the wrong denominator.
		Panel->SetCategory(lcPartCategoryType::AllParts, 0);

		QAbstractItemModel* Model = FindPartsView(Panel) ? FindPartsView(Panel)->model() : nullptr;
		Out() << "keystroke_rows=" << (Model ? Model->rowCount() : -1) << '\n';

		const QStringList Sequence = { QStringLiteral("b"), QStringLiteral("br"), QStringLiteral("bri"),
			QStringLiteral("bric"), QStringLiteral("brick"), QString() };

		// One warm-up pass so the first sample does not pay for lazy allocation.
		for (const QString& Step : Sequence)
			TimeKeystroke(Filter, Step);

		Out() << "# keystroke: textChanged on PartsFilter, end to end\n";
		for (int Round = 0; Round < 5; Round++)
		{
			for (const QString& Step : Sequence)
			{
				const double Ms = TimeKeystroke(Filter, Step);
				Out() << "keystroke_ms=" << QString::number(Ms, 'f', 3)
					<< " step=" << (Step.isEmpty() ? QStringLiteral("(clear)") : Step) << '\n';
				Keystroke.Add(Ms);
			}
		}

		Out().flush();
		Filter->clear();
	}

	// --- scaling curve: the known limit when the library grows
	ReportScalingCurve(Application.mLibrary, FindPartsView(FindPanel(gMainWindow)));

	Out() << "# summary\n";
	LibraryWarm.Print("library_warm");
	Keystroke.Print("keystroke");
	Out() << "whole_core_ms=" << QString::number(NowMs(Whole), 'f', 3) << '\n';
	Out().flush();

	return 0;
}

int RunShow(lcApplication& Application, const QString& Archive)
{
	QElapsedTimer Whole;
	Whole.start();

	PrintHeader(QStringLiteral("show"), Archive);

	QElapsedTimer Timer;

	// The same sequence lcApplication::Initialize() performs, timed per stage, so
	// the stage that dominates is measured instead of guessed.

	Timer.start();
	const bool RendererOK = lcContext::InitializeRenderer();
	Out() << "renderer_init_ms=" << QString::number(NowMs(Timer), 'f', 3) << '\n';
	Out() << "renderer_ok=" << (RendererOK ? 1 : 0) << '\n';
	Out().flush();

	Timer.restart();
	Application.UpdateStyle();
	Out() << "style_ms=" << QString::number(NowMs(Timer), 'f', 3) << '\n';

	Timer.restart();
	gMainWindow = new lcMainWindow();
	lcLoadDefaultKeyboardShortcuts();
	lcLoadDefaultMouseShortcuts();
	Out() << "window_ms=" << QString::number(NowMs(Timer), 'f', 3) << '\n';

	Timer.restart();
	const bool Loaded = Application.LoadPartsLibrary(OnePath(Archive), true);
	Out() << "library_ms=" << QString::number(NowMs(Timer), 'f', 3) << '\n';
	Out() << "library_loaded=" << (Loaded ? 1 : 0) << '\n';
	Out() << "library_pieces=" << (Application.mLibrary ? Application.mLibrary->GetPieceCount() : 0) << '\n';

	Timer.restart();
	gMainWindow->CreateWidgets();
	Out() << "createwidgets_ms=" << QString::number(NowMs(Timer), 'f', 3) << '\n';

	Timer.restart();
	Application.SetProject(new Project());
	Out() << "project_ms=" << QString::number(NowMs(Timer), 'f', 3) << '\n';

	Timer.restart();
	gMainWindow->GetPartSelectionWidget()->SetDefaultPart();
	Out() << "setdefaultpart_ms=" << QString::number(NowMs(Timer), 'f', 3) << '\n';

	Out() << "startup_before_show_ms=" << QString::number(NowMs(Whole), 'f', 3) << '\n';
	Out().flush();

	// show() is the stage the offscreen platform distorts most: there is no real
	// GPU presentation, so this says close to nothing about first frame on a
	// device. It is recorded to show how wide the spread is, not as a target.
	for (int Round = 0; Round < 5; Round++)
	{
		gMainWindow->hide();
		QApplication::processEvents();

		Timer.restart();
		gMainWindow->show();
		QApplication::processEvents();
		Out() << "show_stage_ms=" << QString::number(NowMs(Timer), 'f', 3) << '\n';
		Out().flush();
	}

	Out() << "startup_with_show_ms=" << QString::number(NowMs(Whole), 'f', 3) << '\n';
	Out().flush();

	gMainWindow->hide();

	// Deliberately no ShutdownRenderer() here. lcApplication::Shutdown() already
	// releases the renderer, and it does so after deleting the window, project
	// and library — the only order that works, because the library destructor
	// releases its GL buffers through the global offscreen context. Tearing the
	// renderer down first makes that destructor dereference a null context.

	return 0;
}

} // namespace

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		Err() << "usage: " << (argc > 0 ? argv[0] : "leocad-perf") << " core|show\n";
		return 2;
	}

	const QString Mode = QString::fromUtf8(argv[1]);
	const QString Archive = ArchivePath();

	if (Archive.isEmpty())
	{
		Err() << "LEOCAD_PERF_ARCHIVE is not set\n";
		return 2;
	}

	if (!QFileInfo::exists(Archive))
	{
		Err() << "parts archive not found: " << Archive << '\n';
		return 2;
	}

	// The identity the real entry point sets before anything else; QSettings and
	// QStandardPaths both key off it, so the cache directory would otherwise
	// differ from the application's.
	QCoreApplication::setOrganizationDomain(QStringLiteral("leocad.org"));
	QCoreApplication::setOrganizationName(QStringLiteral("LeoCAD Software"));
	QCoreApplication::setApplicationName(QStringLiteral("LeoCAD"));

	lcApplication Application(argc, argv);

	// The real entry point registers these before any window exists. PieceInfo
	// has to be a complete type here, which is what pieceinf.h above is for:
	// Qt 6 refuses to register a pointer to an opaque type.
	qRegisterMetaType<PieceInfo*>("PieceInfo*");
	qRegisterMetaType<QList<int>>("QList<int>");
	qRegisterMetaType<lcVector3>("lcVector3");

	int Result = 0;

	if (Mode == QStringLiteral("core"))
	{
		Result = RunCore(Application, Archive);

		// The library destructor releases its GL buffers through the global
		// offscreen context, so it must not run before one exists. core mode
		// never creates one (measuring without GL is the point) and Shutdown()
		// would then dereference a null context: a crash after every number had
		// already been printed. A probe that segfaults on exit looks unreliable
		// and hides which line was the last good one.
		//
		// The context is created for teardown only; every number above is read
		// before this point and none of them include it.
		if (lcContext::GetGlobalOffscreenContext() == nullptr)
			lcContext::InitializeRenderer();
	}
	else if (Mode == QStringLiteral("show"))
		Result = RunShow(Application, Archive);
	else
	{
		Err() << "unknown mode: " << Mode << '\n';
		return 2;
	}

	Application.Shutdown();

	return Result;
}
