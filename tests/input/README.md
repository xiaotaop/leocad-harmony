# Production view input regression

Build from an ASCII source path on macOS (Qt Linguist otherwise misparses some shell paths):

```sh
mkdir -p build/input && cd build/input
qmake ../../member-a-input-tests.pro CONFIG+=release
make -j4
build/release/leocad-input-tests
LEOCAD_SAFE_RENDER=1 build/release/leocad-input-tests
```

A real window server and OpenGL are required. On Linux use Mesa + xvfb; on Windows use `scripts/Run-MemberAInputTests.ps1 -QtRoot <Qt-root>` with the software OpenGL archive installed. The script builds once and runs separate normal/fractional-DPR1.5/DPR2/basic-DPR2 processes, fails on skipped cases, and retains outputs. macOS Qt6.12 lrelease also requires its QtQml runtime. Qt6.8+ is exercised by CI.

The suite uses the actual main window, lcViewWidget, renderer and bundled 3003.dat. Dock panels are hidden to isolate canvas input from their separate lifecycle suites. QApplication-delivered events exercise production handlers; the QWindow case traverses QWidgetWindow mouse capture before asserting a normal release commits and Undo restores. These are host event tests, not physical mouse or Harmony acceptance.

The nine geometry rows call the actual broad-phase helper. The captured ray reproduces the DPR1 edge miss of the original float entry-time computation. Near-edge misses remain misses: box boundaries are unchanged. Canvas cases cover navigation, actual picking across sizes/DPR, four interruptions, release loss, pending-move rollback, fresh no-hover press, view-sphere cancellation, rectangle zoom cancellation and synthesized pan lifecycle.

`LEOCAD_INPUT_EVIDENCE_DIR` optionally saves real rendered canvas frames. The macOS evidence uses native DPR2 and `QT_SCALE_FACTOR=0.5` for observed DPR1; the Windows CI uses scale factors1/2 on its DPR1 display. Check the recorded INPUT_WIDGET dimensions/DPR rather than inferring from an environment variable. The original focus negative can be repeated by copying this test and .pro into the pinned baseline and running `leocad-input-tests navigation focusLossStopsTracking`.

D7 adds small angle-delta accumulation (±30/±8), remainders/direction cancellation/focus reset, wheel speed modifiers and horizontal input, actual keyboard QAction move/Undo/Redo, and Shift-pan/Alt-zoom vector semantics. The keyboard fixture explicitly establishes Qt activation/focus; it does not test OS foreground policy. macOS additionally runs scale 0.75 on a native DPR2 display to observe DPR1.5. These injected events do not certify physical devices.

D8 adds `alphaSwitchSequence` (canvas-only/parts-visible): 10 LDR+MPD rounds per row through production OpenProject/SaveProject, mesh picking, keyboard move/Undo/Redo, temporary save/reopen, submodel changes, menus and timer heartbeat. Explicit local paths bypass OS pickers. This regression does not reproduce OHOS NAPI/clipboard behavior or close issue #45. Run just these rows with `leocad-input-tests alphaSwitchSequence`; bound the process externally when investigating hangs. All fixture inputs/outputs use QTemporaryDir copies.
