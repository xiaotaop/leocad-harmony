# FBO image readback regression

Build with Qt 6.5+ Gui/OpenGL/Test:

```sh
cmake -S tests/render-readback -B build/render-readback -DCMAKE_BUILD_TYPE=Release
cmake --build build/render-readback --parallel 2
build/render-readback/leocad-readback-tests
# Linux Mesa, same target backend type as the Harmony renderer:
LIBGL_ALWAYS_SOFTWARE=1 LEOCAD_TEST_GLES=1 xvfb-run -a build/render-readback/leocad-readback-tests
```

The controlled RGBA8888 negative control reproduces the old byte-copy bug. Tests call the same production `lcCopyRenderImageTile` used by `lcView::OnDraw`, including padded rows, alpha conversion, nonzero source/destination origins and untouched borders. The real FBO cases record native format/stride, draw an asymmetric pixel pattern and verify orientation/channels/alpha after readback for single and multisample targets. Their clear values are premultiplied because Qt labels these FBO readbacks as premultiplied; comparisons allow one 8-bit quantization unit.

Full renderer/thumbnail/camera integration is in `tests/model/tst_model_io.cpp`, using the existing `member-b-tests.pro` lifecycle. It renders actual meshes through `lcThumbnailManager` and `lcView`, checks color/size/DPR caches, camera fitting and both partial-tile directions. Run `LEOCAD_SAFE_RENDER=1 <model-test-binary> thumbnailAndCameraColors tiledRenderKeepsColorAndOrientation` in a separate process for the basic path. `LEOCAD_TEST_EVIDENCE_DIR` optionally saves actual thumbnails/camera frames. Those tests are host-only; they do not silently add native window/device acceptance to Harmony's model test count.
