#pragma once

namespace Ui
{
	class lcModelListDialog;
}

struct lcModelListDialogEntry
{
	QString Name;
	lcModel* ExistingModel;
	lcModel* DuplicateSource;
};

class lcModelListDialog : public QDialog
{
	Q_OBJECT

public:
	// AllowStructureEditing controls the controls that change the .mpd structure:
	// new, delete, rename, duplicate, export and drag reordering.
	//
	// The first release passes false — this dialog is kept for submodel switching,
	// which the model tab bar also offers. true restores upstream behaviour.
	lcModelListDialog(QWidget* Parent, const std::vector<std::unique_ptr<lcModel>>& Models,
		bool AllowStructureEditing);
	~lcModelListDialog();

	int GetActiveModelIndex() const;
	std::vector<lcModelListDialogEntry> GetResults() const;

public slots:
	void accept() override;

private slots:
	void NewModelClicked();
	void DeleteModelClicked();
	void RenameModelClicked();
	void ExportModelClicked();
	void DuplicateModelClicked();
	void ModelListItemDoubleClicked(QListWidgetItem* Item);
	void ModelListItemSelectionChanged();

private:
	QListWidgetItem* mActiveModelItem;
	void UpdateButtons();
	Ui::lcModelListDialog* ui;
};

