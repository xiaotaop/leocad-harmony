// Standalone D1 diagnostic. Does not alter LeoCAD's runtime or preferences.
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QSysInfo>
#include <QTimer>
#include <cmath>
#include <memory>

static QJsonObject FormatInfo(const QSurfaceFormat& Format)
{
	return {{"major", Format.majorVersion()}, {"minor", Format.minorVersion()},
		{"profile", int(Format.profile())}, {"renderable_type", int(Format.renderableType())},
		{"depth_bits", Format.depthBufferSize()}, {"stencil_bits", Format.stencilBufferSize()},
		{"samples", Format.samples()}};
}

class RenderProbe final : public QOpenGLWidget
{
public:
	RenderProbe(QString Output, QString Screenshot)
		: mOutput(std::move(Output)), mScreenshot(std::move(Screenshot))
	{
		mReport = {{"schema_version", 1}, {"qt", qVersion()},
			{"platform", QGuiApplication::platformName()}, {"os", QSysInfo::prettyProductName()},
			{"architecture", QSysInfo::currentCpuArchitecture()},
			{"requested_format", FormatInfo(QSurfaceFormat::defaultFormat())},
			{"scope", "host/device diagnostic; not HarmonyOS release acceptance"}};
		resize(640, 480);
		setWindowTitle("LeoCAD GL capability probe");
		QTimer::singleShot(15000, this, [this]() { Finish("No completed widget frame before timeout"); });
	}

	~RenderProbe() override
	{
		if (context() && context()->isValid())
		{
			makeCurrent();
			if (mBuffer)
				context()->functions()->glDeleteBuffers(1, &mBuffer);
			mProgram.reset();
			doneCurrent();
		}
	}

protected:
	void initializeGL() override
	{
		QOpenGLContext* Context = context();
		QOpenGLFunctions* Functions = Context->functions();
		Functions->initializeOpenGLFunctions();
		const auto GLString = [Functions](GLenum Name)
		{
			const GLubyte* Value = Functions->glGetString(Name);
			return Value ? QString::fromLatin1(reinterpret_cast<const char*>(Value)) : QString();
		};
		mReport["context"] = QJsonObject{{"valid", Context->isValid()},
			{"is_gles", Context->isOpenGLES()}, {"format", FormatInfo(Context->format())},
			{"vendor", GLString(GL_VENDOR)}, {"renderer", GLString(GL_RENDERER)},
			{"gl_version", GLString(GL_VERSION)}, {"glsl_version", GLString(GL_SHADING_LANGUAGE_VERSION)}};
		mAPIHonored = Context->isOpenGLES() == (QSurfaceFormat::defaultFormat().renderableType() == QSurfaceFormat::OpenGLES);
		mReport["requested_api_honored"] = mAPIHonored;
		mReport["features"] = QJsonObject{
			{"buffers", Functions->hasOpenGLFeature(QOpenGLFunctions::Buffers)},
			{"framebuffers", Functions->hasOpenGLFeature(QOpenGLFunctions::Framebuffers)},
			{"shaders", Functions->hasOpenGLFeature(QOpenGLFunctions::Shaders)},
			{"threaded_gl", QOpenGLContext::supportsThreadedOpenGL()}};

		// Match the compile-time prefix in common/lc_context.cpp at the D1 baseline.
		// Select by the actual context here to diagnose a mismatched platform build.
		QByteArray Prefix = Context->isOpenGLES()
			? QByteArray("#version 300 es\n#define texture2D texture\n#define LC_VERTEX_INPUT in\n"
				"#define LC_VERTEX_OUTPUT out\n#define LC_PIXEL_INPUT in mediump\n"
				"#define gl_FragColor FragColor\n#define LC_PIXEL_OUTPUT out mediump vec4 gl_FragColor;\n"
				"#define LC_SHADER_PRECISION mediump\n")
			: QByteArray("#version 110\n#define mediump\n#define LC_VERTEX_INPUT attribute\n"
				"#define LC_VERTEX_OUTPUT varying\n#define LC_PIXEL_INPUT varying\n"
				"#define LC_PIXEL_OUTPUT\n#define LC_SHADER_PRECISION\n");
		Prefix += "#define LC_PIXEL_FAKE_LIGHTING \\\n"
			" LC_SHADER_PRECISION vec3 Normal = normalize(PixelNormal); \\\n"
			" LC_SHADER_PRECISION vec3 LightDirection = normalize(PixelPosition - LightPosition); \\\n"
			" LC_SHADER_PRECISION vec3 VertexToEye = normalize(EyePosition - PixelPosition); \\\n"
			" LC_SHADER_PRECISION vec3 LightReflect = normalize(reflect(-LightDirection, Normal)); \\\n"
			" LC_SHADER_PRECISION float Specular = abs(dot(VertexToEye, LightReflect)); \\\n"
			" Specular = min(pow(Specular, 8.0), 1.0) * 0.25; \\\n"
			" LC_SHADER_PRECISION vec3 SpecularColor = vec3(Specular, Specular, Specular); \\\n"
			" LC_SHADER_PRECISION float Diffuse = min(abs(dot(Normal, LightDirection)) * 0.6 + 0.65, 1.0);\n";
		const char* const VertexShaders[] = {"unlit_color", "unlit_color_conditional",
			"unlit_texture_modulate", "unlit_texture_modulate", "unlit_texture_decal",
			"unlit_vertex_color", "unlit_view_sphere", "fakelit_color", "fakelit_texture_decal"};
		const char* const FragmentShaders[] = {"unlit_color", "unlit_color_conditional",
			"unlit_texture_modulate", "text_sdf", "unlit_texture_decal",
			"unlit_vertex_color", "unlit_view_sphere", "fakelit_color", "fakelit_texture_decal"};
		QJsonArray ShaderResults;
		mShadersOK = true;
		for (int Index = 0; Index < 9; Index++)
		{
			QOpenGLShaderProgram Program;
			const auto ReadShader = [](const char* Name, const char* Suffix)
			{
				QFile File(QString(":/probe/shaders/%1_%2.glsl").arg(Name, Suffix));
				return File.open(QIODevice::ReadOnly) ? File.readAll() : QByteArray();
			};
			const QByteArray Vertex = ReadShader(VertexShaders[Index], "vs");
			const QByteArray Fragment = ReadShader(FragmentShaders[Index], "ps");
			const bool VertexOK = !Vertex.isEmpty() && Program.addShaderFromSourceCode(QOpenGLShader::Vertex, Prefix + Vertex);
			const bool FragmentOK = !Fragment.isEmpty() && Program.addShaderFromSourceCode(QOpenGLShader::Fragment, Prefix + Fragment);
			const bool Linked = VertexOK && FragmentOK && Program.link();
			ShaderResults.append(QJsonObject{{"vertex", VertexShaders[Index]}, {"fragment", FragmentShaders[Index]},
				{"vertex_compiled", VertexOK}, {"fragment_compiled", FragmentOK}, {"linked", Linked}, {"log", Program.log()}});
			mShadersOK = mShadersOK && Linked;
		}
		mReport["leocad_shaders"] = ShaderResults;

		mProgram = std::make_unique<QOpenGLShaderProgram>();
		const QByteArray Vertex = Context->isOpenGLES()
			? "#version 300 es\nin vec2 Position; void main(){gl_Position=vec4(Position,0.0,1.0);}\n"
			: "#version 110\nattribute vec2 Position; void main(){gl_Position=vec4(Position,0.0,1.0);}\n";
		const QByteArray Fragment = Context->isOpenGLES()
			? "#version 300 es\nprecision mediump float; out vec4 Color; void main(){Color=vec4(0.0,1.0,0.0,1.0);}\n"
			: "#version 110\nvoid main(){gl_FragColor=vec4(0.0,1.0,0.0,1.0);}\n";
		mProgramOK = mProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, Vertex)
			&& mProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, Fragment) && mProgram->link();
		mReport["triangle_shader"] = QJsonObject{{"linked", mProgramOK}, {"log", mProgram->log()}};
		const GLfloat Vertices[] = {-0.8f, -0.8f, 0.8f, -0.8f, 0.0f, 0.8f};
		Functions->glGenBuffers(1, &mBuffer);
		Functions->glBindBuffer(GL_ARRAY_BUFFER, mBuffer);
		Functions->glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);
		mReport["buffer_uploaded"] = mBuffer != 0 && Functions->glGetError() == GL_NO_ERROR;
		Functions->glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void paintGL() override
	{
		QOpenGLFunctions* Functions = context()->functions();
		Functions->glClearColor(0.05f, 0.1f, 0.3f, 1.0f);
		Functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		if (mProgramOK)
		{
			mProgram->bind();
			Functions->glBindBuffer(GL_ARRAY_BUFFER, mBuffer);
			const int Location = mProgram->attributeLocation("Position");
			mProgram->enableAttributeArray(Location);
			mProgram->setAttributeBuffer(Location, GL_FLOAT, 0, 2);
			Functions->glDrawArrays(GL_TRIANGLES, 0, 3);
			mProgram->disableAttributeArray(Location);
			Functions->glBindBuffer(GL_ARRAY_BUFFER, 0);
			mProgram->release();
		}
		mFrames++;
		if (mFrames == 1)
			QTimer::singleShot(0, this, [this]() { resize(800, 500); update(); });
		else if (mFrames == 2)
		{
#ifdef Q_OS_HARMONY
			constexpr int FinishDelayMs = 3000;
#else
			constexpr int FinishDelayMs = 100;
#endif
			QTimer::singleShot(FinishDelayMs, this, [this]() { Finish({}); });
		}
	}

private:
	void Finish(const QString& Error)
	{
		if (mFinished)
			return;
		mFinished = true;
		bool Passed = Error.isEmpty() && isValid() && mFrames >= 2;
		mReport["error"] = Error;
		if (Passed)
		{
			makeCurrent();
			QOpenGLFunctions* Functions = context()->functions();
			const QImage Image = grabFramebuffer();
			const QColor Center = Image.isNull() ? QColor() : Image.pixelColor(Image.width() / 2, Image.height() / 2);
			const bool PixelOK = !Image.isNull() && Center.green() > 240 && Center.red() < 15 && Center.blue() < 15;
			const bool SizeOK = Image.size() == QSize(qRound(width() * devicePixelRatioF()), qRound(height() * devicePixelRatioF()));
			// Native window geometry may round by one logical pixel at fractional DPR.
			const bool Resized = std::abs(width() - 800) <= 1 && std::abs(height() - 500) <= 1;
			mReport["widget"] = QJsonObject{{"frames", mFrames}, {"default_fbo", int(defaultFramebufferObject())},
				{"logical_width", width()}, {"logical_height", height()}, {"dpr", devicePixelRatioF()},
				{"pixel_width", Image.width()}, {"pixel_height", Image.height()},
				{"size_matches_dpr", SizeOK}, {"resize_completed", Resized}, {"triangle_pixel_passed", PixelOK}};
			bool ScreenshotOK = true;
			if (!mScreenshot.isEmpty())
				ScreenshotOK = Image.save(mScreenshot);
			mReport["screenshot_saved"] = mScreenshot.isEmpty() ? QJsonValue() : QJsonValue(ScreenshotOK);

			bool FBOOK = false;
			{
				QOpenGLFramebufferObjectFormat Format;
				Format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
				QOpenGLFramebufferObject FBO(32, 32, Format);
				if (FBO.isValid() && FBO.attachment() == QOpenGLFramebufferObject::CombinedDepthStencil && FBO.bind())
				{
					Functions->glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
					Functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
					const QImage Pixels = FBO.toImage();
					FBOOK = !Pixels.isNull() && Pixels.pixelColor(16, 16).red() > 240;
				}
			}
			Functions->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
			mReport["depth_stencil_fbo_readback"] = FBOOK;

			GLuint Texture = 0;
			Functions->glGenTextures(1, &Texture);
			Functions->glBindTexture(GL_TEXTURE_2D, Texture);
			const GLubyte Texel[] = {255, 0, 0, 255};
			Functions->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, Texel);
			Functions->glFlush();
			const bool TextureOK = Functions->glGetError() == GL_NO_ERROR;
			QOpenGLContext Shared;
			Shared.setFormat(context()->format());
			Shared.setShareContext(context());
			const bool Created = Shared.create();
			QOffscreenSurface Surface;
			Surface.setFormat(Shared.format());
			Surface.create();
			const bool Current = Created && Surface.isValid() && Shared.makeCurrent(&Surface);
			bool SharedTexture = false;
			if (Current)
			{
				Shared.functions()->initializeOpenGLFunctions();
				SharedTexture = Shared.functions()->glIsTexture(Texture);
				Shared.doneCurrent();
			}
			const bool Sharing = Created && QOpenGLContext::areSharing(context(), &Shared);
			mReport["offscreen"] = QJsonObject{{"context_created", Created}, {"surface_valid", Surface.isValid()},
				{"make_current", Current}, {"sharing", Sharing}, {"texture_visible", SharedTexture},
				{"format", FormatInfo(Shared.format())}};
			makeCurrent();
			Functions->glDeleteTextures(1, &Texture);
			const GLenum GLError = Functions->glGetError();
			mReport["final_gl_error"] = int(GLError);
			doneCurrent();
			Passed = Passed && mAPIHonored && mProgramOK && mShadersOK && PixelOK && SizeOK && Resized && FBOOK && TextureOK
				&& mReport["buffer_uploaded"].toBool() && Current && Sharing && SharedTexture && ScreenshotOK && GLError == GL_NO_ERROR;
		}
		mReport["passed"] = Passed;
		const QByteArray Data = QJsonDocument(mReport).toJson(QJsonDocument::Indented);
		QSaveFile File(mOutput);
		const bool Written = File.open(QIODevice::WriteOnly) && File.write(Data) == Data.size() && File.commit();
		qInfo().noquote() << "LEOCAD_GL_PROBE" << QString::fromUtf8(QJsonDocument(mReport).toJson(QJsonDocument::Compact));
		if (!Written)
			qCritical() << "LEOCAD_GL_PROBE cannot write report:" << File.errorString();
		QCoreApplication::exit(Passed && Written ? 0 : 1);
	}

	QString mOutput;
	QString mScreenshot;
	QJsonObject mReport;
	std::unique_ptr<QOpenGLShaderProgram> mProgram;
	GLuint mBuffer = 0;
	int mFrames = 0;
	bool mProgramOK = false;
	bool mAPIHonored = false;
	bool mShadersOK = false;
	bool mFinished = false;
};

int main(int argc, char* argv[])
{
	QSurfaceFormat Format;
#ifdef Q_OS_HARMONY
	bool GLES = true;
#else
	bool GLES = false;
#endif
	for (int Index = 1; Index < argc; Index++)
		GLES = GLES || QByteArray(argv[Index]) == "--gles";
	Format.setRenderableType(GLES ? QSurfaceFormat::OpenGLES : QSurfaceFormat::OpenGL);
	Format.setVersion(GLES ? 3 : 2, GLES ? 0 : 1);
	Format.setDepthBufferSize(24);
	Format.setStencilBufferSize(8);
	Format.setSamples(0);
	QSurfaceFormat::setDefaultFormat(Format);
	QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
	QApplication Application(argc, argv);
	QCommandLineParser Parser;
	Parser.setApplicationDescription("LeoCAD GL / FBO / shader / shared-context capability probe");
	Parser.addHelpOption();
	Parser.addOption({"gles", "Request OpenGL ES 3.0; verify actual is_gles in the JSON."});
	QString DefaultOutput = "render-probe.json";
	QString DefaultScreenshot;
#ifdef Q_OS_HARMONY
	const QString DataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	if (!DataDirectory.isEmpty() && QDir().mkpath(DataDirectory))
	{
		DefaultOutput = DataDirectory + "/render-probe.json";
		DefaultScreenshot = DataDirectory + "/render-probe.png";
	}
#endif
	Parser.addOption({"output", "JSON report path (parent directory must exist).", "path", DefaultOutput});
	Parser.addOption({"screenshot", "Optional PNG of the widget after resize.", "path", DefaultScreenshot});
	Parser.process(Application);
	RenderProbe Probe(Parser.value("output"), Parser.value("screenshot"));
	Probe.show();
	return Application.exec();
}
