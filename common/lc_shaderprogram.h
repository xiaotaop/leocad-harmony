#pragma once

#include <QFile>
#include <QOpenGLFunctions>
#include <QString>

struct lcShaderProgramSpec
{
	const char* Name;
	const char* Vertex;
	const char* Fragment;
	bool Advanced;
};

// Order matches lcMaterialType; the renderer checks the count at compile time.
inline constexpr lcShaderProgramSpec lcShaderPrograms[] =
{
	{"UnlitColor", ":/resources/shaders/unlit_color_vs.glsl", ":/resources/shaders/unlit_color_ps.glsl", false},
	{"UnlitColorConditional", ":/resources/shaders/unlit_color_conditional_vs.glsl", ":/resources/shaders/unlit_color_conditional_ps.glsl", true},
	{"UnlitTextureModulate", ":/resources/shaders/unlit_texture_modulate_vs.glsl", ":/resources/shaders/unlit_texture_modulate_ps.glsl", false},
	{"TextSDF", ":/resources/shaders/unlit_texture_modulate_vs.glsl", ":/resources/shaders/text_sdf_ps.glsl", false},
	{"UnlitTextureDecal", ":/resources/shaders/unlit_texture_decal_vs.glsl", ":/resources/shaders/unlit_texture_decal_ps.glsl", false},
	{"UnlitVertexColor", ":/resources/shaders/unlit_vertex_color_vs.glsl", ":/resources/shaders/unlit_vertex_color_ps.glsl", false},
	{"UnlitViewSphere", ":/resources/shaders/unlit_view_sphere_vs.glsl", ":/resources/shaders/unlit_view_sphere_ps.glsl", false},
	{"FakeLitColor", ":/resources/shaders/fakelit_color_vs.glsl", ":/resources/shaders/fakelit_color_ps.glsl", true},
	{"FakeLitTextureDecal", ":/resources/shaders/fakelit_texture_decal_vs.glsl", ":/resources/shaders/fakelit_texture_decal_ps.glsl", true}
};

inline QByteArray lcShaderPrefix(bool GLES)
{
	const QByteArray Common =
"#define LC_PIXEL_FAKE_LIGHTING \\\n"
"		LC_SHADER_PRECISION vec3 Normal = normalize(PixelNormal); \\\n"
"		LC_SHADER_PRECISION vec3 LightDirection = normalize(PixelPosition - LightPosition); \\\n"
"		LC_SHADER_PRECISION vec3 VertexToEye = normalize(EyePosition - PixelPosition); \\\n"
"		LC_SHADER_PRECISION vec3 LightReflect = normalize(reflect(-LightDirection, Normal)); \\\n"
"		LC_SHADER_PRECISION float Specular = abs(dot(VertexToEye, LightReflect)); \\\n"
"		Specular = min(pow(Specular, 8.0), 1.0) * 0.25; \\\n"
"		LC_SHADER_PRECISION vec3 SpecularColor = vec3(Specular, Specular, Specular); \\\n"
"		LC_SHADER_PRECISION float Diffuse = min(abs(dot(Normal, LightDirection)) * 0.6 + 0.65, 1.0);\n";
	if (!GLES)
		return QByteArray(
"#version 110\n"
"#define mediump\n"
"#define LC_VERTEX_INPUT attribute\n"
"#define LC_VERTEX_OUTPUT varying\n"
"#define LC_PIXEL_INPUT varying\n"
"#define LC_PIXEL_OUTPUT\n"
"#define LC_SHADER_PRECISION\n") + Common;
	return QByteArray(
"#version 300 es\n"
"#define texture2D texture\n"
"#define LC_VERTEX_INPUT in\n"
"#define LC_VERTEX_OUTPUT out\n"
"#define LC_PIXEL_INPUT in mediump\n"
"#define gl_FragColor FragColor\n"
"#define LC_PIXEL_OUTPUT out mediump vec4 gl_FragColor;\n"
"#define LC_SHADER_PRECISION mediump\n") + Common;
}

struct lcShaderBuildResult
{
	GLuint Object = 0;
	QString Error;
	QString Stage;
	QString Log;
};

// Uses the production sources and checks Release builds too. The caller owns a
// successful program; every shader and unsuccessful program is deleted here.
inline lcShaderBuildResult lcBuildShaderProgram(QOpenGLFunctions& GL,
	const lcShaderProgramSpec& Spec, const QByteArray& Prefix)
{
	lcShaderBuildResult Result;
	const auto Compile = [&](const char* Path, GLenum Type, const char* Stage) -> GLuint
	{
		Result.Stage = QString::fromLatin1(Stage);
		QFile File(QString::fromUtf8(Path));
		if (!File.open(QIODevice::ReadOnly))
		{
			Result.Error = QStringLiteral("shader_resource_open_failed");
			Result.Log = File.errorString();
			return 0;
		}
		const GLuint Shader = GL.glCreateShader(Type);
		if (!Shader)
		{
			Result.Error = QStringLiteral("shader_create_failed");
			return 0;
		}
		const QByteArray Source = Prefix + File.readAll();
		const char* Data = Source.constData();
		GL.glShaderSource(Shader, 1, &Data, nullptr);
		GL.glCompileShader(Shader);
		GLint Compiled = GL_FALSE;
		GL.glGetShaderiv(Shader, GL_COMPILE_STATUS, &Compiled);
		if (Compiled == GL_FALSE)
		{
			GLint Length = 0;
			GL.glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &Length);
			QByteArray Log(qMax(Length, 1), '\0');
			GL.glGetShaderInfoLog(Shader, Log.size(), &Length, Log.data());
			Result.Error = QStringLiteral("shader_compile_failed");
			Result.Log = QString::fromUtf8(Log.constData(), qMax(Length, 0));
			GL.glDeleteShader(Shader);
			return 0;
		}
		return Shader;
	};
	const GLuint Vertex = Compile(Spec.Vertex, GL_VERTEX_SHADER, "vertex");
	if (!Vertex)
		return Result;
	const GLuint Fragment = Compile(Spec.Fragment, GL_FRAGMENT_SHADER, "fragment");
	if (!Fragment)
	{
		GL.glDeleteShader(Vertex);
		return Result;
	}
	Result.Stage = QStringLiteral("link");
	const GLuint Program = GL.glCreateProgram();
	if (Program)
	{
		GL.glAttachShader(Program, Vertex);
		GL.glAttachShader(Program, Fragment);
		const char* Attributes[] = {"VertexPosition", "VertexNormal", "VertexTexCoord", "VertexColor"};
		const char* ControlPoints[] = {"VertexPosition1", "VertexPosition2", "VertexPosition3", "VertexPosition4"};
		for (GLuint Index = 0; Index < 4; Index++)
		{
			GL.glBindAttribLocation(Program, Index, Attributes[Index]);
			GL.glBindAttribLocation(Program, Index, ControlPoints[Index]);
		}
		GL.glLinkProgram(Program);
		GL.glDetachShader(Program, Vertex);
		GL.glDetachShader(Program, Fragment);
	}
	GL.glDeleteShader(Vertex);
	GL.glDeleteShader(Fragment);
	if (!Program)
	{
		Result.Error = QStringLiteral("shader_program_create_failed");
		return Result;
	}
	GLint Linked = GL_FALSE;
	GL.glGetProgramiv(Program, GL_LINK_STATUS, &Linked);
	if (Linked == GL_FALSE)
	{
		GLint Length = 0;
		GL.glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &Length);
		QByteArray Log(qMax(Length, 1), '\0');
		GL.glGetProgramInfoLog(Program, Log.size(), &Length, Log.data());
		Result.Error = QStringLiteral("shader_link_failed");
		Result.Log = QString::fromUtf8(Log.constData(), qMax(Length, 0));
		GL.glDeleteProgram(Program);
		return Result;
	}
	Result.Object = Program;
	Result.Stage.clear();
	return Result;
}
