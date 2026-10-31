#pragma once

#include "lc_math.h"

class Project;
class lcPiecesLibrary;
enum class lcViewSphereLocation;

enum class lcShadingMode
{
	Wireframe,
	Flat,
	DefaultLights,
	Full
};

enum class lcColorTheme
{
	Dark,
	System
};

enum class lcAxisIconLocation
{
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight
};

class lcPreferences
{
public:
	void LoadDefaults();
	void SaveDefaults();
	void SetInterfaceColors(lcColorTheme ColorTheme);

	int mMouseSensitivity;
	lcShadingMode mShadingMode;
	bool mBackgroundGradient;
	quint32 mBackgroundSolidColor;
	quint32 mBackgroundGradientColorTop;
	quint32 mBackgroundGradientColorBottom;
	bool mDrawAxes;
	lcAxisIconLocation mAxisIconLocation;
	quint32 mTextColor;
	quint32 mTextHaloColor;
	quint32 mMarqueeBorderColor;
	quint32 mMarqueeFillColor;
	quint32 mOverlayColor;
	quint32 mActiveViewColor;
	quint32 mInactiveViewColor;
	bool mDrawEdgeLines;
	bool mDrawConditionalLines;
	float mLineWidth;
	bool mAllowLOD;
	float mMeshLODDistance;
	bool mFadeSteps;
	quint32 mFadeStepsColor;
	bool mHighlightNewParts;
	quint32 mHighlightNewPartsColor;
	bool mGridEnabled = true;
	bool mDrawGridStuds;
	quint32 mGridStudColor;
	bool mDrawGridLines;
	int mGridLineSpacing;
	quint32 mGridLineColor;
	bool mDrawGridOrigin;
	bool mFixedAxes;
	bool mViewSphereEnabled;
	lcViewSphereLocation mViewSphereLocation;
	int mViewSphereSize;
	quint32 mViewSphereColor;
	quint32 mViewSphereTextColor;
	quint32 mViewSphereHighlightColor;
	bool mAutoLoadMostRecent;
	bool mRestoreTabLayout;
	lcColorTheme mColorTheme;
	quint32 mObjectSelectedColor;
	quint32 mObjectFocusedColor;
	quint32 mCameraColor;
	quint32 mLightColor;
	quint32 mControlPointColor;
	quint32 mControlPointFocusedColor;


	int mPreviewViewSphereEnabled;
	int mPreviewViewSphereSize;
	lcViewSphereLocation mPreviewViewSphereLocation;
	int mDrawPreviewAxis;

	bool mStudCylinderColorEnabled;
	quint32 mStudCylinderColor;
	bool mPartEdgeColorEnabled;
	quint32 mPartEdgeColor;
	bool mBlackEdgeColorEnabled;
	quint32 mBlackEdgeColor;
	bool mDarkEdgeColorEnabled;
	quint32 mDarkEdgeColor;
	float mPartEdgeContrast;
	float mPartColorValueLDIndex;
	bool  mAutomateEdgeColor;
};

struct lcCommandLineOptions
{
	bool ParseOK = true;
	bool Exit = false;
	bool SaveImage = false;
	bool SaveWavefront = false;
	bool Save3DS = false;
	bool SaveCOLLADA = false;
	bool SaveCSV = false;
	bool SaveHTML = false;
	bool SetCameraAngles = false;
	bool SetCameraPosition = false;
	bool Orthographic = false;
	bool SetFoV = false;
	bool SetZPlanes = false;
	bool SetFadeStepsColor = false;
	bool SetHighlightColor = false;
	bool FadeSteps = false;
	bool ImageHighlight = false;
	bool AutomateEdgeColor = false;
	int ImageWidth;
	int ImageHeight;
	int AASamples;
	lcShadingMode ShadingMode;
	float LineWidth;
	lcStudStyle StudStyle;
	lcStep ImageStart;
	lcStep ImageEnd;
	lcVector3 CameraPosition[3];
	lcVector2 CameraLatLon;
	float FoV;
	float PartEdgeContrast;
	float PartColorValueLDIndex;
	lcVector2 ZPlanes;
	lcViewpoint Viewpoint;
	bool StudCylinderColorEnabled;
	quint32 StudCylinderColor;
	bool PartEdgeColorEnabled;
	quint32 PartEdgeColor;
	bool BlackEdgeColorEnabled;
	quint32 BlackEdgeColor;
	bool DarkEdgeColorEnabled;
	quint32 DarkEdgeColor;
	quint32 FadeStepsColor;
	quint32	HighlightColor;
	QString ImageName;
	QString ModelName;
	QString CameraName;
	QString ProjectName;
	QString SaveWavefrontName;
	QString Save3DSName;
	QString SaveCOLLADAName;
	QString SaveCSVName;
	QString SaveHTMLName;
	QList<QPair<QString, bool>> LibraryPaths;
	QString StdOut;
	QString StdErr;
};

enum class lcStartupMode
{
	ShowWindow,
	Success,
	Error
};

class lcApplication : public QApplication
{
	Q_OBJECT

public:
	lcApplication(int& Argc, char** Argv);
	~lcApplication();

	lcApplication(const lcApplication&) = delete;
	lcApplication(lcApplication&&) = delete;
	lcApplication& operator=(const lcApplication&) = delete;
	lcApplication& operator=(lcApplication&&) = delete;

	void SetProject(Project* Project);
	static lcCommandLineOptions ParseCommandLineOptions();
	lcStartupMode Initialize(const QList<QPair<QString, bool>>& LibraryPaths);
	void Shutdown();
	void ShowPreferencesDialog();
	void SaveTabLayout() const;

	bool LoadPartsLibrary(const QList<QPair<QString, bool>>& LibraryPaths, bool OnlyUsePaths);

	void SetClipboard(const QByteArray& Clipboard);
	void ExportClipboard(const QByteArray& Clipboard);

	// Applies the colour theme to the application: style, palette and the dark
	// stylesheet. Public so a regression test can drive it on the real
	// application object; the theme decides menu and text colours, which is what
	// D5-02 is about, and it cannot be checked without applying it.
	void UpdateStyle();

	// Repairs text roles whose contrast against their own surface is too low.
	//
	// A platform style's standardPalette() is not guaranteed to be
	// self-consistent. The device evidence for #33 is a palette that applied
	// Window #ffffff while the menu and colour-name text stayed invisible, which
	// is the shape of a text role that does not differ from its surface. Static
	// and pure so the rule can be tested against synthetic palettes without a
	// device.
	static QPalette EnsureReadablePalette(const QPalette& Source);

	// Logs what the menu bar and the colour-name button actually resolve, so a
	// device can show where the theme stops reaching. Application-level palette
	// values were not enough to explain #33: the device reported readable
	// application colours while menu text stayed invisible.
	static void LogThemeWidgetState();

	// Test seam: when valid, the System branch uses this instead of the platform
	// style's standardPalette(). Lets the repair be checked through the real
	// UpdateStyle() rather than only in isolation. Never set in production.
	void SetStylePaletteOverride(const QPalette& Palette)
	{
		mStylePaletteOverride = Palette;
		mHasStylePaletteOverride = Palette.color(QPalette::Window).isValid();
	}

	// The style name UpdateStyle() restores for the System theme. Exposed for the
	// theme regression: the device case is precisely a platform where this value
	// came out empty, and a test has to be able to produce that state.
	QString GetDefaultStyleName() const
	{
		return mDefaultStyle;
	}

	void SetDefaultStyleName(const QString& Name)
	{
		mDefaultStyle = Name;
	}

	Project* mProject = nullptr;
	lcPiecesLibrary* mLibrary = nullptr;
	lcPreferences mPreferences;
	QByteArray mClipboard;

protected:
	QString GetTabLayoutKey() const;

	QString mDefaultStyle;
	QPalette mStylePaletteOverride;
	bool mHasStylePaletteOverride = false;
};

extern lcApplication* gApplication;

inline lcPiecesLibrary* lcGetPiecesLibrary()
{
	return gApplication->mLibrary;
}

inline Project* lcGetActiveProject()
{
	return gApplication->mProject;
}

inline lcPreferences& lcGetPreferences()
{
	return gApplication->mPreferences;
}
