#include "lc_global.h"
#include <QtTest>
#include "lc_application.h"
#include "lc_context.h"
#include "lc_library.h"
#include "lc_mainwindow.h"
#include "lc_model.h"
#include "lc_modelhistory.h"
#include "piece.h"
#include "project.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif

class FailingWriteDevice : public QIODevice
{
public:
    FailingWriteDevice() { open(QIODevice::WriteOnly); }
protected:
    qint64 readData(char*, qint64) override { return -1; }
    qint64 writeData(const char*, qint64) override
    {
        setErrorString("Simulated full output device");
        return -1;
    }
};

// Real upstream UI collaborators, kept hidden. Preview models do not retain undo.
class EditingSession
{
public:
    EditingSession()
    {
        lcLoadDefaultKeyboardShortcuts();
        lcLoadDefaultMouseShortcuts();
        window = std::make_unique<lcMainWindow>();
        window->CreateWidgets();
        gApplication->SetProject(new Project());
        window->SetAddKeys(false);
        window->SetRelativeTransform(false);
        window->SetSeparateTransform(false);
    }
    ~EditingSession()
    {
        window.reset();
        delete gApplication->mProject;
        gApplication->mProject = nullptr;
    }
    std::unique_ptr<lcMainWindow> window;
};

class ModelIoTest : public QObject
{
    Q_OBJECT
private slots:
    void emptyModelRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("empty.ldr");
        Project source(true);
        QVERIFY(source.Save(path).has_value());
        Project reopened(true);
        QVERIFY(reopened.Load(path, false));
        QCOMPARE(reopened.GetModels().size(), size_t(1));
        QCOMPARE(reopened.GetMainModel()->GetPieces().size(), size_t(0));
    }

    void ldrRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Project source(true);
        QVERIFY(source.Load(QFINDTESTDATA("../fixtures/two-pieces.ldr"), false));
        QCOMPARE(source.GetMainModel()->GetPieces().size(), size_t(2));
        const QString path = dir.filePath(QString::fromUtf8("模型 roundtrip.ldr"));
        QVERIFY(source.Save(path).has_value());
        Project reopened(true);
        QVERIFY(reopened.Load(path, false));
        const auto& pieces = reopened.GetMainModel()->GetPieces();
        QCOMPARE(pieces.size(), size_t(2));
        QCOMPARE(pieces[0]->GetID(), QString("b_fixture_brick.dat"));
        QCOMPARE(pieces[0]->GetColorCode(), quint32(4));
        QCOMPARE(pieces[1]->GetColorCode(), quint32(1));
        // LDraw (x,y,z) maps to LeoCAD (x,z,-y).
        QCOMPARE(pieces[1]->mModelWorld.GetTranslation(), lcVector3(20.0f, 40.0f, -30.0f));
        const lcMatrix44& matrix = pieces[1]->mModelWorld;
        QCOMPARE(matrix[0], lcVector4(0.0f, -1.0f, 0.0f, 0.0f));
        QCOMPARE(matrix[1], lcVector4(1.0f, 0.0f, 0.0f, 0.0f));
        QCOMPARE(matrix[2], lcVector4(0.0f, 0.0f, 1.0f, 0.0f));
    }

    void mpdRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("assembly.mpd");
        Project source(true);
        QVERIFY(source.Load(QFINDTESTDATA("../fixtures/submodel.mpd"), false));
        QCOMPARE(source.GetModels().size(), size_t(2));
        QVERIFY(source.Save(path).has_value());
        Project reopened(true);
        QVERIFY(reopened.Load(path, false));
        QCOMPARE(reopened.GetModels().size(), size_t(2));
        lcModel* main = reopened.GetModel("main.ldr");
        lcModel* child = reopened.GetModel("child.ldr");
        QVERIFY(main);
        QVERIFY(child);
        QCOMPARE(main->GetPieces().size(), size_t(1));
        QCOMPARE(child->GetPieces().size(), size_t(1));
        QCOMPARE(main->GetPieces()[0]->GetID(), QString("child.ldr"));
        QCOMPARE(child->GetPieces()[0]->GetColorCode(), quint32(14));
    }

    void missingFilePreservesModel()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Project project(true);
        QVERIFY(project.Load(QFINDTESTDATA("../fixtures/two-pieces.ldr"), false));
        const QString originalName = project.GetFileName();
        QVERIFY(!project.Load(dir.filePath("missing.ldr"), false));
        QCOMPARE(project.GetFileName(), originalName);
        QCOMPARE(project.GetMainModel()->GetPieces().size(), size_t(2));
    }

    void unwritableDestinationReportsError()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Project project(true);
        // A directory is a deterministic invalid file destination.
        const auto result = project.Save(dir.path());
        QVERIFY(!result.has_value());
        QVERIFY(!result.error().isEmpty());
    }

    void failedSaveAsPreservesDocumentState()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Project project(true);
        QVERIFY(project.Load(QFINDTESTDATA("../fixtures/two-pieces.ldr"), false));
        const QString originalName = project.GetFileName();
        project.MarkAsModified();
        const auto result = project.Save(dir.path());
        QVERIFY(!result.has_value());
        QCOMPARE(project.GetFileName(), originalName);
        QVERIFY(project.IsModified());
        QCOMPARE(project.GetMainModel()->GetPieces().size(), size_t(2));
    }

    void streamWriteFailureIsReported()
    {
        Project project(true);
        FailingWriteDevice device;
        QTextStream stream(&device);
        QVERIFY(!project.Save(stream));
        QCOMPARE(stream.status(), QTextStream::WriteFailed);
    }

    void successfulOverwriteCanBeReopened()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("existing.ldr");
        Project project(true);
        QVERIFY(project.Save(path).has_value());
        QVERIFY(project.Load(QFINDTESTDATA("../fixtures/two-pieces.ldr"), false));
        project.MarkAsModified();
        QVERIFY(project.Save(path).has_value());
        QCOMPARE(project.GetFileName(), path);
        QVERIFY(!project.IsModified());
        Project reopened(true);
        QVERIFY(reopened.Load(path, false));
        QCOMPARE(reopened.GetMainModel()->GetPieces().size(), size_t(2));
    }

#ifdef Q_OS_WIN
    void failedReplacementPreservesExistingFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("locked.ldr");
        const QByteArray original("0 Original document must survive failed replacement\r\n");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(original), qint64(original.size()));
        file.close();
        Project project(true);
        QVERIFY(project.Load(path, false));
        project.MarkAsModified();

        // Allow reading, but deny writes and replacement of this test-owned file.
        HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(handle != INVALID_HANDLE_VALUE);
        const auto result = project.Save(path);
        CloseHandle(handle);

        QVERIFY(!result.has_value());
        QVERIFY(!result.error().isEmpty());
        QCOMPARE(project.GetFileName(), path);
        QVERIFY(project.IsModified());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), original);
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden).size(), 1);
    }
#endif

    void editingSequenceUndoRedoAndSave()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        EditingSession session;
        auto loaded = std::make_unique<Project>();
        QVERIFY(loaded->Load(QFINDTESTDATA("../fixtures/two-pieces.ldr"), false));
        gApplication->SetProject(loaded.release());
        Project* project = lcGetActiveProject();
        lcModel* model = project->GetActiveModel();
        lcPiece* first = model->GetPieces()[0].get();
        model->SetSelectionAndFocusAction({first}, first, LC_PIECE_SECTION_POSITION, lcSelectionMode::Single);
        QVERIFY(project->Save(dir.filePath("edited.ldr")).has_value());

        model->MoveSelectedObjects(lcVector3(20, 0, 0), lcModelTransformFlag::Checkpoint, lcModelHistoryEditMerge::None);
        QCOMPARE(first->mModelWorld.GetTranslation(), lcVector3(20, 0, 0));
        const lcMatrix33 quarterTurn(lcVector3(0, 1, 0), lcVector3(-1, 0, 0), lcVector3(0, 0, 1));
        model->RotateSelectedObjects(quarterTurn, lcModelTransformFlag::Checkpoint, lcModelHistoryEditMerge::None);
        QCOMPARE(first->mModelWorld[0], lcVector4(0, 1, 0, 0));
        QCOMPARE(first->mModelWorld[1], lcVector4(-1, 0, 0, 0));
        model->SetSelectedPiecesColorIndex(lcGetColorIndex(2));
        QCOMPARE(first->GetColorCode(), quint32(2));
        model->DeleteSelectedObjects();
        QCOMPARE(model->GetPieces().size(), size_t(1));

        model->UndoAction();
        QCOMPARE(model->GetPieces().size(), size_t(2));
        model->UndoAction();
        QCOMPARE(model->GetPieces()[0]->GetColorCode(), quint32(4));
        model->UndoAction();
        QCOMPARE(model->GetPieces()[0]->mModelWorld[0], lcVector4(1, 0, 0, 0));
        model->UndoAction();
        QCOMPARE(model->GetPieces()[0]->mModelWorld.GetTranslation(), lcVector3(0, 0, 0));
        QVERIFY(!project->IsModified());
        for (int i = 0; i < 4; ++i)
            model->RedoAction();
        QCOMPARE(model->GetPieces().size(), size_t(1));
        QVERIFY(project->IsModified());
        model->UndoAction();

        const QString path = dir.filePath("edited.ldr");
        QVERIFY(project->Save(path).has_value());
        QVERIFY(!model->IsModified());
        Project reopened(true);
        QVERIFY(reopened.Load(path, false));
        const auto& pieces = reopened.GetMainModel()->GetPieces();
        QCOMPARE(pieces.size(), size_t(2));
        QCOMPARE(pieces[0]->GetColorCode(), quint32(2));
        QCOMPARE(pieces[0]->mModelWorld.GetTranslation(), lcVector3(20, 0, 0));
        QCOMPARE(pieces[0]->mModelWorld[0], lcVector4(0, 1, 0, 0));

        model->UndoAction();
        QVERIFY(model->IsModified());
        model->RedoAction();
        QVERIFY(!model->IsModified());
        model->UndoAction();
        first = model->GetPieces()[0].get();
        model->SetSelectionAndFocusAction({first}, first, LC_PIECE_SECTION_POSITION, lcSelectionMode::Single);
        model->SetSelectedPiecesColorIndex(lcGetColorIndex(14));
        model->RedoAction();
        QCOMPARE(model->GetPieces()[0]->GetColorCode(), quint32(14));
        QVERIFY(model->IsModified());
    }

    void insertTwentyPiecesUndoRedoAndReopen()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        EditingSession session;
        Project* project = lcGetActiveProject();
        lcModel* model = project->GetActiveModel();
        PieceInfo* info = lcGetPiecesLibrary()->FindPiece("b_fixture_brick.dat", project, true, true);
        QVERIFY(info);
        for (int i = 0; i < 20; ++i)
        {
            QVERIFY(model->AddPiece(info, LC_PIECE_SECTION_INVALID));
            QCOMPARE(model->GetPieces().size(), size_t(i + 1));
        }
        for (int i = 0; i < 20; ++i)
            model->UndoAction();
        QCOMPARE(model->GetPieces().size(), size_t(0));
        QVERIFY(!project->IsModified());
        for (int i = 0; i < 20; ++i)
            model->RedoAction();
        QCOMPARE(model->GetPieces().size(), size_t(20));
        const QString path = dir.filePath("twenty.ldr");
        QVERIFY(project->Save(path).has_value());
        Project reopened(true);
        QVERIFY(reopened.Load(path, false));
        QCOMPARE(reopened.GetMainModel()->GetPieces().size(), size_t(20));
    }

    void serializationDoesNotClearEditHistoryDirtyState()
    {
        EditingSession session;
        Project* project = lcGetActiveProject();
        lcModel* model = project->GetActiveModel();
        PieceInfo* info = lcGetPiecesLibrary()->FindPiece("b_fixture_brick.dat", project, true, true);
        QVERIFY(model->AddPiece(info, LC_PIECE_SECTION_INVALID));
        QVERIFY(model->IsModified());
        FailingWriteDevice failingDevice;
        QTextStream failingStream(&failingDevice);
        QVERIFY(!project->Save(failingStream));
        QVERIFY(model->IsModified());
        QString text;
        QTextStream stream(&text);
        QVERIFY(project->Save(stream));
        QVERIFY(!text.isEmpty());
        QVERIFY(model->IsModified());
        model->UndoAction();
        QVERIFY(!model->IsModified());
        model->RedoAction();
        QVERIFY(model->IsModified());
    }

    void invalidLDrawPreservesCurrentDocument_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::newRow("empty-file") << QByteArray();
        QTest::newRow("truncated-reference") << QByteArray("0 Broken\n1 4 0 0 0 1 0\n");
        QTest::newRow("missing-part-name") << QByteArray("0 Broken\n1 4 0 0 0 1 0 0 0 1 0 0 0 1\n");
        QTest::newRow("nonfinite-transform") << QByteArray("0 Broken\n1 4 nan 0 0 1 0 0 0 1 0 0 0 1 b_fixture_brick.dat\n");
        QTest::newRow("duplicate-submodel") << QByteArray("0 FILE main.ldr\n0 First\n0 FILE MAIN.LDR\n0 Duplicate\n");
        QTest::newRow("unnamed-submodel") << QByteArray("0 FILE\n0 Missing name\n");
        QTest::newRow("truncated-triangle") << QByteArray("0 Broken\n3 16 0 0 0\n");
        QTest::newRow("overlong-reference") << (QByteArray("0 Broken\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 ") + QByteArray(256, 'a') + "\n");
    }

    void invalidLDrawPreservesCurrentDocument()
    {
        QFETCH(QByteArray, contents);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("invalid.mpd");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), qint64(contents.size()));
        file.close();
        Project project(true);
        QVERIFY(project.Load(QFINDTESTDATA("../fixtures/two-pieces.ldr"), false));
        project.MarkAsModified();
        const QString originalName = project.GetFileName();
        lcModel* originalModel = project.GetMainModel();
        QVERIFY(!project.Load(path, false));
        QCOMPARE(project.GetFileName(), originalName);
        QCOMPARE(project.GetMainModel(), originalModel);
        QVERIFY(project.IsModified());
        QCOMPARE(project.GetMainModel()->GetPieces().size(), size_t(2));
        const auto result = project.Load(path);
        QVERIFY(!result.has_value());
        QVERIFY(!result.error().isEmpty());
    }

    void loadedActiveModelBelongsToProject()
    {
        Project project(true);
        QVERIFY(project.Load(QFINDTESTDATA("../fixtures/submodel.mpd"), false));
        QCOMPARE(project.GetActiveModel(), project.GetMainModel());
    }

    void compatibleLDrawSyntax_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::newRow("bom-and-direct-color") << QByteArray("\xef\xbb\xbf" "0 Valid\n1 0x2FF0000 0 0 0 1 0 0 0 1 0 0 0 1 b_fixture_brick.dat\n");
        QTest::newRow("unknown-meta-and-line") << QByteArray("0 !OTHER_VENDOR keep this\n9 ignored\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 b_fixture_brick.dat\n");
        QTest::newRow("mpd-trailing-signature") << QByteArray("0 FILE main.ldr\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 b_fixture_brick.dat\n0 NOFILE\n1 incomplete text outside a block\n");
    }

    void compatibleLDrawSyntax()
    {
        QFETCH(QByteArray, contents);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath("compatible.ldr"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), qint64(contents.size()));
        file.close();
        Project project(true);
        const auto result = project.Load(file.fileName());
        QVERIFY2(result.has_value(), result.has_value() ? "" : qPrintable(result.error()));
        QCOMPARE(project.GetMainModel()->GetPieces().size(), size_t(1));
    }
};

int main(int argc, char** argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("LeoCAD-MemberB-Tests");
    QCoreApplication::setApplicationName("ModelIo");
    QTemporaryDir settings;
    if (!settings.isValid())
        return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settings.path());
    lcApplication app(argc, argv);
    // The unmodified upstream library releases buffers through its renderer.
    // Initialize it even though these tests do not display or validate a view.
    if (!lcContext::InitializeRenderer())
    {
        qCritical("Desktop OpenGL context is required by the upstream library lifecycle.");
        return 2;
    }
    int result;
    {
        lcPiecesLibrary library;
        app.mLibrary = &library;
        library.LoadColors();
        ModelIoTest tests;
        result = QTest::qExec(&tests, argc, argv);
    }
    app.mLibrary = nullptr;
    lcContext::ShutdownRenderer();
    return result;
}

#include "tst_model_io.moc"
