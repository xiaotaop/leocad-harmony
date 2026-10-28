#pragma once

#include <QGuiApplication>
#include <QJsonObject>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QThread>

// Shared by the renderer and its standalone regression test. Keep Surface
// creation on the GUI thread, even on backends that use a hidden native window.
inline bool lcCreateOffscreenGLContext(QOpenGLContext& Context, QOffscreenSurface& Surface,
	QOpenGLContext* ShareContext, QString& Error)
{
	Error.clear();
	const QThread* Thread = QThread::currentThread();
	if (!QGuiApplication::instance() || Thread != QGuiApplication::instance()->thread()
		|| Context.thread() != Thread || Surface.thread() != Thread)
	{
		Error = QStringLiteral("offscreen_setup_requires_gui_thread");
		return false;
	}
	if (Context.isValid() || Surface.isValid())
	{
		Error = QStringLiteral("offscreen_objects_already_created");
		return false;
	}
	if (ShareContext && !ShareContext->isValid())
	{
		Error = QStringLiteral("share_context_invalid");
		return false;
	}

	Context.setScreen(ShareContext ? ShareContext->screen() : QGuiApplication::primaryScreen());
	Context.setFormat(ShareContext ? ShareContext->format() : QSurfaceFormat::defaultFormat());
	Context.setShareContext(ShareContext);
	if (!Context.create() || !Context.isValid())
	{
		Error = QStringLiteral("offscreen_context_create_failed");
		return false;
	}
	if (ShareContext && !QOpenGLContext::areSharing(&Context, ShareContext))
	{
		Error = QStringLiteral("offscreen_context_not_sharing");
		return false;
	}

	// The requested/default format may differ from the actual EGL configuration.
	Surface.setScreen(Context.screen());
	Surface.setFormat(Context.format());
	Surface.create();
	if (!Surface.isValid())
	{
		Error = QStringLiteral("offscreen_surface_create_failed");
		return false;
	}
	if (!Context.makeCurrent(&Surface))
	{
		Error = QStringLiteral("offscreen_make_current_failed");
		return false;
	}
	return true;
}

// The renderer uses compile-time desktop GL or GLES paths, not only different
// shader prefixes. Reject an incompatible runtime API before allocating meshes.
inline QString lcGLAPICompatibilityError(bool GLESBuild, bool ActualGLES, int MajorVersion)
{
	if (GLESBuild != ActualGLES)
		return QStringLiteral("renderer_context_api_mismatch");
	if (GLESBuild && MajorVersion < 3)
		return QStringLiteral("renderer_requires_gles3");
	return {};
}

inline QJsonObject lcGLFormatInfo(const QSurfaceFormat& Format)
{
	return {{"major", Format.majorVersion()}, {"minor", Format.minorVersion()},
		{"profile", int(Format.profile())}, {"renderable_type", int(Format.renderableType())},
		{"depth_bits", Format.depthBufferSize()}, {"stencil_bits", Format.stencilBufferSize()},
		{"samples", Format.samples()}};
}

inline QJsonObject lcGLContextInfo(QOpenGLContext& Context,
	const QOffscreenSurface* Surface, QOpenGLContext* ShareContext)
{
	const bool Current = QOpenGLContext::currentContext() == &Context;
	QJsonObject Report{{"valid", Context.isValid()}, {"is_current", Current},
		{"is_gles", Context.isOpenGLES()}, {"context_format", lcGLFormatInfo(Context.format())},
		{"share_requested", ShareContext != nullptr},
		{"sharing", ShareContext && Context.isValid() && ShareContext->isValid()
			&& QOpenGLContext::areSharing(&Context, ShareContext)}};
	if (Surface)
	{
		Report["surface_valid"] = Surface->isValid();
		Report["surface_requested_format"] = lcGLFormatInfo(Surface->requestedFormat());
		Report["surface_format"] = lcGLFormatInfo(Surface->format());
	}
	// Never query GL on an invalid/non-current context while reporting an error.
	if (Context.isValid() && Current)
	{
		QOpenGLFunctions* Functions = Context.functions();
		Functions->initializeOpenGLFunctions();
		const auto GLString = [Functions](GLenum Name)
		{
			const GLubyte* Value = Functions->glGetString(Name);
			return Value ? QString::fromLatin1(reinterpret_cast<const char*>(Value)) : QString();
		};
		Report["gl_version"] = GLString(GL_VERSION);
		Report["glsl_version"] = GLString(GL_SHADING_LANGUAGE_VERSION);
		Report["vendor"] = GLString(GL_VENDOR);
		Report["renderer"] = GLString(GL_RENDERER);
		Report["buffers"] = Functions->hasOpenGLFeature(QOpenGLFunctions::Buffers);
		Report["framebuffers"] = Functions->hasOpenGLFeature(QOpenGLFunctions::Framebuffers);
		Report["shaders"] = Functions->hasOpenGLFeature(QOpenGLFunctions::Shaders);
	}
	return Report;
}
