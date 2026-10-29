#include "lc_glcontext.h"
#include "lc_shaderprogram.h"
#include <QOpenGLFramebufferObject>
#include <QTemporaryDir>
#include <QTest>

class ShaderTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QString Error;
        QVERIFY2(lcCreateOffscreenGLContext(Context, Surface, nullptr, Error), qPrintable(Error));
        QCOMPARE(Context.isOpenGLES(), qgetenv("LEOCAD_TEST_GLES") == "1");
        GL = Context.functions();
        GL->initializeOpenGLFunctions();
        Prefix = lcShaderPrefix(Context.isOpenGLES());
    }

    void materials_data()
    {
        QTest::addColumn<int>("Index");
        for (int Index = 0; Index < int(std::size(lcShaderPrograms)); Index++)
            QTest::newRow(lcShaderPrograms[Index].Name) << Index;
    }

    void materials()
    {
        QFETCH(int, Index);
        const auto Built = lcBuildShaderProgram(*GL, lcShaderPrograms[Index], Prefix);
        QVERIFY2(Built.Object, qPrintable(Built.Error + ": " + Built.Log));
        GL->glDeleteProgram(Built.Object);
        QVERIFY(!GL->glIsProgram(Built.Object));
        QCOMPARE(GL->glGetError(), GLenum(GL_NO_ERROR));
    }

    void rejectsBrokenSource_data()
    {
        QTest::addColumn<QByteArray>("Vertex");
        QTest::addColumn<QByteArray>("Fragment");
        QTest::addColumn<QString>("Stage");
        QTest::addColumn<QString>("Error");
        const QByteArray VS = "LC_VERTEX_INPUT vec3 VertexPosition; LC_VERTEX_OUTPUT vec3 Payload; void main() { Payload=VertexPosition; gl_Position=vec4(VertexPosition,1.0); }";
        const QByteArray PS = "LC_PIXEL_INPUT vec3 Payload; LC_PIXEL_OUTPUT void main() { gl_FragColor=vec4(Payload,1.0); }";
        QTest::newRow("vertex-compile") << QByteArray("invalid syntax!") << PS << QString("vertex") << QString("shader_compile_failed");
        QTest::newRow("fragment-compile") << VS << QByteArray("invalid syntax!") << QString("fragment") << QString("shader_compile_failed");
        QTest::newRow("link-interface") << VS
            << QByteArray("LC_PIXEL_INPUT vec2 Payload; LC_PIXEL_OUTPUT void main() { gl_FragColor=vec4(Payload,0.0,1.0); }")
            << QString("link") << QString("shader_link_failed");
    }

    void rejectsBrokenSource()
    {
        QFETCH(QByteArray, Vertex);
        QFETCH(QByteArray, Fragment);
        QFETCH(QString, Stage);
        QFETCH(QString, Error);
        QTemporaryDir Dir;
        QVERIFY(Dir.isValid());
        QFile VS(Dir.filePath("vertex.glsl")), PS(Dir.filePath("fragment.glsl"));
        QVERIFY(VS.open(QIODevice::WriteOnly));
        QCOMPARE(VS.write(Vertex), qint64(Vertex.size())); VS.close();
        QVERIFY(PS.open(QIODevice::WriteOnly));
        QCOMPARE(PS.write(Fragment), qint64(Fragment.size())); PS.close();
        const QByteArray VP = VS.fileName().toUtf8(), FP = PS.fileName().toUtf8();
        const lcShaderProgramSpec Spec{"broken", VP.constData(), FP.constData(), false};
        // Repeated failures must remain clean and allow a good program afterwards.
        for (int Attempt = 0; Attempt < 3; Attempt++)
        {
            const auto Built = lcBuildShaderProgram(*GL, Spec, Prefix);
            QCOMPARE(Built.Object, GLuint(0));
            QCOMPARE(Built.Error, Error);
            QCOMPARE(Built.Stage, Stage);
            QVERIFY(!Built.Log.trimmed().isEmpty());
            QCOMPARE(GL->glGetError(), GLenum(GL_NO_ERROR));
        }
        const auto Good = lcBuildShaderProgram(*GL, lcShaderPrograms[0], Prefix);
        QVERIFY(Good.Object);
        GL->glDeleteProgram(Good.Object);
    }

    void missingResource_data()
    {
        QTest::addColumn<bool>("MissingVertex");
        QTest::newRow("vertex") << true;
        QTest::newRow("fragment") << false;
    }

    void missingResource()
    {
        QFETCH(bool, MissingVertex);
        auto Spec = lcShaderPrograms[0];
        if (MissingVertex) Spec.Vertex = ":/missing-vertex.glsl";
        else Spec.Fragment = ":/missing-fragment.glsl";
        const auto Built = lcBuildShaderProgram(*GL, Spec, Prefix);
        QCOMPARE(Built.Object, GLuint(0));
        QCOMPARE(Built.Error, QString("shader_resource_open_failed"));
        QCOMPARE(Built.Stage, QString(MissingVertex ? "vertex" : "fragment"));
        QVERIFY(!Built.Log.isEmpty());
        QCOMPARE(GL->glGetError(), GLenum(GL_NO_ERROR));
    }

    void basicMaterialDrawsPixels()
    {
        const auto Built = lcBuildShaderProgram(*GL, lcShaderPrograms[0], Prefix);
        QVERIFY(Built.Object);
        QOpenGLFramebufferObject FBO(16, 16);
        QVERIFY(FBO.isValid());
        QVERIFY(FBO.bind());
        GL->glViewport(0, 0, 16, 16);
        GL->glClearColor(0, 0, 0, 1);
        GL->glClear(GL_COLOR_BUFFER_BIT);
        GL->glUseProgram(Built.Object);
        const GLfloat Identity[] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        GL->glUniformMatrix4fv(GL->glGetUniformLocation(Built.Object, "WorldViewProjectionMatrix"), 1, GL_FALSE, Identity);
        GL->glUniform4f(GL->glGetUniformLocation(Built.Object, "MaterialColor"), 0, 1, 0, 1);
        const GLfloat Vertices[] = {-1,-1,0, 1,-1,0, 0,1,0};
        GLuint VBO = 0;
        GL->glGenBuffers(1, &VBO);
        GL->glBindBuffer(GL_ARRAY_BUFFER, VBO);
        GL->glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);
        GL->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        GL->glEnableVertexAttribArray(0);
        GL->glDrawArrays(GL_TRIANGLES, 0, 3);
        GLubyte Pixel[4] = {};
        GL->glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, Pixel);
        GL->glDisableVertexAttribArray(0);
        GL->glBindBuffer(GL_ARRAY_BUFFER, 0);
        GL->glDeleteBuffers(1, &VBO);
        GL->glUseProgram(0);
        GL->glDeleteProgram(Built.Object);
        FBO.release();
        QCOMPARE(GL->glGetError(), GLenum(GL_NO_ERROR));
        QVERIFY(Pixel[0] < 5 && Pixel[1] > 250 && Pixel[2] < 5 && Pixel[3] > 250);
    }

    void cleanupTestCase() { Context.doneCurrent(); }
private:
    QOpenGLContext Context;
    QOffscreenSurface Surface;
    QOpenGLFunctions* GL = nullptr;
    QByteArray Prefix;
};

int main(int argc, char** argv)
{
    QSurfaceFormat Format;
    const bool GLES = qgetenv("LEOCAD_TEST_GLES") == "1";
    Format.setRenderableType(GLES ? QSurfaceFormat::OpenGLES : QSurfaceFormat::OpenGL);
    Format.setVersion(GLES ? 3 : 2, GLES ? 0 : 1);
    QSurfaceFormat::setDefaultFormat(Format);
    QGuiApplication App(argc, argv);
    ShaderTests Tests;
    return QTest::qExec(&Tests, argc, argv);
}
#include "tst_shaders.moc"
