# D7 screenshot harness

Captures what the parts panel, its options menu and the empty states look like in
Chinese. This is evidence for issue #28 / D4-03, not a test: the assertions live
in `tests/ui/d7/tst_localization.cpp`, and they check strings and metrics. What
they cannot check is whether the result is actually *readable*, which is what the
review asked for ("两种窗口尺寸下选项文字可辨识").

## Why a tool and not a test

`tests/ui/d7` runs under `QT_QPA_PLATFORM=offscreen` because it only needs to read
properties. Screenshots need pixels, and on a headless Linux box the only platform
here with a real GL context is Wayland through WSLg:

| platform | result |
| --- | --- |
| `offscreen` | `QOpenGLWidget is not supported on this platform`, then `QOpenGLWidget: No fbo, cannot render` and a segfault |
| `xcb` | needs `libxcb-cursor0` and 7 more libraries; and an X11 screen grab of a WSLg window returns a black rectangle because Weston composites it |
| `wayland` | real GL 4.1 on `D3D12 (Intel(R) UHD Graphics)`, `QWidget::grab()` returns the real pixels |

So the capture is done from inside the process with `QWidget::grab()`, never with
`ffmpeg -f x11grab`.

## Running it

```bash
source .devtools/env.sh
mkdir -p build/d7-shots && cd build/d7-shots
qmake ../../d7-shots.pro CONFIG+=debug && make -j4

cd ../..
QT_QPA_PLATFORM=wayland WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir \
  ./build/d7-shots/build/debug/leocad-d7-shots docs/evidence/member-c/<date>/d7-host
```

The output directory defaults to the path above with the date hardcoded; pass one
as the first argument to put it somewhere else.

## What it writes

Per window size (`narrow-1280x800`, `wide-1920x1080`):

| file | shows |
| --- | --- |
| `*-window.png` | the whole main window: menus, panel, count, colour groups |
| `*-options-menu.png` | the parts panel options menu, all entries |
| `*-filter-submenu.png` | the filter submenu — the one that showed `Regular...ression` on device |
| `*-categories.png` | the category tree with translated names |
| `*-search-empty.png` | a search matching nothing, i.e. the translated empty state |

Once, at the end: `colours.png` (the colour dock), `colour-button.png` and
`colour-button-light-grey.png` (the selection colour button, where the device
showed `Light Grey`), and `window-final.png`.

## Three things this had to work around

**The 3D view is blank in the captures.** `QWidget::grab()` reads a widget's
backing store; a `QOpenGLWidget` draws into its own FBO, which is not part of that
store. The GL output is verified separately (the harness prints `LEOCAD_GL_*`
diagnostics with `framebuffer_status: 36053`, which is
`GL_FRAMEBUFFER_COMPLETE`, and `passed: true`). Nothing in D7 touches the 3D view,
so a blank viewport costs nothing here — but do not read these files as evidence
that rendering works.

For the same reason, `narrow-1280x800-window.png` is **not byte-reproducible**.
Thumbnails in the parts list are rendered offscreen and arrive asynchronously, so
two runs can differ inside the thumbnail grid (measured: everything outside
`(841, 215)-(1241, 389)` is identical). If a future run changes only that one file,
it is the thumbnail timing and not a regression — check the other files verify
against their recorded hashes, and do not compare that one byte-for-byte.

**Menus are filled by emitting `aboutToShow`, not by popping them up.** The options
menu is emptied and refilled in that handler, and the width fix lives in it, so a
capture has to run it. Real popups under Wayland are `xdg_popup` surfaces, and
opening a submenu as a nested popup made the compositor kill the connection:

```
xdg_wm_base@3: error 2: xdg_popup was not created on the topmost popup
```

Emitting the signal directly runs the handler without a surface, which is enough
to render the widget with `grab()`.

**`lcContext::InitializeRenderer()` has to be called explicitly.** The real entry
point does it in `lcApplication::Initialize()`; a harness that constructs
`lcMainWindow` itself skips it, and the first paint then dereferences the grid
texture that would have been created there — a segfault in
`lcContext::BindTexture2D`, nowhere near the code under test. `ShutdownRenderer()`
is called at the end for the matching reason: the GL resources are static members
of `lcContext`, and left to the C++ runtime they are destroyed after
`QApplication` is gone, crashing on exit after every file had already been written.

## Refreshing the checked-in evidence

The PNGs under `docs/evidence/member-c/` are committed, so `.gitattributes` has to
keep them byte-exact or the recorded SHA-256 values stop matching. The directory is
already covered by the `-text` exceptions used for the other evidence trees; a new
directory needs its own line.
