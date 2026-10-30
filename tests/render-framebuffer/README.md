# Widget framebuffer / grid regression

Uses the production depth-state reset, reference-grid pass and shader compiler in
a real `QOpenGLWidget`, not the Qt offscreen platform plugin. Run with a graphical
session (Linux CI uses Xvfb and Mesa).

```sh
cmake -S tests/render-framebuffer -B build/render-framebuffer \
  -DCMAKE_PREFIX_PATH=/path/to/Qt -DCMAKE_BUILD_TYPE=Release
cmake --build build/render-framebuffer --parallel 4
build/render-framebuffer/leocad-framebuffer-tests -o build/render-framebuffer/results.txt,txt
# On a GLES-capable backend; silently falling back to desktop GL fails.
LEOCAD_TEST_GLES=1 build/render-framebuffer/leocad-framebuffer-tests
```

Nine totals (seven cases + init/cleanup) cover:

- A negative control reproduces grid overdraw with leaked depth range/clear state.
- A second negative control puts a grid plane in front of solid geometry even
  with correct depth state. The reference-background pass leaves solid interiors
  unchanged while the grid remains visible around them.
- The reference pass temporarily disables depth writes and restores either
  original mask value; it does not change the context's cached mask.
- Full depth-state restoration and foreground/background pixel readback across
  resize and restore; image size must match logical size times actual DPR.
- A real half-alpha texture and translucent foreground are blended and read back.
- A color-only FBO is detected as missing depth; Qt's widget FBO binding is restored.
- Destroying and recreating a widget preserves usable depth and occlusion.

This is controlled geometry using the production helpers. Complete LeoCAD scene
captures provide a separate integration check; see D4 evidence. The Pillow helper
`compare_grid_pixels.py` checks two sizes of the fixed red/blue model, with a
5x5 erosion to exclude silhouettes and an 8/255 channel tolerance. It is not a
general model/image classifier and is not used as a cross-GPU golden image.
