#pragma once

#include "lc_math.h"

class lcFile;

#define LC_MAX_COLOR_NAME 64
#define LC_COLOR_DIRECT 0x80000000
#define LC_COLOR_NOCOLOR 0xffffffff
#define LC_STUD_CYLINDER_COLOR_CODE 4242

struct lcColor
{
	quint32 Code;
	int Group;
	bool Translucent = false;
	bool Chrome = false;
	bool Rubber = false;
	bool Adjusted = false;
	lcVector4 Value;
	lcVector4 Edge;
	char Name[LC_MAX_COLOR_NAME];
	char SafeName[LC_MAX_COLOR_NAME];
};

enum
{
	LC_COLORGROUP_SOLID,
	LC_COLORGROUP_TRANSLUCENT,
	LC_COLORGROUP_SPECIAL,
	LC_NUM_COLORGROUPS
};

struct lcColorGroup
{
	std::vector<int> Colors;
	QString Name;
};

extern std::vector<lcColor> gColorList;
extern lcColorGroup gColorGroups[LC_NUM_COLORGROUPS];
extern int gEdgeColor;
extern int gDefaultColor;

void lcLoadDefaultColors(lcStudStyle StudStyle);
bool lcLoadColorFile(lcFile& File, lcStudStyle StudStyle);

// Parses colour definitions out of a file without touching global state.
//
// lcLoadColorFile() cannot be used to validate a file before loading it: it writes
// gColorList, gColorGroups, gDefaultColor and gEdgeColor, and it calls
// lcAdjustStudStyleColors() which reads the preferences. That makes it destructive
// and order-dependent — running it to "check" a broken archive would corrupt the
// colour table the application then keeps using.
//
// This returns only the records that actually parsed into a usable colour: a line
// must yield both a CODE and a VALUE, which is the same test the loader applies
// before accepting a record.
std::vector<lcColor> lcParseColorFile(lcFile& File);
int lcGetColorIndex(quint32 ColorCode);

inline quint32 lcGetColorCodeFromExtendedColor(int Color)
{
	const quint32 ConversionTable[] = { 4, 12, 2, 10, 1, 9, 14, 15, 8, 0, 6, 13, 13, 334, 36, 44, 34, 42, 33, 41, 46, 47, 7, 382, 6, 13, 11, 383 };
	return ConversionTable[Color];
}

inline quint32 lcGetColorCodeFromOriginalColor(int Color)
{
	const quint32 ConversionTable[] = { 0, 2, 4, 9, 7, 6, 22, 8, 10, 11, 14, 16, 18, 9, 21, 20, 22, 8, 10, 11 };
	return lcGetColorCodeFromExtendedColor(ConversionTable[Color]);
}

inline quint32 lcGetColorCode(int ColorIndex)
{
	return gColorList[ColorIndex].Code;
}

inline bool lcIsColorTranslucent(size_t ColorIndex)
{
	return gColorList[ColorIndex].Translucent;
}

inline bool lcIsColorChrome(size_t ColorIndex)
{
	return gColorList[ColorIndex].Chrome;
}

inline bool lcIsColorRubber(size_t ColorIndex)
{
	return gColorList[ColorIndex].Rubber;
}

QString lcGetColorToolTip(int ColorIndex);

// Display name for a colour, translated when a translation exists.
//
// gColorList[].Name is data, not text: it is written into saved models, exported
// files and the colour profile, and lcColor::Name is a fixed char[64] filled by
// the LDConfig parser. Translating it in place would change saved values and,
// for names longer than 64 bytes, truncate them. So the stored name stays as
// parsed and only the string drawn on screen goes through the translator.
//
// The source strings live in the "Colors" context (see lc_colors.cpp), which
// keeps 300+ colour names out of the general translation context.
QString lcGetColorDisplayName(int ColorIndex);

// Same idea for the stored name of a colour group ("Solid", "Translucent",
// "Special"). Those three are already built with QApplication::tr() at load
// time, so this only exists to keep the call sites symmetrical.
QString lcGetColorGroupDisplayName(int ColorGroup);

void lcDrawNoColorRect(QPainter& Painter, const QRect& Rect);
