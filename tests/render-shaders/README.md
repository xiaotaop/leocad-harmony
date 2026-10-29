# Production shader regression

This target calls the same compiler/linker and uses the same prefix, material
manifest and resources as `lcContext`. It is compiled with `QT_NO_DEBUG` so Release
failures cannot disappear behind debug-only checks.

```sh
cmake -S tests/render-shaders -B build/render-shaders \
  -DCMAKE_PREFIX_PATH=/path/to/Qt -DCMAKE_BUILD_TYPE=Release
cmake --build build/render-shaders --parallel 4
build/render-shaders/leocad-render-shader-tests -o build/render-shaders/results.txt,txt
```

Requires a real graphical session. Tests cover all nine material pairs, missing
vertex/fragment resources, vertex/fragment compiler errors, a varying type link
error, repeated failure followed by recovery, and a green triangle read back from
an FBO using the production basic shader. Total: 15 cases + init/cleanup = 17.
All failures must return program 0, name the failed stage, include an error log,
and leave no GL error. Object deletion is also checked on successful programs.

On an actual GLES-capable backend, set `LEOCAD_TEST_GLES=1` to require ES 3.0 and
test the production `#version 300 es` prefix. A desktop fallback is a failure.
CI runs desktop GL and GLES against Mesa/Xvfb. macOS only validates desktop GL;
Mesa GLES is still not HarmonyOS GPU validation.
