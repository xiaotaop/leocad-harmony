// Verifies which parts library startup picks when more than one is available.
//
// Review P1: the HarmonyOS entry point lists :/resources/library.zip (the 128-part
// built-in archive) before the curated library, and LoadPartsLibrary() returns at
// the first path that loads successfully. Appending the curated library therefore
// left the target build on the 128-part library, with no visible error.
//
// The first version of this test rebuilt that list by hand and then asserted the
// order it had just built. It passed with the entry point appending again — the
// exact regression it was written to catch — because it never touched the code
// that does the ordering. These tests now call PlaceBundledLibraryArchive(), the
// same function startup calls, so changing append/prepend back fails here.
//
// Why this cannot be caught by running the desktop binary: :/resources/library.zip
// is registered under #ifdef Q_OS_HARMONY, so on desktop the curated library is
// the only entry and the ordering makes no difference. The bug is invisible
// outside the target build.

#include <QtTest>

#include "lc_application.h"
#include "lc_library.h"
#include "lc_zipfile.h"

class LibraryPriorityTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();

	void curatedLibraryWinsOverBuiltIn();
	void loadedLibraryIsTheCuratedOne();
	void rejectedArchiveLeavesListUnchanged();
	void missingArchiveLeavesListUnchanged();
	void curatedGoesAheadOfEveryPlatformEntry();
	void builtInArchivePieceCountIsPinned();

private:
	QString mCurated;
	QString mBuiltIn;

	// The list the HarmonyOS entry point has built by the time the curated
	// library is placed: the packaged built-in archive first, then the
	// platform's library.bin.
	QList<QPair<QString, bool>> PlatformPaths() const;
};

void LibraryPriorityTest::initTestCase()
{
	mCurated = QFINDTESTDATA("../../../assets/ldraw-lite/library-lite.zip");
	QVERIFY2(!mCurated.isEmpty(), "curated library not found");

	// The 128-part archive that PR #11 lists for HarmonyOS.
	mBuiltIn = QFINDTESTDATA("../../../resources/library.zip");
	QVERIFY2(!mBuiltIn.isEmpty(), "built-in library not found");
}

QList<QPair<QString, bool>> LibraryPriorityTest::PlatformPaths() const
{
	QList<QPair<QString, bool>> Paths;
	Paths += qMakePair(mBuiltIn, true);
	Paths += qMakePair(QStringLiteral("/platform/library.bin"), true);
	return Paths;
}

void LibraryPriorityTest::curatedLibraryWinsOverBuiltIn()
{
	QString Problem;
	const QList<QPair<QString, bool>> Paths = lcPiecesLibrary::PlaceBundledLibraryArchive(
		PlatformPaths(), mCurated, 1160, Problem);

	QVERIFY2(Problem.isEmpty(), qPrintable(Problem));

	// This is the assertion that fails if the placement appends instead of
	// prepending. It is checked on the real function, not a copy of it.
	QCOMPARE(Paths.first().first, mCurated);
	QCOMPARE(Paths.size(), 3);
	QCOMPARE(Paths.at(1).first, mBuiltIn);

	// And the library those paths select really is the curated one.
	QVERIFY(gApplication);
	QVERIFY2(gApplication->LoadPartsLibrary(Paths, true), "no library loaded at all");
	QCOMPARE(gApplication->mLibrary->GetPieceCount(), 1160);
	QCOMPARE(gApplication->mLibrary->mNumOfficialPieces, 1160);
}

void LibraryPriorityTest::loadedLibraryIsTheCuratedOne()
{
	QVERIFY(gApplication);

	// Counting alone would also pass if some other 1160-part archive were loaded, so
	// look the parts up by name. mPieces is keyed by the upper-cased file name, which
	// is what OpenArchive() stores.
	lcPiecesLibrary* Library = gApplication->mLibrary;
	QVERIFY(Library);

	const auto Has = [Library](const char* Name)
	{
		return Library->mPieces.find(Name) != Library->mPieces.end();
	};

	// 10172 is absent from resources/library.zip; 3001 exists in both, so together
	// they distinguish "curated subset" from "the 128-part archive".
	QVERIFY2(Has("10172.DAT"), "10172 missing: the loaded library is not the curated subset");
	QVERIFY2(Has("3001.DAT"), "3001 missing: this is not a valid LDraw library at all");
}

void LibraryPriorityTest::curatedGoesAheadOfEveryPlatformEntry()
{
	// A real LEOCAD_LIB or a compiled-in LC_LDRAW_LIBRARY_PATH is registered
	// before the curated library is placed. Prepending must put the packaged
	// library ahead of those too: a path baked into a build should not outrank
	// the library the package actually ships.
	QList<QPair<QString, bool>> Paths = PlatformPaths();
	Paths += qMakePair(QStringLiteral("/opt/lDraw"), false);

	QString Problem;
	const QList<QPair<QString, bool>> Placed =
		lcPiecesLibrary::PlaceBundledLibraryArchive(Paths, mCurated, 1160, Problem);

	QVERIFY2(Problem.isEmpty(), qPrintable(Problem));
	QCOMPARE(Placed.first().first, mCurated);
	QCOMPARE(Placed.size(), Paths.size() + 1);
	QCOMPARE(Placed.last().first, QStringLiteral("/opt/lDraw"));
}

void LibraryPriorityTest::rejectedArchiveLeavesListUnchanged()
{
	// A curated archive that fails validation must not be offered at all,
	// otherwise startup loads a broken library instead of falling through to the
	// platform paths.
	const QList<QPair<QString, bool>> Original = PlatformPaths();
	QString Problem;

	// Part count mismatch: the real archive does not have 9999 parts.
	const QList<QPair<QString, bool>> Placed =
		lcPiecesLibrary::PlaceBundledLibraryArchive(Original, mCurated, 9999, Problem);

	QVERIFY2(!Problem.isEmpty(), "a wrong expected count must be reported");
	QCOMPARE(Placed, Original);
}

void LibraryPriorityTest::missingArchiveLeavesListUnchanged()
{
	const QList<QPair<QString, bool>> Original = PlatformPaths();
	QString Problem;

	const QList<QPair<QString, bool>> Placed =
		lcPiecesLibrary::PlaceBundledLibraryArchive(Original, QString(), 1160, Problem);

	QCOMPARE(Problem, QStringLiteral("archive not found"));
	QCOMPARE(Placed, Original);
}

void LibraryPriorityTest::builtInArchivePieceCountIsPinned()
{
	// The docs disagreed about this number (128 vs 129) because nobody had
	// measured it: the validator's count was only ever asserted against the
	// curated archive. Pin it so the prose cannot drift again.
	//
	// Counted from the archive itself rather than through
	// ValidateBundledLibraryArchive(): that function applies the curated
	// library's contract, which requires ldraw/ldconfig.ldr. The built-in
	// archive has none, so the validator reports it unusable — correct for
	// deciding whether to ship a curated replacement, wrong as a measure of how
	// many parts it holds. LoadColors() falls back to lcLoadDefaultColors(), so
	// the missing file costs the built-in archive its exact palette, not its
	// ability to load.
	lcZipFile Zip;
	QVERIFY2(Zip.OpenRead(mBuiltIn), "built-in archive is not a readable zip");

	int TopLevelParts = 0;
	int Primitives = 0;

	for (const lcZipFileInfo& Info : Zip.mFiles)
	{
		QString Name = QString::fromStdString(Info.file_name).toUpper();

		if (!Name.startsWith(QLatin1String("LDRAW/")) || !Name.endsWith(QLatin1String(".DAT")))
			continue;

		const QString Stem = Name.mid(6, Name.size() - 10);	// drop "LDRAW/" and ".DAT"

		if (Stem.startsWith(QLatin1String("PARTS/")))
		{
			const QString PartName = Stem.mid(6);

			if (PartName.startsWith(QLatin1String("S/")) || PartName.contains(QLatin1Char('/')))
				Primitives++;
			else
				TopLevelParts++;
		}
		else if (Stem.startsWith(QLatin1String("P/")))
			Primitives++;
	}

	// 128 top-level parts under LDRAW/PARTS/, 8 primitives under LDRAW/P/.
	QCOMPARE(TopLevelParts, 128);
	QCOMPARE(Primitives, 8);
	QVERIFY(TopLevelParts < 1160);
}

int main(int argc, char** argv)
{
	lcApplication Application(argc, argv);
	LibraryPriorityTest Test;

	return QTest::qExec(&Test, argc, argv);
}

#include "tst_library_priority.moc"
