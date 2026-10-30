#pragma once

#include <QJsonObject>
#include <QOpenGLFunctions>

// Qt's compositor and other views share GL state. Own the complete depth
// mapping before clearing/drawing a frame, not only DEPTH_TEST/DEPTH_FUNC.
inline void lcResetFrameDepthState(QOpenGLFunctions& GL)
{
	GL.glEnable(GL_DEPTH_TEST);
	GL.glDepthFunc(GL_LEQUAL);
	GL.glDepthMask(GL_TRUE);
	GL.glDepthRangef(0.0f, 1.0f);
	GL.glClearDepthf(1.0f);
}

// Call only with the intended draw framebuffer bound and its context current.
// Context format.depthBufferSize() does not prove that this FBO has depth.
inline QJsonObject lcGLFramebufferInfo(QOpenGLFunctions& GL)
{
	GLint Framebuffer = 0, DepthBits = 0, StencilBits = 0, Function = 0;
	GLboolean Write = GL_FALSE;
	GLfloat Range[2] = {}, Clear = 0;
	GL.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &Framebuffer);
	GL.glGetIntegerv(GL_DEPTH_BITS, &DepthBits);
	GL.glGetIntegerv(GL_STENCIL_BITS, &StencilBits);
	GL.glGetIntegerv(GL_DEPTH_FUNC, &Function);
	GL.glGetBooleanv(GL_DEPTH_WRITEMASK, &Write);
	GL.glGetFloatv(GL_DEPTH_RANGE, Range);
	GL.glGetFloatv(GL_DEPTH_CLEAR_VALUE, &Clear);
	const GLenum Status = GL.glCheckFramebufferStatus(GL_FRAMEBUFFER);
	return {{"framebuffer", Framebuffer}, {"depth_bits", DepthBits},
		{"stencil_bits", StencilBits}, {"framebuffer_status", int(Status)},
		{"depth_test", bool(GL.glIsEnabled(GL_DEPTH_TEST))}, {"depth_write", bool(Write)},
		{"depth_func", Function}, {"depth_range_near", Range[0]},
		{"depth_range_far", Range[1]}, {"clear_depth", Clear},
		{"passed", Status == GL_FRAMEBUFFER_COMPLETE && DepthBits >= 16}};
}

// A construction grid is a visual reference, not an occluder. Draw it before
// model geometry and preserve the caller's depth-write state. The callback must
// not change the depth mask (lcContext's cached state remains untouched).
template<typename DrawGrid>
inline void lcDrawReferenceGrid(QOpenGLFunctions& GL, DrawGrid Draw)
{
	GLboolean Write = GL_FALSE;
	GL.glGetBooleanv(GL_DEPTH_WRITEMASK, &Write);
	struct Restore
	{
		QOpenGLFunctions& GL;
		GLboolean Write;
		~Restore() { GL.glDepthMask(Write); }
	} Guard{GL, Write};
	GL.glDepthMask(GL_FALSE);
	Draw();
}
