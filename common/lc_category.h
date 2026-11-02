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

// Display name for a category, translated when a translation exists.
//
// lcLibraryCategory::Name is the key the category file is written with, so it
// has to stay English: lcSaveCategories() writes it back to the profile and
// lcLoadCategories() reads it from there, and the category name is also what a
// user-edited categories file is matched against. Translating it in place would
// mean a profile saved in one language no longer loads in another.
//
// The default names below are the source strings, in the "Categories" context.
QString lcGetCategoryDisplayName(const QString& Name);
