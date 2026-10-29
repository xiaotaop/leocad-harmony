#pragma once

#include <QtGlobal>

// Process-scoped diagnostic fallback; restart to change it. Never save it into
// user preferences or disable the shaders required by the GLES renderer.
inline bool lcSafeRenderEnabled()
{
#ifdef LC_SAFE_RENDER
	return true;
#else
	static const bool Enabled = qgetenv("LEOCAD_SAFE_RENDER") == "1";
	return Enabled;
#endif
}
