#pragma once

#include "lc_context.h"
#include "lc_math.h"
#include "lc_meshloader.h"

class PieceInfo;
class lcTrainTrackInfo;
struct lcTrainTrackConnectionType;
class lcZipFile;
class lcLibraryMeshData;
class lcThumbnailManager;

enum class lcStudStyle
{
	Plain,
	ThinLinesLogo,
	OutlineLogo,
	SharpTopLogo,
	RoundedTopLogo,
	FlattenedLogo,
	HighContrast,
	HighContrastLogo,
	Count
};

constexpr bool lcIsHighContrast(lcStudStyle StudStyle)
{
	return StudStyle == lcStudStyle::HighContrast || StudStyle == lcStudStyle::HighContrastLogo;
}

enum class lcZipFileType
{
	Official,
	Unofficial,
	StudStyle,
	Count
};

enum class lcLibraryFolderType
{
	Official,
	Unofficial,
	Count
};

enum class lcPrimitiveState
{
	NotLoaded,
	Loading,
	Loaded
};

class lcLibraryPrimitive
{
public:
	explicit lcLibraryPrimitive(QString&& FileName, const char* Name, lcZipFileType ZipFileType, quint32 ZipFileIndex, bool Stud, bool StudStyle, bool SubFile)
		: mFileName(std::move(FileName))
	{
		strncpy(mName, Name, sizeof(mName)-1);
		mName[sizeof(mName) - 1] = 0;

		mZipFileType = ZipFileType;
		mZipFileIndex = ZipFileIndex;
		mState = lcPrimitiveState::NotLoaded;
		mStud = Stud;
		mStudStyle = StudStyle;
		mSubFile = SubFile;
	}

	void SetZipFile(lcZipFileType ZipFileType, quint32 ZipFileIndex)
	{
		mZipFileType = ZipFileType;
		mZipFileIndex = ZipFileIndex;
	}

	void Unload()
	{
		mState = lcPrimitiveState::NotLoaded;
		mMeshData.Clear();
	}

	QString mFileName;
	char mName[LC_MAXNAME];
	lcZipFileType mZipFileType;
	quint32 mZipFileIndex;
	lcPrimitiveState mState;
	bool mStud;
	bool mStudStyle;
	bool mSubFile;
	lcLibraryMeshData mMeshData;
};

enum class lcLibrarySourceType
{
	Library,
	StudStyle
};

struct lcLibrarySource
{
	lcLibrarySource() = default;

	~lcLibrarySource()
	{
		for (const auto& PrimitiveIt : Primitives)
			delete PrimitiveIt.second;
	}

	lcLibrarySource(const lcLibrarySource&) = delete;
	lcLibrarySource(lcLibrarySource&&) = delete;
	lcLibrarySource& operator=(const lcLibrarySource&) = delete;
	lcLibrarySource& operator=(lcLibrarySource&&) = delete;

	lcLibrarySourceType Type;
	std::map<std::string, lcLibraryPrimitive*> Primitives;
};

class lcPiecesLibrary : public QObject
{
	Q_OBJECT

public:
	lcPiecesLibrary();
	~lcPiecesLibrary();

	lcPiecesLibrary(const lcPiecesLibrary&) = delete;
	lcPiecesLibrary(lcPiecesLibrary&&) = delete;
	lcPiecesLibrary& operator=(const lcPiecesLibrary&) = delete;
	lcPiecesLibrary& operator=(lcPiecesLibrary&&) = delete;

	lcThumbnailManager* GetThumbnailManager() const
	{
		return mThumbnailManager.get();
	}

	bool Load(const QString& LibraryPath, bool ShowProgress);

	// Where the curated parts library shipped inside the application package
	// can be found, in priority order. The caller only needs one path it can
	// hand to Load(); GetBundledLibraryArchiveCandidates() is exposed so a
	// diagnostic can report why nothing was picked up instead of guessing.
	static QString FindBundledLibraryArchive();
	static QStringList GetBundledLibraryArchiveCandidates();

	// Checks that a parts archive is actually usable. Load() is not enough:
	// OpenArchive() skips entries whose names do not start with "ldraw/" and
	// still returns true, so a mis-rooted archive loads "successfully" with zero
	// parts and no error anywhere. This reports what is wrong instead, and the
	// caller decides whether to fall back.
	//
	// ExpectedParts is the count the shipped manifest promises; zero skips the
	// count check.
	struct ArchiveCheck
	{
		bool Usable = false;
		QString Problem;
		int Pieces = 0;
	};

	static ArchiveCheck ValidateBundledLibraryArchive(const QString& FileName, int ExpectedParts);

	// Decides where the curated library goes in the list LoadPartsLibrary()
	// walks, given the entries the platform layer already registered.
	//
	// This lives here, not in the entry point, so the ordering startup really
	// produces can be tested. The first version of the regression test rebuilt
	// the list itself and then asserted the order it had just built, which
	// passed even with the entry point appending instead of prepending — the
	// bug it was written to catch. Anything that matters must be reachable from
	// a test without copying it.
	//
	// Prepending is the whole point: LoadPartsLibrary() stops at the first path
	// that loads successfully, and the platform registers its own archive
	// (:/resources/library.zip, 128 parts) before this runs. Appending leaves
	// the target build on the small archive with no error anywhere.
	//
	// A curated archive that fails validation is never offered; the list comes
	// back unchanged so startup falls through to the platform paths.
	//
	// Returns the rejection reason in Problem when the archive was refused,
	// so the entry point can report it without re-running the checks.
	static QList<QPair<QString, bool>> PlaceBundledLibraryArchive(
		const QList<QPair<QString, bool>>& PlatformPaths, const QString& FileName,
		int ExpectedParts, QString& Problem);

	int GetPieceCount() const
	{
		return (int)mPieces.size();
	}

	// Why the packaged archive was refused, in the validator's wording; empty
	// when it loaded or when none was found.
	//
	// Static because it describes the package, not a loaded instance: the check
	// runs in the entry point before any lcPiecesLibrary exists. This is a
	// diagnostic. It says why the bundled archive was skipped, which is not the
	// same thing as the library the process ended up using — LoadPartsLibrary()
	// tries LEOCAD_LIB and the parts-library preference first and returns on
	// success without ever reaching the bundled archive.
	static void SetBundledLibraryProblem(const QString& Problem)
	{
		mBundledLibraryProblem = Problem;
	}

	static const QString& GetBundledLibraryProblem()
	{
		return mBundledLibraryProblem;
	}

	// Top-level part count the shipped archive promises, from
	// assets/ldraw-lite/manifest.json (counts.parts). Used both to validate the
	// archive at startup and to decide whether the library that finally loaded
	// is worse than that — so the two judgements cannot drift apart.
	static constexpr int BundledLibraryParts = 1160;

	// Whether the library this process actually loaded is worse than the one
	// the package ships, which is the only condition under which the user should
	// be told anything.
	//
	// The first version of the banner asked only whether the bundled archive had
	// been refused. That is wrong whenever an external library wins: with a
	// corrupt bundled archive and a valid LEOCAD_LIB, the load succeeds with the
	// full curated library and the panel still claimed only a few parts were
	// available. Reported in review, with a regression.
	//
	// ExpectedParts is what the package promises (the manifest count). A library
	// with at least that many pieces is not degraded, whatever route produced
	// it: a user pointing LEOCAD_LIB at a complete LDraw library has more parts
	// than the package, not fewer.
	static bool IsLibraryDegraded(int LoadedPieces, int ExpectedParts)
	{
		return LoadedPieces < ExpectedParts;
	}

	void LoadColors();
	void Unload();
	void RemoveTemporaryPieces();
	void RemovePiece(PieceInfo* Info);

	void RenamePiece(PieceInfo* Info, const char* NewName);
	PieceInfo* FindPiece(const char* PieceName, Project* Project, bool CreatePlaceholder, bool SearchProjectFolder);
	void LoadPieceInfo(PieceInfo* Info, bool Wait, bool Priority);
	void ReleasePieceInfo(PieceInfo* Info);
	bool LoadBuiltinPieces();
	bool LoadPieceData(PieceInfo* Info);
	void LoadQueuedPiece();
	void WaitForLoadQueue();

	lcTexture* FindTexture(const char* TextureName, Project* CurrentProject, bool SearchProjectFolder);
	bool LoadTexture(lcTexture* Texture);
	void ReleaseTexture(lcTexture* Texture);

	bool PieceInCategory(PieceInfo* Info, const char* CategoryKeywords) const;
	void GetCategoryEntries(int CategoryIndex, bool GroupPieces, std::vector<PieceInfo*>& SinglePieces, std::vector<PieceInfo*>& GroupedPieces);
	void GetCategoryEntries(const char* CategoryKeywords, bool GroupPieces, std::vector<PieceInfo*>& SinglePieces, std::vector<PieceInfo*>& GroupedPieces);
	void GetParts(std::vector<PieceInfo*>& Parts) const;
	std::vector<PieceInfo*> GetVisibleTrainTrackParts(const lcTrainTrackConnectionType& ConnectionType) const;

	std::vector<PieceInfo*> GetPartsFromSet(const std::vector<std::string>& PartIds) const;
	std::string GetPartId(const PieceInfo* Info) const;

	void GetPrimitiveFile(lcLibraryPrimitive* Primitive, std::function<void(lcFile& File)> Callback);
	void GetPieceFile(const char* FileName, std::function<void(lcFile& File)> Callback);

	bool IsPrimitive(const char* Name) const;
	lcLibraryPrimitive* FindPrimitive(const char* Name) const;
	bool LoadPrimitive(lcLibraryPrimitive* Primitive);

	bool SupportsStudStyle() const;
	void SetStudStyle(lcStudStyle StudStyle, bool Reload, bool StudCylinderColorEnabled);

	lcStudStyle GetStudStyle() const
	{
		return mStudStyle;
	}

	void SetOfficialPieces()
	{
		if (mZipFiles[static_cast<int>(lcZipFileType::Official)])
			mNumOfficialPieces = (int)mPieces.size();
	}

	bool ShouldCancelLoading() const
	{
		return mCancelLoading;
	}

	void UpdateBuffers(lcContext* Context);
	void UnloadUnusedParts();

	std::map<std::string, PieceInfo*> mPieces;
	int mNumOfficialPieces;

	std::vector<lcTexture*> mTextures;

	QDir mLibraryDir;

	bool mBuffersDirty;
	lcVertexBuffer mVertexBuffer;
	lcIndexBuffer mIndexBuffer;

signals:
	void PartLoaded(PieceInfo* Info);
	void ColorsLoaded();

protected:
	bool OpenArchive(const QString& FileName, lcZipFileType ZipFileType);
	bool OpenArchive(std::unique_ptr<lcFile> File, lcZipFileType ZipFileType);
	bool OpenDirectory(const QDir& LibraryDir, bool ShowProgress);
	void ReadArchiveDescriptions(const QString& OfficialFileName, const QString& UnofficialFileName);
	void ReadDirectoryDescriptions(const QFileInfoList (&FileLists)[static_cast<int>(lcLibraryFolderType::Count)], bool ShowProgress);

	bool ReadArchiveCacheFile(const QString& FileName, lcMemFile& CacheFile);
	bool WriteArchiveCacheFile(const QString& FileName, lcMemFile& CacheFile);
	bool LoadCacheIndex(const QString& FileName);
	bool SaveArchiveCacheIndex(const QString& FileName);
	bool LoadCachePiece(PieceInfo* Info);
	bool SaveCachePiece(PieceInfo* Info);
	bool ReadDirectoryCacheFile(const QString& FileName, lcMemFile& CacheFile);
	bool WriteDirectoryCacheFile(const QString& FileName, lcMemFile& CacheFile);

	static bool IsStudPrimitive(const char* FileName);
	static bool IsStudStylePrimitive(const char* FileName);
	void UpdateStudStyleSource();

	void ReleaseBuffers();

	std::vector<std::unique_ptr<lcLibrarySource>> mSources;

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	QRecursiveMutex mLoadMutex;
#else
	QMutex mLoadMutex;
#endif
	QList<QFuture<void>> mLoadFutures;
	QList<PieceInfo*> mLoadQueue;

	QMutex mTextureMutex;

	lcStudStyle mStudStyle;
	bool mStudCylinderColorEnabled;

	std::unique_ptr<lcThumbnailManager> mThumbnailManager;
	QString mCachePath;
	qint64 mArchiveCheckSum[4];
	std::unique_ptr<lcZipFile> mZipFiles[static_cast<int>(lcZipFileType::Count)];
	bool mHasUnofficial;
	bool mCancelLoading;

	static QString mBundledLibraryProblem;
};
