#pragma once

#include "lc_application.h"
#include "lc_shortcuts.h"
#include "lc_category.h"

struct lcPreferencesDialogOptions
{
	lcPreferences Preferences;

	QString LibraryPath;
	QString ColorConfigPath;
	QString MinifigSettingsPath;
	QString POVRayPath;
	QString LGEOPath;
	QString DefaultAuthor;
	QString Language;
	int CheckForUpdates;

	int AASamples;
	lcStudStyle StudStyle;

	std::vector<lcLibraryCategory> Categories;
	bool CategoriesModified;
	bool CategoriesDefault;

	lcKeyboardShortcuts KeyboardShortcuts;
	bool KeyboardShortcutsModified;
	bool KeyboardShortcutsDefault;

	lcMouseShortcuts MouseShortcuts;
	bool MouseShortcutsModified;
	bool MouseShortcutsDefault;
};

namespace Ui
{
class lcPreferencesDialog;
}

class lcPreferencesDialog : public QDialog
{
	Q_OBJECT

public:
	lcPreferencesDialog(QWidget* Parent, lcPreferencesDialogOptions* Options);
	~lcPreferencesDialog();

	lcPreferencesDialogOptions* mOptions;

	enum
	{
		CategoryRole = Qt::UserRole
	};

	bool eventFilter(QObject* Object, QEvent* Event) override;

	// Hides the settings for features the first HarmonyOS release does not ship.
	//
	// Split in two because the dialog cannot be constructed without a live GL
	// context: its constructor calls gMainWindow->GetActiveView()->GetWidget()->
	// context()->functions() to query the line-width range, which dereferences a
	// null context on an offscreen platform. The list of names is therefore
	// exposed separately, so a test can check the scope itself without building
	// the dialog — and so the two lists cannot drift apart.
	struct ScopedWidgets
	{
		// Out of scope: hidden.
		static const char* const* Hidden();
		static int HiddenCount();

		// In scope: must stay visible. Listed explicitly because the two lists
		// live in the same pages and a group-level hide would take them too.
		static const char* const* Kept();
		static int KeptCount();
	};

	void ApplyFirstReleasePreferencesScope();

public slots:
	void accept() override;

private slots:
	void PartsLibraryBrowseClicked();
	void PartsArchiveBrowseClicked();
	void ColorConfigBrowseButtonClicked();
	void MinifigSettingsBrowseButtonClicked();
	void PovrayExecutableBrowseClicked();
	void LgeoPathBrowseClicked();
	void ColorThemeCurrentIndexChanged(int Index);
	void ColorButtonClicked();
	void AutomateEdgeColor();
	void AutomateEdgeColorToggled();
	void BlenderAddonSettingsButtonClicked();
	void StudStyleComboCurrentIndexChanged(int Index);
	void AntiAliasingToggled();
	void EdgeLinesToggled();
	void ConditionalLinesCheckBoxToggled();
	void LineWidthSliderValueChanged();
	void MeshLODSliderValueChanged();
	void FadeStepsToggled();
	void HighlightNewPartsToggled();
	void GridStudsToggled();
	void GridLinesToggled();
	void ViewSphereSizeComboCurrentIndexChanged(int Index);
	void PreviewViewSphereSizeComboCurrentIndexChanged(int Index);
	void UpdateParts();
	void CategoriesDropped(const QModelIndex& Parent, int First, int Last);
	void NewCategoryClicked();
	void EditCategoryClicked();
	void DeleteCategoryClicked();
	void ImportCategoriesClicked();
	void ExportCategoriesClicked();
	void ResetCategoriesClicked();
	void ShortcutAssignClicked();
	void ShortcutRemoveClicked();
	void ShortcutsImportClicked();
	void ShortcutsExportClicked();
	void ShortcutsResetClicked();
	void CommandChanged(QTreeWidgetItem* Current);
	void KeyboardFilterEditTextEdited(const QString& Text);
	void MouseAssignClicked();
	void MouseRemoveClicked();
	void MouseImportButtonClicked();
	void MouseExportButtonClicked();
	void MouseResetClicked();
	void MouseTreeItemChanged(QTreeWidgetItem* Current);

private:
	Ui::lcPreferencesDialog* ui;

	void UpdateCategories();
	void UpdateCommandList();
	void UpdateMouseTree();
	void UpdateMouseTreeItem(int ItemIndex);
	void SetShortcutModified(QTreeWidgetItem* TreeItem, bool Modified);

	float mLineWidthRange[2];
	float mLineWidthGranularity;
	static constexpr float mMeshLODMultiplier = 25.0f;
};
