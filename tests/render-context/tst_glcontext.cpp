#include "lc_glcontext.h"
#include <QTest>
#include <memory>

class RenderContextTests : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase()
	{
		QString Error;
		QVERIFY2(lcCreateOffscreenGLContext(mContext, mSurface, nullptr, Error), qPrintable(Error));
		mContext.doneCurrent();
	}

	void apiCompatibility_data()
	{
		QTest::addColumn<bool>("GLESBuild");
		QTest::addColumn<bool>("ActualGLES");
		QTest::addColumn<int>("MajorVersion");
		QTest::addColumn<QString>("Error");
		QTest::newRow("desktop-compatibility") << false << false << 2 << QString();
		QTest::newRow("desktop-modern") << false << false << 4 << QString();
		QTest::newRow("gles3") << true << true << 3 << QString();
		QTest::newRow("gles2-rejected") << true << true << 2 << QString("renderer_requires_gles3");
		QTest::newRow("desktop-build-gles-context") << false << true << 3 << QString("renderer_context_api_mismatch");
		QTest::newRow("gles-build-desktop-context") << true << false << 4 << QString("renderer_context_api_mismatch");
	}

	void apiCompatibility()
	{
		QFETCH(bool, GLESBuild);
		QFETCH(bool, ActualGLES);
		QFETCH(int, MajorVersion);
		QFETCH(QString, Error);
		QCOMPARE(lcGLAPICompatibilityError(GLESBuild, ActualGLES, MajorVersion), Error);
	}

	void sharedTextureReadbackAndActualFormat()
	{
		QVERIFY(mContext.makeCurrent(&mSurface));
		QOpenGLFunctions* ParentFunctions = mContext.functions();
		ParentFunctions->initializeOpenGLFunctions();
		GLuint Texture = 0;
		const GLubyte Texel[] = {21, 87, 143, 255};
		ParentFunctions->glGenTextures(1, &Texture);
		ParentFunctions->glBindTexture(GL_TEXTURE_2D, Texture);
		ParentFunctions->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, Texel);
		// This test checks sharing, not asynchronous upload scheduling.
		ParentFunctions->glFinish();
		QCOMPARE(ParentFunctions->glGetError(), GLenum(GL_NO_ERROR));
		mContext.doneCurrent();

		QOffscreenSurface Surface;
		QOpenGLContext Context;
		// A stale/requested format must not leak into the new shared Surface.
		QSurfaceFormat WrongFormat;
		WrongFormat.setRenderableType(mContext.isOpenGLES() ? QSurfaceFormat::OpenGL : QSurfaceFormat::OpenGLES);
		WrongFormat.setVersion(9, 9);
		WrongFormat.setSamples(64);
		Context.setFormat(WrongFormat);
		Surface.setFormat(WrongFormat);
		QString Error;
		QVERIFY2(lcCreateOffscreenGLContext(Context, Surface, &mContext, Error), qPrintable(Error));
		QCOMPARE(Surface.requestedFormat(), Context.format());
		QCOMPARE(Context.screen(), mContext.screen());
		QCOMPARE(Surface.screen(), Context.screen());
		QVERIFY(QOpenGLContext::areSharing(&Context, &mContext));
		QCOMPARE(QOpenGLContext::currentContext(), &Context);

		QOpenGLFunctions* Functions = Context.functions();
		Functions->initializeOpenGLFunctions();
		const bool TextureVisible = Functions->glIsTexture(Texture);
		GLuint FBO = 0;
		Functions->glGenFramebuffers(1, &FBO);
		Functions->glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		Functions->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Texture, 0);
		const GLenum Status = Functions->glCheckFramebufferStatus(GL_FRAMEBUFFER);
		GLubyte Pixel[4] = {};
		if (Status == GL_FRAMEBUFFER_COMPLETE)
			Functions->glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, Pixel);
		const GLenum GLError = Functions->glGetError();
		Functions->glBindFramebuffer(GL_FRAMEBUFFER, Context.defaultFramebufferObject());
		Functions->glDeleteFramebuffers(1, &FBO);
		Context.doneCurrent();
		QVERIFY(mContext.makeCurrent(&mSurface));
		ParentFunctions->glDeleteTextures(1, &Texture);
		mContext.doneCurrent();

		QVERIFY(TextureVisible);
		QCOMPARE(Status, GLenum(GL_FRAMEBUFFER_COMPLETE));
		QCOMPARE(GLError, GLenum(GL_NO_ERROR));
		for (int Index = 0; Index < 4; Index++)
			QCOMPARE(Pixel[Index], Texel[Index]);
	}

	void rejectsInvalidShareContext()
	{
		QOffscreenSurface Surface;
		QOpenGLContext Context;
		QOpenGLContext InvalidShare;
		QString Error;
		QVERIFY(!lcCreateOffscreenGLContext(Context, Surface, &InvalidShare, Error));
		QCOMPARE(Error, QString("share_context_invalid"));
		QVERIFY(!Context.isValid());
		QVERIFY(!Surface.isValid());
	}

	void rejectsExistingObjects()
	{
		QString Error;
		QVERIFY(!lcCreateOffscreenGLContext(mContext, mSurface, nullptr, Error));
		QCOMPARE(Error, QString("offscreen_objects_already_created"));
		QVERIFY(mContext.isValid());
		QVERIFY(mSurface.isValid());
	}

	void rejectsWorkerThreadBeforeNativeCalls()
	{
		QOffscreenSurface Surface;
		QOpenGLContext Context;
		QString Error;
		bool Created = true;
		std::unique_ptr<QThread> Worker(QThread::create([&]()
		{
			Created = lcCreateOffscreenGLContext(Context, Surface, &mContext, Error);
		}));
		Worker->start();
		QVERIFY(Worker->wait(5000));
		QVERIFY(!Created);
		QCOMPARE(Error, QString("offscreen_setup_requires_gui_thread"));
		QVERIFY(!Context.isValid());
		QVERIFY(!Surface.isValid());
	}

	void diagnosticsOnlyQueryCurrentContext()
	{
		QVERIFY(QOpenGLContext::currentContext() != &mContext);
		const QJsonObject NotCurrent = lcGLContextInfo(mContext, &mSurface, nullptr);
		QVERIFY(NotCurrent["valid"].toBool());
		QVERIFY(!NotCurrent["is_current"].toBool());
		QVERIFY(!NotCurrent.contains("gl_version"));
		QVERIFY(!NotCurrent.contains("renderer"));
		QVERIFY(mContext.makeCurrent(&mSurface));
		const QJsonObject Current = lcGLContextInfo(mContext, &mSurface, nullptr);
		QVERIFY(Current["is_current"].toBool());
		QVERIFY(!Current["gl_version"].toString().isEmpty());
		QVERIFY(!Current["renderer"].toString().isEmpty());
		QCOMPARE(mContext.functions()->glGetError(), GLenum(GL_NO_ERROR));
		mContext.doneCurrent();
	}

private:
	QOffscreenSurface mSurface;
	QOpenGLContext mContext;
};

int main(int argc, char* argv[])
{
	QSurfaceFormat Format;
#ifdef QT_OPENGL_ES
	Format.setRenderableType(QSurfaceFormat::OpenGLES);
	Format.setVersion(3, 0);
#else
	Format.setRenderableType(QSurfaceFormat::OpenGL);
	Format.setVersion(2, 1);
#endif
	Format.setDepthBufferSize(24);
	Format.setStencilBufferSize(8);
	QSurfaceFormat::setDefaultFormat(Format);
	QGuiApplication Application(argc, argv);
	RenderContextTests Tests;
	return QTest::qExec(&Tests, argc, argv);
}

#include "tst_glcontext.moc"
