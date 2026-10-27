# LeoCAD for HarmonyOS PC

This repository hosts a community port of [LeoCAD](https://github.com/leozide/leocad) to HarmonyOS PC. It is not an official LeoCAD release.

本仓库用于将 LeoCAD 移植到鸿蒙 PC。首发版本聚焦 LDraw 核心建模闭环，并复用上游 C++、Qt Widgets 与 `QOpenGLWidget` 界面，不使用 ArkUI 重写主界面。

## Baseline

- Upstream release: `v26.09`
- Upstream commit: `5196355d4a5adb35f14c97e7672d7a9bfd3af391`
- Target: HarmonyOS PC, ARM64
- UI: Qt 6.12 for HarmonyOS, Qt Widgets
- Rendering: Qt OpenGL / `QOpenGLWidget`
- Packaging: DevEco Studio, Hvigor and CMake

The `upstream` Git remote must continue to point to `https://github.com/leozide/leocad.git`.

## First-release scope

The first release is expected to support:

- Creating a model and loading a curated offline LDraw library.
- Selecting, inserting, moving, rotating, deleting and recoloring pieces.
- Mouse-based orbit, pan and zoom.
- Undo and redo.
- Basic `.ldr` and `.mpd` open/save workflows.
- A signed HarmonyOS package suitable for store submission.

Printing, external renderers, online updates, online part downloads, gamepads, advanced exports and full-library packaging are outside the first-release scope.

## Repository layout

```text
app/                         DevEco/Hvigor application shell and packaging
platform/harmonyos/          HarmonyOS lifecycle, paths and system integration
common/                      Upstream LeoCAD application and modeling code
qt/                          Upstream Qt entry point
resources/                   Upstream Qt resources
assets/ldraw-lite/           Curated offline parts-library manifest and packaging
cmake/                       HarmonyOS/Qt CMake integration
patches/                     Port-specific patch records by module
tests/device/                Physical-device test procedures and evidence index
docs/                        Architecture, schedule and release documentation
```

The root `CMakeLists.txt` is the shared desktop/HarmonyOS build entry. The port
directories contain module contracts and will receive platform-specific changes
without mixing generated HAP files or signing material into source control.

## Team workflow

- `main` must remain buildable.
- `release/harmony-first` is created at feature freeze.
- Suggested working branches: `feature/leader-platform`, `feature/a-render`, `feature/b-model`, and `feature/c-ui-library`.
- One physical-device run is performed by the team lead every day at 21:00. Test requests must be ready by 20:30.
- After day 8, only release material, tests, and P0/P1 fixes may enter the release branch.

See [CONTRIBUTING.md](CONTRIBUTING.md) and the [two-week development plan](docs/LeoCAD_鸿蒙PC首发版_两周分工与技术方案.md).

## Build status

The CMake wrapper, desktop Qt 6.12 compile regression and an ARM64 HarmonyOS
Qt Widgets/OpenGL diagnostic HAP build are available. Physical-device execution
still requires a trusted Huawei developer debug signature; an unsigned or locally
self-signed HAP is not proof that the package runs on a commercial device.

Run the local baseline check with:

```powershell
pwsh -File scripts/check-toolchain.ps1
```

Exact committed toolchain versions are in
[`toolchain.lock`](toolchain.lock). See the [D1 baseline](docs/D1_BASELINE.md)
and [store submission checklist](docs/STORE_SUBMISSION_CHECKLIST.md). The D2
probe build/sign/run procedure is in
[`docs/D2_HARMONY_DEVICE_PROBE.md`](docs/D2_HARMONY_DEVICE_PROBE.md).

## Licensing and trademarks

LeoCAD is distributed under the GNU General Public License version 2. See [COPYING](COPYING) and the upstream notices in `docs/`.

LEGO is a trademark of the LEGO Group, which does not sponsor, authorize or endorse this project. LeoCAD and LDraw names, artwork and parts-library redistribution terms must be checked separately before store submission.
