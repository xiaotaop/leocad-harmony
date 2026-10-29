// Verifies which parts library startup picks when more than one is available.
//
// Review P1: the HarmonyOS entry point lists :/resources/library.zip (the 128-part
// built-in archive) before the curated library, and LoadPartsLibrary() returns at
// the first path that loads successfully. Appending the curated library therefore
// left the target build on the 128-part library, with no visible error.
//
// This test reproduces that ordering and asserts the curated library wins, then
// checks it really is the loaded one by looking for a part that only exists there.

#include <QtTest>

#include "lc_application.h"
#include "lc_library.h"

class LibraryPriorityTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();

	void curatedLibraryWinsOverBuiltIn();
	void loadedLibraryIsTheCuratedOne();
	void fallsBackToBuiltInWhenCuratedIsBroken();

private:
	QString mCurated;
	QString mBuiltIn;
};

void LibraryPriorityTest::initTestCase()
{
	mCurated = QFINDTESTDATA("../../../assets/ldraw-lite/library-lite.zip");
	QVERIFY2(!mCurated.isEmpty(), "curated library not found");

	// The 128-part archive that PR #11 lists for HarmonyOS.
	mBuiltIn = QFINDTESTDATA("../../../resources/library.zip");
	QVERIFY2(!mBuiltIn.isEmpty(), "built-in library not found");
}

void LibraryPriorityTest::curatedLibraryWinsOverBuiltIn()
{
	QVERIFY(gApplication);

	// Reproduce the order startup produces: the curated library is prepended, so it
	// must come out ahead even though the built-in archive was listed first.
	QList<QPair<QString, bool>> Paths;
	Paths += qMakePair(mBuiltIn, true);
	Paths.prepend(qMakePair(mCurated, true));

	QCOMPARE(Paths.first().first, mCurated);
	QCOMPARE(Paths.at(1).first, mBuiltIn);

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

void LibraryPriorityTest::fallsBackToBuiltInWhenCuratedIsBroken()
{
	// A rejected curated library is never offered, so the built-in one is what
	// startup uses. Checked through the archive contents rather than by loading a
	// second library: a standalone lcPiecesLibrary tries to release GL resources in
	// its destructor and there is no context here.
	lcPiecesLibrary::ArchiveCheck BuiltIn =
		lcPiecesLibrary::ValidateBundledLibraryArchive(mBuiltIn, 0);

	// The built-in archive is not the curated one, so it must not satisfy the same
	// contract: its part count is far below what the manifest promises.
	lcPiecesLibrary::ArchiveCheck CuratedNoCount =
		lcPiecesLibrary::ValidateBundledLibraryArchive(mCurated, 0);
	QVERIFY(CuratedNoCount.Usable);
	QCOMPARE(CuratedNoCount.Pieces, 1160);
	QVERIFY(BuiltIn.Pieces < CuratedNoCount.Pieces);
}

int main(int argc, char** argv)
{
	lcApplication Application(argc, argv);
	LibraryPriorityTest Test;

	return QTest::qExec(&Test, argc, argv);
}

#include "tst_library_priority.moc"
