// Verifies the submodel list dialog offers switching but not structure editing.
//
// The dialog is reached from LC_MODEL_LIST, which the first release keeps because
// it is the second entry point to MPD submodel switching (the model tab bar is the
// first). Removing LC_MODEL_NEW from the menu does not restrict this path: the
// dialog calls the same Project operations directly, so it has to be checked on
// the real widgets rather than inferred from the menu.

#include <QAbstractItemView>
#include <QCheckBox>
#include <QListWidget>
#include <QPushButton>
#include <QtTest>

#include "lc_application.h"
#include "lc_library.h"
#include "lc_mainwindow.h"
#include "lc_model.h"
#include "lc_modellistdialog.h"
#include "project.h"

class ModelListDialogTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();

	void structureControlsAreHidden();
	void listIsNotReorderable();
	void switchingControlsRemain();

private:
	// Builds a dialog over a two-submodel project, the way Project does.
	lcModelListDialog* MakeDialog(bool AllowStructureEditing);

	Project* mProject = nullptr;
};

void ModelListDialogTest::initTestCase()
{
	QVERIFY(gApplication);

	if (!gApplication->mLibrary)
	{
		const QString Archive = QFINDTESTDATA("../../../assets/ldraw-lite/library-lite.zip");
		QVERIFY2(!Archive.isEmpty(), "bundled library not found");

		QList<QPair<QString, bool>> Paths;
		Paths += qMakePair(Archive, true);
		QVERIFY2(gApplication->LoadPartsLibrary(Paths, true), "could not load the parts library");
	}

	// The dialog reads the active model from the active project.
	mProject = new Project();

	if (!lcGetActiveProject())
		gApplication->SetProject(mProject);
}

lcModelListDialog* ModelListDialogTest::MakeDialog(bool AllowStructureEditing)
{
	Project* Active = lcGetActiveProject();
	Q_ASSERT(Active);

	// Empty parent: the dialog is inspected, never shown.
	return new lcModelListDialog(nullptr, Active->GetModels(), AllowStructureEditing);
}

void ModelListDialogTest::structureControlsAreHidden()
{
	QScopedPointer<lcModelListDialog> Dialog(MakeDialog(false));

	const char* Buttons[] = { "NewModel", "DeleteModel", "RenameModel", "ExportModel", "DuplicateModel" };

	for (const char* Name : Buttons)
	{
		QPushButton* Button = Dialog->findChild<QPushButton*>(QLatin1String(Name));
		QVERIFY2(Button, qPrintable(QStringLiteral("%1 button missing from the dialog").arg(QLatin1String(Name))));
		QVERIFY2(Button->isHidden(),
			qPrintable(QStringLiteral("%1 is still visible; it changes the .mpd structure")
				.arg(QLatin1String(Name))));
	}
}

void ModelListDialogTest::listIsNotReorderable()
{
	QScopedPointer<lcModelListDialog> Dialog(MakeDialog(false));

	QListWidget* List = Dialog->findChild<QListWidget*>(QLatin1String("ModelList"));
	QVERIFY(List);

	// InternalMove on the view is what let a user reorder submodels by dragging.
	QCOMPARE(List->dragDropMode(), QAbstractItemView::NoDragDrop);
}

void ModelListDialogTest::switchingControlsRemain()
{
	QScopedPointer<lcModelListDialog> Dialog(MakeDialog(false));

	// The whole reason the dialog is kept: activate-on-accept and double-click.
	QVERIFY(Dialog->findChild<QListWidget*>(QLatin1String("ModelList")));

	// It is a QCheckBox, not a button: "set the selected submodel active on accept".
	QCheckBox* SetActive = Dialog->findChild<QCheckBox*>(QLatin1String("SetActiveModel"));
	QVERIFY2(SetActive, "the activate toggle disappeared; the dialog no longer switches submodels");
	QVERIFY(!SetActive->isHidden());

	// Sanity check on the gate itself: with structure editing allowed the buttons
	// must come back, otherwise this test would pass on a dialog that is simply
	// always disabled.
	QScopedPointer<lcModelListDialog> Permissive(MakeDialog(true));
	QPushButton* NewModel = Permissive->findChild<QPushButton*>(QLatin1String("NewModel"));
	QVERIFY(NewModel);
	QVERIFY2(!NewModel->isHidden(), "AllowStructureEditing=true did not restore the controls");
}

int main(int argc, char** argv)
{
	lcApplication Application(argc, argv);
	ModelListDialogTest Test;

	return QTest::qExec(&Test, argc, argv);
}

#include "tst_modellist_dialog.moc"
