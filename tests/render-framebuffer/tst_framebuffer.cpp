#include "lc_glframebuffer.h"
#include "lc_shaderprogram.h"
#include <QApplication>
#include <QOpenGLFramebufferObject>
#include <QOpenGLWidget>
#include <QTest>

class DepthWidget : public QOpenGLWidget
{
public:
    bool Repair = true;
    bool Leak = true;
    bool Translucent = false;
    bool GridFirst = true;
    bool GridInFront = false;
    int Frames = 0;
    GLenum Error = GL_NO_ERROR;
    QString BuildError;
    QJsonObject Before, After;
    QOpenGLFunctions* GL = nullptr;
    ~DepthWidget() override
    {
        if (!context() || !context()->isValid()) return;
        makeCurrent();
        GL->glDeleteProgram(Solid);
        GL->glDeleteProgram(Textured);
        GL->glDeleteBuffers(1, &VBO);
        GL->glDeleteTextures(1, &Texture);
        doneCurrent();
    }
protected:
    void initializeGL() override
    {
        GL = context()->functions();
        GL->initializeOpenGLFunctions();
        const QByteArray Prefix = lcShaderPrefix(context()->isOpenGLES());
        const auto A = lcBuildShaderProgram(*GL, lcShaderPrograms[0], Prefix);
        const auto B = lcBuildShaderProgram(*GL, lcShaderPrograms[2], Prefix);
        Solid = A.Object; Textured = B.Object;
        BuildError = A.Error + A.Log + B.Error + B.Log;
        GL->glGenBuffers(1, &VBO);
        GL->glGenTextures(1, &Texture);
        GL->glBindTexture(GL_TEXTURE_2D, Texture);
        const GLubyte Texel[] = {255,255,255,128};
        GL->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, Texel);
        GL->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        GL->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    void quad(GLuint Program, float Extent, float Z, float Red, float Green, float Alpha)
    {
        const GLfloat Vertices[] = {-Extent,-Extent,Z,0,0, Extent,-Extent,Z,1,0,
            -Extent,Extent,Z,0,1, Extent,Extent,Z,1,1};
        const GLfloat Identity[] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        GL->glUseProgram(Program);
        GL->glUniformMatrix4fv(GL->glGetUniformLocation(Program, "WorldViewProjectionMatrix"), 1, GL_FALSE, Identity);
        GL->glUniform4f(GL->glGetUniformLocation(Program, "MaterialColor"), Red, Green, 0, Alpha);
        GL->glBindBuffer(GL_ARRAY_BUFFER, VBO);
        GL->glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);
        GL->glEnableVertexAttribArray(0);
        GL->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), nullptr);
        if (Program == Textured)
        {
            GL->glActiveTexture(GL_TEXTURE0);
            GL->glBindTexture(GL_TEXTURE_2D, Texture);
            GL->glUniform1i(GL->glGetUniformLocation(Program, "Texture"), 0);
            GL->glEnableVertexAttribArray(2);
            GL->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), reinterpret_cast<void*>(3*sizeof(GLfloat)));
        }
        GL->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        GL->glDisableVertexAttribArray(2);
    }
    void paintGL() override
    {
        if (!Solid || !Textured) return;
        if (Leak)
        {
            GL->glDepthRangef(0, 0);
            GL->glClearDepthf(0);
            GL->glDepthMask(GL_FALSE);
            GL->glDepthFunc(GL_ALWAYS);
            GL->glDisable(GL_DEPTH_TEST);
        }
        Before = lcGLFramebufferInfo(*GL);
        if (Repair) lcResetFrameDepthState(*GL);
        else
        {
            // Old SetDefaultState: misses the depth mapping and clear value.
            GL->glEnable(GL_DEPTH_TEST);
            GL->glDepthFunc(GL_LEQUAL);
            GL->glDepthMask(GL_TRUE);
        }
        After = lcGLFramebufferInfo(*GL);
        GL->glViewport(0, 0, qRound(width()*devicePixelRatioF()), qRound(height()*devicePixelRatioF()));
        GL->glClearColor(0,0,0,1);
        GL->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        GL->glDisable(GL_CULL_FACE);
        GL->glDisable(GL_BLEND);
        GL->glDisable(GL_POLYGON_OFFSET_FILL);
        GL->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        const auto Grid = [&]() {
            GL->glEnable(GL_BLEND);
            quad(Textured, 1, GridInFront ? -.75f : .5f, 0, 1, 1);
            GL->glDisable(GL_BLEND);
        };
        if (GridFirst) lcDrawReferenceGrid(*GL, Grid);
        if (!Translucent) quad(Solid, .45f, -.5f, 1, 0, 1);
        if (!GridFirst)
        {
            GL->glDepthMask(GL_FALSE);
            Grid();
            GL->glDepthMask(GL_TRUE);
        }
        if (Translucent)
        {
            GL->glEnable(GL_BLEND);
            GL->glDepthMask(GL_FALSE);
            quad(Solid, .45f, -.5f, 1, 0, .5f);
        }
        GL->glDepthMask(GL_TRUE);
        GL->glDisable(GL_BLEND);
        GL->glDisableVertexAttribArray(0);
        GL->glBindBuffer(GL_ARRAY_BUFFER, 0);
        GL->glUseProgram(0);
        Error = GL->glGetError();
        Frames++;
    }
private:
    GLuint Solid = 0, Textured = 0, VBO = 0, Texture = 0;
};

class FramebufferTests : public QObject
{
    Q_OBJECT
private:
    bool ready(DepthWidget& W)
    {
        W.resize(160,120); W.show();
        return QTest::qWaitForWindowExposed(&W) && W.isValid();
    }
    QColor center(const QImage& I) { return I.pixelColor(I.width()/2,I.height()/2); }
private slots:
    void leakedStateReproducesGridOverSolid()
    {
        DepthWidget W; W.Repair = false; W.GridFirst = false;
        QVERIFY(ready(W));
        const QImage I = W.grabFramebuffer();
        QVERIFY2(W.BuildError.isEmpty(), qPrintable(W.BuildError));
        QVERIFY(W.After["passed"].toBool());
        const QColor C = center(I);
        QVERIFY2(C.green()>100 && C.red()<180, qPrintable(C.name()));
        QCOMPARE(W.Error, GLenum(GL_NO_ERROR));
    }
    void gridPlaneIntersectingModelIsReferenceOnly()
    {
        DepthWidget Old; Old.Leak=false; Old.GridFirst=false; Old.GridInFront=true;
        QVERIFY(ready(Old));
        const QColor Bad=center(Old.grabFramebuffer());
        QVERIFY(Bad.green()>100 && Bad.red()<180);
        DepthWidget Fixed; Fixed.Leak=false; Fixed.GridInFront=true;
        QVERIFY(ready(Fixed));
        const QImage I=Fixed.grabFramebuffer();
        const QColor Good=center(I), Grid=I.pixelColor(I.width()/10,I.height()/10);
        QVERIFY(Good.red()>250 && Good.green()<5);
        QVERIFY(Grid.green()>120 && Grid.green()<140);
        QCOMPARE(Fixed.Error,GLenum(GL_NO_ERROR));
    }
    void referenceGridRestoresDepthWriteMask()
    {
        DepthWidget W; QVERIFY(ready(W)); W.grabFramebuffer(); W.makeCurrent();
        for (bool Original : {false,true})
        {
            W.GL->glDepthMask(Original?GL_TRUE:GL_FALSE);
            bool Disabled=false;
            lcDrawReferenceGrid(*W.GL,[&]() {
                GLboolean Mask=GL_TRUE; W.GL->glGetBooleanv(GL_DEPTH_WRITEMASK,&Mask);
                Disabled=Mask==GL_FALSE;
            });
            GLboolean Mask=GL_FALSE; W.GL->glGetBooleanv(GL_DEPTH_WRITEMASK,&Mask);
            QVERIFY(Disabled); QCOMPARE(bool(Mask),Original);
        }
        W.doneCurrent();
    }
    void restoresDepthAndOcclusionAcrossResize()
    {
        DepthWidget W;
        QVERIFY(ready(W));
        QCOMPARE(W.context()->isOpenGLES(), qgetenv("LEOCAD_TEST_GLES")=="1");
        for (const QSize S : {QSize(160,120),QSize(240,180),QSize(160,120)})
        {
            W.resize(S); QTest::qWait(50);
            const QImage I = W.grabFramebuffer();
            QVERIFY2(W.BuildError.isEmpty(), qPrintable(W.BuildError));
            QVERIFY(W.After["passed"].toBool());
            QCOMPARE(W.After["depth_range_near"].toDouble(),0.0);
            QCOMPARE(W.After["depth_range_far"].toDouble(),1.0);
            QCOMPARE(W.After["clear_depth"].toDouble(),1.0);
            QVERIFY(W.After["depth_write"].toBool() && W.After["depth_test"].toBool());
            QCOMPARE(W.After["depth_func"].toInt(),int(GL_LEQUAL));
            const QColor C = center(I), Grid = I.pixelColor(I.width()/10,I.height()/10);
            QVERIFY2(C.red()>250 && C.green()<5, qPrintable(C.name()));
            QVERIFY2(Grid.green()>120 && Grid.green()<140 && Grid.red()<5,qPrintable(Grid.name()));
            QCOMPARE(I.size(), QSize(qRound(W.width()*W.devicePixelRatioF()),qRound(W.height()*W.devicePixelRatioF())));
            QCOMPARE(W.Error, GLenum(GL_NO_ERROR));
        }
    }
    void translucentForegroundKeepsTexturedBackground()
    {
        DepthWidget W; W.Translucent = true;
        QVERIFY(ready(W));
        const QColor C = center(W.grabFramebuffer());
        QVERIFY2(C.red()>120 && C.red()<140 && C.green()>55 && C.green()<75,qPrintable(C.name()));
        QCOMPARE(W.Error, GLenum(GL_NO_ERROR));
    }
    void detectsFboWithoutDepthAndRestoresWidgetBinding()
    {
        DepthWidget W; QVERIFY(ready(W)); W.grabFramebuffer(); W.makeCurrent();
        const GLuint Target = W.defaultFramebufferObject();
        {
            QOpenGLFramebufferObject FBO(32,32,QOpenGLFramebufferObject::NoAttachment);
            QVERIFY(FBO.isValid() && FBO.bind());
            const auto Info = lcGLFramebufferInfo(*W.GL);
            QCOMPARE(Info["depth_bits"].toInt(),0);
            QVERIFY(!Info["passed"].toBool());
            W.GL->glBindFramebuffer(GL_FRAMEBUFFER,Target);
            QVERIFY(lcGLFramebufferInfo(*W.GL)["passed"].toBool());
        }
        W.doneCurrent();
        const QColor C = center(W.grabFramebuffer());
        QVERIFY(C.red()>250 && C.green()<5);
    }
    void recreatedWidgetRetainsDepth()
    {
        for(int Round=0;Round<2;Round++)
        {
            DepthWidget W; QVERIFY(ready(W));
            const QColor C=center(W.grabFramebuffer());
            QVERIFY(W.After["passed"].toBool());
            QVERIFY(C.red()>250 && C.green()<5);
            QCOMPARE(W.Error,GLenum(GL_NO_ERROR));
        }
    }
};
int main(int argc,char** argv)
{
    const bool GLES=qgetenv("LEOCAD_TEST_GLES")=="1";
    QSurfaceFormat Format; Format.setRenderableType(GLES?QSurfaceFormat::OpenGLES:QSurfaceFormat::OpenGL);
    Format.setVersion(GLES?3:2,GLES?0:1); Format.setDepthBufferSize(24); Format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(Format);
    QApplication App(argc,argv); FramebufferTests Tests; return QTest::qExec(&Tests,argc,argv);
}
#include "tst_framebuffer.moc"
