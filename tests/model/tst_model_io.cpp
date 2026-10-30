#include "lc_global.h"
#include <QtTest>
#include <QScopeGuard>
#include "lc_application.h"
#include "lc_context.h"
#include "lc_scene.h"
#ifndef Q_OS_HARMONY
#include <QOpenGLFramebufferObject>
#endif
#include "lc_library.h"
#include "lc_mainwindow.h"
#include "lc_model.h"
#include "lc_modelhistory.h"
#include "lc_http.h"
#include "piece.h"
#include "pieceinf.h"
#include "project.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#ifdef Q_OS_HARMONY
// Host source paths are unavailable in the device sandbox. The test HAP embeds
// the same fixtures and extracts writable copies, matching desktop input modes.
static QString gHarmonyFixtureDirectory;
#undef QFINDTESTDATA
#define QFINDTESTDATA(path) (gHarmonyFixtureDirectory + QLatin1Char('/') + QFileInfo(QString::fromUtf8(path)).fileName())
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
#ifndef Q_OS_HARMONY
    // Reuse this host suite's real production renderer/library lifecycle. The
    // standalone quad tests do not exercise lcScene's state changes or cache.
    void referenceGridPreservesModelLineWidth_data()
    {
        QTest::addColumn<float>("Width");
        QTest::addColumn<int>("Mode");
        for (const auto Mode : {lcShadingMode::Flat, lcShadingMode::DefaultLights, lcShadingMode::Wireframe})
            for (float Width : {1.0f, 2.0f, 3.0f})
                QTest::newRow(qPrintable(QString("mode%1-width%2").arg(int(Mode)).arg(Width))) << Width << int(Mode);
    }

    void referenceGridPreservesModelLineWidth()
    {
        QFETCH(float, Width);
        QFETCH(int, Mode);
        lcContext* Context = lcContext::GetGlobalOffscreenContext();
        QVERIFY(Context);
        Context->MakeCurrent();
        GLint PreviousFBO = 0;
        Context->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &PreviousFBO);
        auto& Preferences = lcGetPreferences();
        const float SavedWidth = Preferences.mLineWidth;
        const bool SavedEdges = Preferences.mDrawEdgeLines, SavedConditional = Preferences.mDrawConditionalLines;
        auto Restore = qScopeGuard([&]() {
            Preferences.mLineWidth = SavedWidth;
            Preferences.mDrawEdgeLines = SavedEdges;
            Preferences.mDrawConditionalLines = SavedConditional;
            Context->glBindFramebuffer(GL_FRAMEBUFFER, GLuint(PreviousFBO));
            Context->SetDefaultState();
            Context->SetLineWidth(SavedWidth);
        });
        Preferences.mLineWidth = Width;
        Preferences.mDrawEdgeLines = true;
        Preferences.mDrawConditionalLines = false;
        QOpenGLFramebufferObject FBO(64, 64, QOpenGLFramebufferObject::CombinedDepthStencil);
        QVERIFY(FBO.isValid());
        lcMesh Mesh;
        Mesh.CreateBox();
        for (bool Grid : {false, true})
        {
            QVERIFY(FBO.bind());
            Context->SetDefaultState();
            Context->SetViewport(0, 0, 64, 64);
            Context->SetProjectionMatrix(lcMatrix44Ortho(-20, 20, -20, 20, -100, 100));
            Context->ClearColorAndDepth(lcVector4(0, 0, 0, 1));
            Context->SetLineWidth(Width);
            lcScene Scene;
            Scene.SetShadingMode(static_cast<lcShadingMode>(Mode));
            Scene.SetAllowLOD(false);
            Scene.Begin(lcMatrix44Identity());
            if (Grid)
                Scene.SetBackgroundCallback([&]() { Context->SetLineWidth(1.0f); });
            Scene.AddMesh(&Mesh, lcMatrix44Identity(), lcGetColorIndex(4), lcRenderMeshState::Default);
            Scene.End();
            Scene.Draw(Context); // Actual opaque/wireframe production draw path.
            GLfloat Actual = 0;
            Context->glGetFloatv(GL_LINE_WIDTH, &Actual);
            QCOMPARE(Actual, Width);
            if (!Grid)
            {
                // Callers without a background (e.g. thumbnails) retain their
                // explicit width even when it differs from the user preference.
                Context->SetLineWidth(Width + 1.0f);
                Scene.Draw(Context);
                Context->glGetFloatv(GL_LINE_WIDTH, &Actual);
                QCOMPARE(Actual, Width + 1.0f);
                Context->SetLineWidth(Width);
            }
            // Verify the lcContext cache agrees with GL after scene drawing too.
            Context->SetLineWidth(1.0f);
            Context->glGetFloatv(GL_LINE_WIDTH, &Actual);
            QCOMPARE(Actual, 1.0f);
            Context->SetLineWidth(Width);
            Context->glGetFloatv(GL_LINE_WIDTH, &Actual);
            QCOMPARE(Actual, Width);
            QCOMPARE(Context->glGetError(), GLenum(GL_NO_ERROR));
        }
    }
#endif

    void disabledExportsPreserveFilesAndDocument_data()
    {
        QTest::addColumn<QString>("format");
        for (const char* format : {"3ds", "dae", "csv", "pov", "obj"})
            QTest::newRow(format) << QString::fromLatin1(format);
    }

    void disabledExportsPreserveFilesAndDocument()
    {
        QFETCH(QString, format);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Project project(true);
        QVERIFY(project.Load(QFINDTESTDATA("../fixtures/two-pieces.ldr"), false));
        project.MarkAsModified();
        const QString originalName = project.GetFileName();
        lcModel* originalModel = project.GetActiveModel();
        const QString path = dir.filePath("existing." + format);
        QFile output(path);
        QVERIFY(output.open(QIODevice::WriteOnly));
        QCOMPARE(output.write("keep existing data"), qint64(18));
        output.close();
        const auto result = format == "3ds" ? project.Export3DStudio(path)
            : format == "dae" ? project.ExportCOLLADA(path)
            : format == "csv" ? project.ExportCSV(path)
            : format == "pov" ? project.ExportPOVRay(path)
            : project.ExportWavefront(path);
        QVERIFY(!result.has_value());
        QCOMPARE(result.error(), QString("This format is disabled in this build."));
        QVERIFY(output.open(QIODevice::ReadOnly));
        QCOMPARE(output.readAll(), QByteArray("keep existing data"));
        QCOMPARE(project.GetFileName(), originalName);
        QCOMPARE(project.GetActiveModel(), originalModel);
        QCOMPARE(project.GetMainModel()->GetPieces().size(), size_t(2));
        QVERIFY(project.IsModified());
    }

    void offlineDownloadCompletesWithError()
    {
        lcHttpManager manager(nullptr);
        QSignalSpy finished(&manager, &lcHttpManager::DownloadFinished);
        lcHttpReply* reply = manager.DownloadFile("http://127.0.0.1:1/member-b-offline-check");
        QVERIFY(reply);
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(qvariant_cast<lcHttpReply*>(finished.at(0).at(0)), reply);
        QVERIFY(reply->error());
        QVERIFY(reply->readAll().isEmpty());
    }

    void disabledLegacyLoadPreservesDocument_data()
    {
        QTest::addColumn<QString>("name");
        QTest::newRow("lcd") << QString("legacy.LCD");
        QTest::newRow("leocad") << QString("legacy.leocad");
        QTest::newRow("sniffed-binary") << QString("imported.bin");
    }

    void disabledLegacyLoadPreservesDocument()
    {
        QFETCH(QString, name);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // A recognizable legacy header must be rejected before binary parsing.
        QFile input(dir.filePath(name));
        QVERIFY(input.open(QIODevice::WriteOnly));
        const QByteArray header = QByteArray("LeoCAD ").leftJustified(32, '\0');
        QCOMPARE(input.write(header), qint64(header.size()));
        input.close();
        Project project(true);
        QVERIFY(project.Load(QFINDTESTDATA("../fixtures/two-pieces.ldr"), false));
        project.MarkAsModified();
        const QString originalName = project.GetFileName();
        lcModel* originalModel = project.GetActiveModel();
        const auto result = project.Load(input.fileName());
        QVERIFY(!result.has_value());
        QCOMPARE(result.error(), QString("This format is disabled in this build."));
        QCOMPARE(project.GetFileName(), originalName);
        QCOMPARE(project.GetActiveModel(), originalModel);
        QCOMPARE(project.GetMainModel()->GetPieces().size(), size_t(2));
        QVERIFY(project.IsModified());
    }

    void disabledLddImportPreservesDocument()
    {
        Project project(true);
        QVERIFY(project.Load(QFINDTESTDATA("../fixtures/two-pieces.ldr"), false));
        project.MarkAsModified();
        const QString originalName = project.GetFileName();
        lcModel* originalModel = project.GetActiveModel();
        QVERIFY(!project.ImportLDD(QFINDTESTDATA("../fixtures/empty-model.lxf")));
        QCOMPARE(project.GetFileName(), originalName);
        QCOMPARE(project.GetActiveModel(), originalModel);
        QCOMPARE(project.GetMainModel()->GetPieces().size(), size_t(2));
        QVERIFY(project.IsModified());
    }

    void disabledHtmlExportDoesNotCreateDirectory()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Project project(true);
        project.MarkAsModified();
        lcHTMLExportOptions options(&project);
        options.PathName = dir.filePath("instructions");
        const auto result = project.ExportHTML(options);
        QVERIFY(!result.has_value());
        QCOMPARE(result.error(), QString("This format is disabled in this build."));
        QVERIFY(!QFile::exists(options.PathName));
        QVERIFY(project.IsModified());
    }

    void disabledBrickLinkExportReturnsError()
    {
        Project project(true);
        const auto result = project.ExportBrickLink();
        QVERIFY(!result.has_value());
        QCOMPARE(result.error(), QString("This format is disabled in this build."));
        QVERIFY(!project.IsModified());
    }

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
        QTest::newRow("self-referencing-submodel") << QByteArray("0 FILE self.ldr\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 SELF.LDR\n");
        QTest::newRow("mutually-referencing-submodels") << QByteArray("0 FILE root.ldr\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 child.ldr\n0 FILE child.ldr\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 ROOT.LDR\n");
        QTest::newRow("unused-submodel-cycle") << QByteArray("0 FILE root.ldr\n0 Empty root\n0 FILE a.ldr\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 b.ldr\n0 FILE b.ldr\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 a.ldr\n");
        QTest::newRow("truncated-synth-control-point") << QByteArray("0 Broken\n0 !LEOCAD SYNTH CONTROL_POINT 0 0\n");
        QTest::newRow("nonfinite-synth-control-point") << QByteArray("0 Broken\n0 !LEOCAD SYNTH CONTROL_POINT 0 0 0 1 0 0 0 1 0 0 0 1 nan\n");
        QTest::newRow("truncated-piece-pivot") << QByteArray("0 Broken\n0 !LEOCAD PIECE PIVOT 0 0\n");
        QTest::newRow("nonfinite-piece-pivot") << QByteArray("0 Broken\n0 !LEOCAD PIECE PIVOT 0 0 0 1 0 0 0 1 0 0 0 inf\n");
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

    void sharedSubmodelRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile input(dir.filePath("shared.mpd"));
        QVERIFY(input.open(QIODevice::WriteOnly));
        const QByteArray contents("0 FILE root.ldr\n"
            "1 4 0 0 0 1 0 0 0 1 0 0 0 1 left.ldr\n"
            "1 1 20 0 0 1 0 0 0 1 0 0 0 1 right.ldr\n"
            "0 FILE left.ldr\n1 16 0 0 0 1 0 0 0 1 0 0 0 1 shared.ldr\n"
            "0 FILE right.ldr\n1 16 0 0 0 1 0 0 0 1 0 0 0 1 shared.ldr\n"
            "0 FILE shared.ldr\n1 14 0 0 0 1 0 0 0 1 0 0 0 1 b_fixture_brick.dat\n");
        QCOMPARE(input.write(contents), qint64(contents.size()));
        input.close();
        Project project(true);
        QVERIFY(project.Load(input.fileName()).has_value());
        const QString output = dir.filePath("reopened.mpd");
        QVERIFY(project.Save(output).has_value());
        Project reopened(true);
        QVERIFY(reopened.Load(output).has_value());
        QCOMPARE(reopened.GetModels().size(), size_t(4));
        QCOMPARE(reopened.GetMainModel()->GetPieces().size(), size_t(2));
        lcModel* shared = reopened.GetModel("shared.ldr");
        QVERIFY(shared);
        QCOMPARE(shared->GetPieces().size(), size_t(1));
        QCOMPARE(shared->GetPieces()[0]->GetColorCode(), quint32(14));
        for (const QString& name : {QString("left.ldr"), QString("right.ldr")})
        {
            lcModel* parent = reopened.GetModel(name);
            QVERIFY(parent);
            QCOMPARE(parent->GetPieces().size(), size_t(1));
            QCOMPARE(parent->GetPieces()[0]->mPieceInfo->GetModel(), shared);
        }
    }

    void relativeDocumentPathsSurviveWorkingDirectoryChange_data()
    {
        QTest::addColumn<bool>("loadFirst");
        QTest::newRow("load-relative-path") << true;
        QTest::newRow("save-relative-path") << false;
    }

    void relativeDocumentPathsSurviveWorkingDirectoryChange()
    {
        QFETCH(bool, loadFirst);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString documents = dir.filePath(QString::fromUtf8("沙箱 Documents"));
        const QString cache = dir.filePath("cache");
        QVERIFY(QDir().mkpath(documents));
        QVERIFY(QDir().mkpath(cache));
        const QString expectedPath = documents + "/model.ldr";
        QVERIFY(QFile::copy(QFINDTESTDATA("../fixtures/two-pieces.ldr"), expectedPath));
        const QString previousDirectory = QDir::currentPath();
        const auto restoreDirectory = qScopeGuard([&]() { QDir::setCurrent(previousDirectory); });
        QVERIFY(QDir::setCurrent(documents));
        Project project(true);
        if (loadFirst)
            QVERIFY(project.Load("model.ldr").has_value());
        else
        {
            QVERIFY(project.Load(expectedPath).has_value());
            QVERIFY(project.Save("model.ldr").has_value());
        }
        QCOMPARE(project.GetFileName(), expectedPath);
        QVERIFY(QDir::setCurrent(cache));
        project.MarkAsModified();
        QVERIFY(project.Save(project.GetFileName()).has_value());
        QVERIFY(!project.IsModified());
        QVERIFY(!QFile::exists(cache + "/model.ldr"));
        Project reopened(true);
        QVERIFY(reopened.Load(project.GetFileName()).has_value());
        QCOMPARE(reopened.GetMainModel()->GetPieces().size(), size_t(2));
    }

    void compatibleLDrawSyntax_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::newRow("bom-and-direct-color") << QByteArray("\xef\xbb\xbf" "0 Valid\n1 0x2FF0000 0 0 0 1 0 0 0 1 0 0 0 1 b_fixture_brick.dat\n");
        QTest::newRow("unknown-meta-and-line") << QByteArray("0 !OTHER_VENDOR keep this\n9 ignored\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 b_fixture_brick.dat\n");
        QTest::newRow("mpd-trailing-signature") << QByteArray("0 FILE main.ldr\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 b_fixture_brick.dat\n0 NOFILE\n1 incomplete text outside a block\n");
        QTest::newRow("piece-pivot") << QByteArray("0 Valid\n0 !LEOCAD PIECE PIVOT 1 2 3 1 0 0 0 1 0 0 0 1 HIDDEN\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 b_fixture_brick.dat\n");
        QTest::newRow("synth-control-point") << QByteArray("0 Valid\n0 !LEOCAD SYNTH BEGIN\n0 !LEOCAD SYNTH CONTROL_POINT 0 0 0 1 0 0 0 1 0 0 0 1 1\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 b_fixture_brick.dat\n0 !LEOCAD SYNTH END\n");
        QTest::newRow("ignored-meta-after-nofile") << QByteArray("0 FILE main.ldr\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 b_fixture_brick.dat\n0 NOFILE\n0 !LEOCAD PIECE PIVOT 1\n");
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
#ifdef Q_OS_HARMONY
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(3, 0);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);
#endif
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
#ifdef Q_OS_HARMONY
    QTemporaryDir fixtureDirectory;
    if (!fixtureDirectory.isValid())
        return 2;
    gHarmonyFixtureDirectory = fixtureDirectory.path();
    for (const char* name : {"two-pieces.ldr", "submodel.mpd", "empty-model.lxf"})
    {
        QFile source(QStringLiteral(":/fixtures/") + QString::fromLatin1(name));
        QFile destination(fixtureDirectory.filePath(QString::fromLatin1(name)));
        if (!source.open(QIODevice::ReadOnly) || !destination.open(QIODevice::WriteOnly))
            return 2;
        const QByteArray contents = source.readAll();
        if (destination.write(contents) != contents.size())
            return 2;
    }
#endif
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
#ifdef Q_OS_HARMONY
        const QString outputDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (!QDir().mkpath(outputDirectory))
            return 2;
        const QString output = outputDirectory + QStringLiteral("/model-tests.txt");
        result = QTest::qExec(&tests, QStringList{QStringLiteral("leocad-model-tests"),
            QStringLiteral("-o"), output + QStringLiteral(",txt")});
        qInfo().noquote() << "LEOCAD_MODEL_TESTS" << output << "exit" << result;
#else
        result = QTest::qExec(&tests, argc, argv);
#endif
    }
    app.mLibrary = nullptr;
    lcContext::ShutdownRenderer();
    return result;
}

#include "tst_model_io.moc"
