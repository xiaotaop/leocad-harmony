#include "lc_global.h"
#include "lc_colors.h"
#include "lc_file.h"
#include "lc_library.h"
#include "lc_application.h"
#include "lc_string.h"
#include <float.h>

std::vector<lcColor> gColorList;
lcColorGroup gColorGroups[LC_NUM_COLORGROUPS];
int gEdgeColor;
int gDefaultColor;

static void GetToken(char*& Ptr, char* Token)
{
	while (*Ptr && *Ptr <= 32)
		Ptr++;

	while (*Ptr > 32)
		*Token++ = *Ptr++;

	*Token = 0;
}

static void lcAdjustStudStyleColors(std::vector<lcColor>& Colors, lcStudStyle StudStyle)
{
	const lcPreferences& Preferences = lcGetPreferences();

	if (!Preferences.mAutomateEdgeColor && !lcIsHighContrast(StudStyle))
		return;

	const lcVector4 Edge = lcVector4FromColor(Preferences.mPartEdgeColor);
	const lcVector4 DarkEdge = lcVector4FromColor(Preferences.mDarkEdgeColor);
	const lcVector4 BlackEdge = lcVector4FromColor(Preferences.mBlackEdgeColor);

	const float ContrastControl = Preferences.mPartEdgeContrast;
	const float LightDarkControl = Preferences.mAutomateEdgeColor ? Preferences.mPartColorValueLDIndex : LC_SRGB_TO_LINEAR(Preferences.mPartColorValueLDIndex);

	for (lcColor& Color : Colors)
	{
		const lcVector3 LinearColor = lcSRGBToLinear(lcVector3(Color.Value));
		const float ValueLuminescence = lcLuminescence(LinearColor);

		if (Preferences.mAutomateEdgeColor)
		{
			if (Color.Adjusted)
				continue;

			const float EdgeLuminescence = lcLuminescence(lcSRGBToLinear(lcVector3(Color.Edge)));

			Color.Edge = lcAlgorithmicEdgeColor(LinearColor, ValueLuminescence, EdgeLuminescence, ContrastControl, LightDarkControl);
			Color.Adjusted = true;
		}
		else
		{
			if (Color.Code == LC_STUD_CYLINDER_COLOR_CODE)
				continue;
			else if (Preferences.mBlackEdgeColorEnabled && Color.Code == 0)
				Color.Edge = BlackEdge;
			else if (Preferences.mDarkEdgeColorEnabled && ValueLuminescence < LightDarkControl)
				Color.Edge = DarkEdge;
			else if (Preferences.mPartEdgeColorEnabled)
				Color.Edge = Edge;
		}
	}
}

static std::vector<lcColor> lcParseColorFile(lcFile& File)
{
	char Line[1024], Token[1024];
	std::vector<lcColor> Colors;
	lcColor Color;

	while (File.ReadLine(Line, sizeof(Line)))
	{
		char* Ptr = Line;

		GetToken(Ptr, Token);
		if (strcmp(Token, "0"))
			continue;

		GetToken(Ptr, Token);
		lcstrupr(Token);
		if (strcmp(Token, "!COLOUR"))
			continue;

		Color.Code = ~0U;
		Color.Translucent = false;
		Color.Chrome = false;
		Color.Rubber = false;
		Color.Group = LC_COLORGROUP_SOLID;
		Color.Value[0] = FLT_MAX;
		Color.Value[1] = FLT_MAX;
		Color.Value[2] = FLT_MAX;
		Color.Value[3] = 1.0f;
		Color.Edge[0] = FLT_MAX;
		Color.Edge[1] = FLT_MAX;
		Color.Edge[2] = FLT_MAX;
		Color.Edge[3] = 1.0f;

		GetToken(Ptr, Token);
		strncpy(Color.Name, Token, sizeof(Color.Name));
		Color.Name[LC_MAX_COLOR_NAME - 1] = 0;
		strncpy(Color.SafeName, Color.Name, sizeof(Color.SafeName));

		for (char* Underscore = strchr((char*)Color.Name, '_'); Underscore; Underscore = strchr(Underscore, '_'))
			*Underscore = ' ';

		for (GetToken(Ptr, Token); Token[0]; GetToken(Ptr, Token))
		{
			lcstrupr(Token);

			if (!strcmp(Token, "CODE"))
			{
				GetToken(Ptr, Token);
				Color.Code = atoi(Token);
			}
			else if (!strcmp(Token, "VALUE"))
			{
				GetToken(Ptr, Token);
				if (Token[0] == '#')
					Token[0] = ' ';

				int Value;
				if (sscanf(Token, "%x", &Value) != 1)
					Value = 0;

				Color.Value[2] = (float)(Value & 0xff) / 255.0f;
				Value >>= 8;
				Color.Value[1] = (float)(Value & 0xff) / 255.0f;
				Value >>= 8;
				Color.Value[0] = (float)(Value & 0xff) / 255.0f;
			}
			else if (!strcmp(Token, "EDGE"))
			{
				GetToken(Ptr, Token);
				if (Token[0] == '#')
					Token[0] = ' ';

				int Value;
				if (sscanf(Token, "%x", &Value) != 1)
					Value = 0;

				Color.Edge[2] = (float)(Value & 0xff) / 255.0f;
				Value >>= 8;
				Color.Edge[1] = (float)(Value & 0xff) / 255.0f;
				Value >>= 8;
				Color.Edge[0] = (float)(Value & 0xff) / 255.0f;
			}
			else if (!strcmp(Token, "ALPHA"))
			{
				GetToken(Ptr, Token);
				int Value = atoi(Token);
				Color.Value[3] = (float)(Value & 0xff) / 255.0f;
				if (Value != 255)
					Color.Translucent = true;

				if (Value == 128)
					Color.Group = LC_COLORGROUP_TRANSLUCENT;
				else if (Value != 0)
					Color.Group = LC_COLORGROUP_SPECIAL;
			}
			else if (!strcmp(Token, "CHROME"))
			{
				Color.Chrome = true;
				Color.Group = LC_COLORGROUP_SPECIAL;
			}
			else if (!strcmp(Token, "RUBBER"))
			{
				Color.Rubber = true;
				Color.Group = LC_COLORGROUP_SPECIAL;
			}
			else if (!strcmp(Token, "PEARLESCENT") || !strcmp(Token, "MATTE_METALIC") || !strcmp(Token, "METAL") || !strcmp(Token, "LUMINANCE"))
			{
				Color.Group = LC_COLORGROUP_SPECIAL;
			}
			else if (!strcmp(Token, "MATERIAL"))
			{
				Color.Group = LC_COLORGROUP_SPECIAL;
				break; // Material is always last so ignore it and the rest of the line.
			}
		}

		if (Color.Code == ~0U || Color.Value[0] == FLT_MAX)
			continue;

		if (Color.Edge[0] == FLT_MAX)
		{
			Color.Edge[0] = 33.0f / 255.0f;
			Color.Edge[1] = 33.0f / 255.0f;
			Color.Edge[2] = 33.0f / 255.0f;
		}

		bool Duplicate = false;

		for (lcColor& ExistingColor : Colors)
		{
			if (ExistingColor.Code == Color.Code)
			{
				ExistingColor = Color;
				Duplicate = true;
				break;
			}
		}

		if (!Duplicate)
			Colors.push_back(Color);
	}

	return Colors;
}

bool lcLoadColorFile(lcFile& File, lcStudStyle StudStyle)
{
	std::vector<lcColor> Colors = lcParseColorFile(File);
	const bool Valid = !Colors.empty();

	if (Valid)
		lcAdjustStudStyleColors(Colors, StudStyle);

	bool FoundMain = false, FoundEdge = false, FoundStudCylinder = false, FoundNoColor = false;

	for (const lcColor& Color : Colors)
	{
		switch (Color.Code)
		{
			case 16:
				FoundMain = true;
				break;

			case 24:
				FoundEdge = true;
				break;

			case LC_STUD_CYLINDER_COLOR_CODE:
				FoundStudCylinder = true;
				break;

			case LC_COLOR_NOCOLOR:
				FoundNoColor = true;
				break;
		}
	}

	if (!FoundMain)
	{
		lcColor MainColor;

		MainColor.Code = 16;
		MainColor.Translucent = false;
		MainColor.Chrome = false;
		MainColor.Rubber = false;
		MainColor.Group = LC_COLORGROUP_SOLID;
		MainColor.Value[0] = 1.0f;
		MainColor.Value[1] = 1.0f;
		MainColor.Value[2] = 0.5f;
		MainColor.Value[3] = 1.0f;
		MainColor.Edge[0] = 0.2f;
		MainColor.Edge[1] = 0.2f;
		MainColor.Edge[2] = 0.2f;
		MainColor.Edge[3] = 1.0f;
		lcstrcpy(MainColor.Name, "Main Color");
		lcstrcpy(MainColor.SafeName, "Main_Color");

		Colors.push_back(MainColor);
	}

	if (!FoundEdge)
	{
		lcColor EdgeColor;

		EdgeColor.Code = 24;
		EdgeColor.Translucent = false;
		EdgeColor.Chrome = false;
		EdgeColor.Rubber = false;
		EdgeColor.Group = LC_NUM_COLORGROUPS;
		EdgeColor.Value[0] = 0.5f;
		EdgeColor.Value[1] = 0.5f;
		EdgeColor.Value[2] = 0.5f;
		EdgeColor.Value[3] = 1.0f;
		EdgeColor.Edge[0] = 0.2f;
		EdgeColor.Edge[1] = 0.2f;
		EdgeColor.Edge[2] = 0.2f;
		EdgeColor.Edge[3] = 1.0f;
		lcstrcpy(EdgeColor.Name, "Edge Color");
		lcstrcpy(EdgeColor.SafeName, "Edge_Color");

		Colors.push_back(EdgeColor);
	}

	if (!FoundStudCylinder)
	{
		const lcPreferences& Preferences = lcGetPreferences();
		lcColor StudCylinderColor;

		StudCylinderColor.Code = LC_STUD_CYLINDER_COLOR_CODE;
		StudCylinderColor.Translucent = false;
		StudCylinderColor.Chrome = false;
		StudCylinderColor.Rubber = false;
		StudCylinderColor.Group = LC_NUM_COLORGROUPS;
		StudCylinderColor.Value = lcVector4FromColor(Preferences.mStudCylinderColor);
		StudCylinderColor.Edge = lcVector4FromColor(Preferences.mPartEdgeColor);
		lcstrcpy(StudCylinderColor.Name, "Stud Cylinder Color");
		lcstrcpy(StudCylinderColor.SafeName, "Stud_Cylinder_Color");

		Colors.push_back(StudCylinderColor);
	}

	if (!FoundNoColor)
	{
		lcColor NoColor;

		NoColor.Code = LC_COLOR_NOCOLOR;
		NoColor.Translucent = false;
		NoColor.Chrome = false;
		NoColor.Rubber = false;
		NoColor.Group = LC_NUM_COLORGROUPS;
		NoColor.Value[0] = 0.5f;
		NoColor.Value[1] = 0.5f;
		NoColor.Value[2] = 0.5f;
		NoColor.Value[3] = 1.0f;
		NoColor.Edge[0] = 0.2f;
		NoColor.Edge[1] = 0.2f;
		NoColor.Edge[2] = 0.2f;
		NoColor.Edge[3] = 1.0f;
		lcstrcpy(NoColor.Name, "No Color");
		lcstrcpy(NoColor.SafeName, "No_Color");

		Colors.push_back(NoColor);
	}

	for (lcColor& Color : gColorList)
		Color.Group = LC_NUM_COLORGROUPS;

	for (int GroupIdx = 0; GroupIdx < LC_NUM_COLORGROUPS; GroupIdx++)
		gColorGroups[GroupIdx].Colors.clear();

	gColorGroups[0].Name = QApplication::tr("Solid", "Colors");
	gColorGroups[1].Name = QApplication::tr("Translucent", "Colors");
	gColorGroups[2].Name = QApplication::tr("Special", "Colors");

	for (lcColor& Color : Colors)
	{
		int ColorIndex;

		for (ColorIndex = 0; ColorIndex < static_cast<int>(gColorList.size()); ColorIndex++)
			if (gColorList[ColorIndex].Code == Color.Code)
				break;

		if (ColorIndex == static_cast<int>(gColorList.size()))
			gColorList.push_back(Color);
		else
			gColorList[ColorIndex] = Color;

		if (Color.Group != LC_NUM_COLORGROUPS)
			gColorGroups[Color.Group].Colors.push_back(ColorIndex);

		if (Color.Code == 16)
			gDefaultColor = ColorIndex;
		else if (Color.Code == 24)
			gEdgeColor = ColorIndex;
	}

	return Valid;
}

void lcLoadDefaultColors(lcStudStyle StudStyle)
{
	lcDiskFile ConfigFile(":/resources/ldconfig.ldr");

	if (ConfigFile.Open(QIODevice::ReadOnly))
		lcLoadColorFile(ConfigFile, StudStyle);
}

int lcGetColorIndex(quint32 ColorCode)
{
	for (size_t ColorIdx = 0; ColorIdx < gColorList.size(); ColorIdx++)
		if (gColorList[ColorIdx].Code == ColorCode)
			return (int)ColorIdx;

	lcColor Color;

	Color.Code = ColorCode;
	Color.Translucent = false;
	Color.Chrome = false;
	Color.Rubber = false;
	Color.Edge[0] = 0.2f;
	Color.Edge[1] = 0.2f;
	Color.Edge[2] = 0.2f;
	Color.Edge[3] = 1.0f;

	if (ColorCode & LC_COLOR_DIRECT)
	{
		Color.Value[0] = (float)((ColorCode & 0xff0000) >> 16) / 255.0f;
		Color.Value[1] = (float)((ColorCode & 0x00ff00) >> 8) / 255.0f;
		Color.Value[2] = (float)((ColorCode & 0x0000ff) >> 0) / 255.0f;
		Color.Value[3] = 1.0f;
		snprintf(Color.Name, sizeof(Color.Name), "Color %06X", ColorCode & 0xffffff);
		snprintf(Color.SafeName, sizeof(Color.SafeName), "Color_%06X", ColorCode & 0xffffff);
	}
	else
	{
		Color.Value[0] = 0.5f;
		Color.Value[1] = 0.5f;
		Color.Value[2] = 0.5f;
		Color.Value[3] = 1.0f;
		snprintf(Color.Name, sizeof(Color.Name), "Color %03d", ColorCode);
		snprintf(Color.SafeName, sizeof(Color.SafeName), "Color_%03d", ColorCode);
	}

	gColorList.push_back(Color);
	return (int)gColorList.size() - 1;
}

QString lcGetColorToolTip(int ColorIndex)
{
	lcColor* Color = &gColorList[ColorIndex];
	QColor Rgb(Color->Value[0] * 255, Color->Value[1] * 255, Color->Value[2] * 255);

	QImage Image(16, 16, QImage::Format_RGB888);
	Image.fill(Rgb);

	QPainter Painter(&Image);
	Painter.setPen(Qt::darkGray);

	if (Color->Code != LC_COLOR_NOCOLOR)
		Painter.drawRect(0, 0, Image.width() - 1, Image.height() - 1);
	else
		lcDrawNoColorRect(Painter, QRect(0, 0, Image.width() - 1, Image.height() - 1));

	Painter.end();

	QByteArray ByteArray;
	QBuffer Buffer(&ByteArray);
	Buffer.open(QIODevice::WriteOnly);
	Image.save(&Buffer, "PNG");
	Buffer.close();

	QString Text;

	if (Color->Code != LC_COLOR_NOCOLOR)
	{
		const char* format = "<table><tr><td style=\"vertical-align:middle\"><img src=\"data:image/png;base64,%1\"/></td><td>%2 (%3)</td></tr></table>";
		Text = QString(format).arg(QString(Buffer.data().toBase64()), lcGetColorDisplayName(ColorIndex), QString::number(gColorList[ColorIndex].Code));
	}
	else
	{
		const char* format = "<table><tr><td style=\"vertical-align:middle\"><img src=\"data:image/png;base64,%1\"/></td><td>%2</td></tr></table>";
		Text = QString(format).arg(QString(Buffer.data().toBase64()), lcGetColorDisplayName(ColorIndex));
	}

	return Text;
}


void lcDrawNoColorRect(QPainter& Painter, const QRect& Rect)
{
	Painter.setBrush(Qt::black);
	Painter.drawRect(Rect);

	const int SquareSize = 3;
	int Column = 0;

	for (;;)
	{
		int x = Rect.left() + 1 + Column * SquareSize;

		if (x >= Rect.right())
			break;

		int Row = Column & 1;

		for (;;)
		{
			int y = Rect.top() + 1 + Row * SquareSize;

			if (y >= Rect.bottom())
				break;

			QRect GridRect(x, y, SquareSize, SquareSize);

			if (GridRect.right() > Rect.right())
				GridRect.setRight(Rect.right());

			if (GridRect.bottom() > Rect.bottom())
				GridRect.setBottom(Rect.bottom());

			Painter.fillRect(GridRect, Qt::white);

			Row += 2;
		}

		Column++;
	}
}

QString lcGetColorDisplayName(int ColorIndex)
{
	if (ColorIndex < 0 || ColorIndex >= (int)gColorList.size())
		return QString();

	// The stored name is English on purpose (see lc_colors.h). Only the value
	// returned here is translated, so exports and the fixed-size buffers stay
	// untouched.
	const QString EnglishName = QString::fromUtf8(gColorList[ColorIndex].Name);

	if (EnglishName.isEmpty())
		return EnglishName;

	const QString Translated = QCoreApplication::translate("Colors", gColorList[ColorIndex].Name);

	// QCoreApplication::translate() returns the input when there is no
	// translation, which is the fallback we want for custom colour codes.
	return Translated;
}

// Extraction source for the colour names that lcGetColorDisplayName()
// translates at runtime. lupdate cannot see a string built at runtime, so the
// English names are listed here (generated from resources/color_names_zh.tsv)
// purely so they appear in the translation files.
void lcTranslateBuiltInColorNames()
{
	QT_TRANSLATE_NOOP("Colors", "Black");
	QT_TRANSLATE_NOOP("Colors", "Blue");
	QT_TRANSLATE_NOOP("Colors", "Green");
	QT_TRANSLATE_NOOP("Colors", "Dark Turquoise");
	QT_TRANSLATE_NOOP("Colors", "Red");
	QT_TRANSLATE_NOOP("Colors", "Dark Pink");
	QT_TRANSLATE_NOOP("Colors", "Brown");
	QT_TRANSLATE_NOOP("Colors", "Light Grey");
	QT_TRANSLATE_NOOP("Colors", "Dark Grey");
	QT_TRANSLATE_NOOP("Colors", "Light Blue");
	QT_TRANSLATE_NOOP("Colors", "Bright Green");
	QT_TRANSLATE_NOOP("Colors", "Light Turquoise");
	QT_TRANSLATE_NOOP("Colors", "Salmon");
	QT_TRANSLATE_NOOP("Colors", "Pink");
	QT_TRANSLATE_NOOP("Colors", "Yellow");
	QT_TRANSLATE_NOOP("Colors", "White");
	QT_TRANSLATE_NOOP("Colors", "Light Green");
	QT_TRANSLATE_NOOP("Colors", "Light Yellow");
	QT_TRANSLATE_NOOP("Colors", "Tan");
	QT_TRANSLATE_NOOP("Colors", "Light Violet");
	QT_TRANSLATE_NOOP("Colors", "Purple");
	QT_TRANSLATE_NOOP("Colors", "Light Purple");
	QT_TRANSLATE_NOOP("Colors", "Orange");
	QT_TRANSLATE_NOOP("Colors", "Magenta");
	QT_TRANSLATE_NOOP("Colors", "Lime");
	QT_TRANSLATE_NOOP("Colors", "Dark Tan");
	QT_TRANSLATE_NOOP("Colors", "Bright Pink");
	QT_TRANSLATE_NOOP("Colors", "Medium Blue");
	QT_TRANSLATE_NOOP("Colors", "Nougat");
	QT_TRANSLATE_NOOP("Colors", "Sand Blue");
	QT_TRANSLATE_NOOP("Colors", "Sand Green");
	QT_TRANSLATE_NOOP("Colors", "Sand Red");
	QT_TRANSLATE_NOOP("Colors", "Dark Red");
	QT_TRANSLATE_NOOP("Colors", "Dark Blue");
	QT_TRANSLATE_NOOP("Colors", "Dark Green");
	QT_TRANSLATE_NOOP("Colors", "Trans Clear");
	QT_TRANSLATE_NOOP("Colors", "Trans Black");
	QT_TRANSLATE_NOOP("Colors", "Trans Red");
	QT_TRANSLATE_NOOP("Colors", "Trans Blue");
	QT_TRANSLATE_NOOP("Colors", "Trans Green");
	QT_TRANSLATE_NOOP("Colors", "Trans Yellow");
	QT_TRANSLATE_NOOP("Colors", "Trans Orange");
	QT_TRANSLATE_NOOP("Colors", "Trans Dark Blue");
	QT_TRANSLATE_NOOP("Colors", "Trans Light Blue");
	QT_TRANSLATE_NOOP("Colors", "Trans Neon Green");
	QT_TRANSLATE_NOOP("Colors", "Trans Neon Orange");
	QT_TRANSLATE_NOOP("Colors", "Chrome Silver");
	QT_TRANSLATE_NOOP("Colors", "Chrome Gold");
	QT_TRANSLATE_NOOP("Colors", "Main Color");
	QT_TRANSLATE_NOOP("Colors", "Edge Color");
	QT_TRANSLATE_NOOP("Colors", "No Color");
	QT_TRANSLATE_NOOP("Colors", "Stud Cylinder Color");
}
