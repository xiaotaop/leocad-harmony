#pragma once

struct lcLibraryCategory
{
	QString Name;
	QByteArray Keywords;
};

extern std::vector<lcLibraryCategory> gCategories;

void lcResetDefaultCategories();
void lcLoadDefaultCategories(bool BuiltInLibrary = false);
void lcSaveDefaultCategories();

void lcResetCategories(std::vector<lcLibraryCategory>& Categories, bool BuiltInLibrary = false);
bool lcLoadCategories(const QString& FileName, std::vector<lcLibraryCategory>& Categories);
bool lcLoadCategories(const QByteArray& Buffer, std::vector<lcLibraryCategory>& Categories);
bool lcSaveCategories(const QString& FileName, const std::vector<lcLibraryCategory>& Categories);
bool lcSaveCategories(QTextStream& Stream, const std::vector<lcLibraryCategory>& Categories);

bool lcMatchCategory(const char* PieceName, const char* Expression);

// Localized name for a category, for display only.
//
// Category names are stored as English keywords in the profile and in
// lc_category.cpp because they are user data: a user can rename or add
// categories, and the name is written back to the settings file. Translating the
// stored value would corrupt that file and make saved categories unreadable
// after a language switch. The built-in names therefore stay English on disk and
// are translated here, at the point where they are put on screen.
//
// A name with no translation (a category the user created) is returned
// unchanged.
QString lcGetCategoryDisplayName(const QString& CategoryName);

// The built-in category names, for translation extraction. Kept next to
// lcGetCategoryDisplayName() so lupdate sees both in the same context.
void lcTranslateBuiltInCategoryNames();
