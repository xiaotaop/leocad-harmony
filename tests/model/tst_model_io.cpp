#include "lc_global.h"
#include <QtTest>
#include "lc_application.h"
#include "lc_context.h"
#include "lc_library.h"
#include "lc_model.h"
#include "piece.h"
#include "project.h"

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
