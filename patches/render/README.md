# Render patch inventory

## 2026-10-27: D1 baseline and capability diagnostic

- Base: `3e2e15b5bcd6ba87720bd2dfebffd49fd48ee3c0`.
- Added independent `tests/render/` target, `scripts/Build-MemberA.py`, a fixed
  two-brick model, host evidence, risk inventory and physical-device handoff.
- No upstream production files, root build configuration, application entry or
  C's UI/library work are changed.
- Context compatibility patches are planned after the first target-device probe;
  macOS success is not evidence of HarmonyOS compatibility.
- Rollback: remove this diagnostic target and its documentation. It is not linked
  into the normal LeoCAD application, so no application feature flag is required.

See [D1 result](../../docs/MEMBER_A_D1.md) and
[risk inventory](../../docs/MEMBER_A_RENDER_RISKS.md).

## 2026-10-28: D2 offscreen context compatibility

- Base: `32ce8425f5fa7f14b7a4a837ead2339a314a66e8`.
- Route `lcContext::CreateOffscreenContext` through the tested `lc_glcontext.h`
  helper so Surface format/screen use the actual context and the sharing group
  is checked. Reject non-GUI setup and incompatible renderer GL/GLES paths.
- Log `LEOCAD_GL_CONTEXT` once for offscreen setup and each view initialization.
- Preserve desktop rendering and upstream null-share offscreen behavior.
- Keep the team lead's pending PR #7 capability probe and HAP scripts untouched.
- Host tests are complete; these production changes still require target testing.

See [D2 result](../../docs/MEMBER_A_D2.md) and
[device handoff / rollback](../../docs/MEMBER_A_D2_DEVICE_HANDOFF.md).

## 2026-10-29: D3 shader failures and basic mode

- Base: `fbcdc5e11be8a9dc46471b8ac0990cb603982740` (includes leader #11).
- Shared production compiler checks resource/compile/link errors in Release and
  propagates failure before allocating font/view resources. Keep the original
  desktop GLSL110 and GLES300ES prefixes and all nine default material pairs.
- Default-OFF `LEOCAD_SAFE_RENDER` / runtime `LEOCAD_SAFE_RENDER=1` disables
  optional lighting, conditional edges, step effects and grid; GLES keeps basic
  shaders. This is an explicit fallback, not the grid-depth fix for issue #12.
- Real shader tests and complete Release negative cases are host verified.
  Target GPU validation is still pending; see D3 result and device handoff.
- Rollback: revert this change; restoring effects alone requires OFF and no runtime 1.

See [D3 result](../../docs/MEMBER_A_D3.md) and
[device handoff](../../docs/MEMBER_A_D3_DEVICE_HANDOFF.md).

## 2026-10-30: D4 reference grid / depth and framebuffer

- Base: `1dfc63cfbc855fb524097a42f7a8d252708e338d`.
- The fixed model spans Z=-24..4 while the grid is Z=0. Drawing the reference
  grid after solids lets it cover the body. Draw it once as background, with no
  depth writes; preserve the original mask and construction/picking coordinates.
- Restore complete frame depth mapping/clear state. This is a separate guard;
  the host repro does not attribute the device bug to leaked driver state.
- Log actual widget FBO/depth on first paint and resize/DPR/FBO changes, rather
  than treating the context format as proof of attached depth.
- Host real-widget regression: 9/9. Full application two-size comparison shows
  zero changed model interior pixels after repair, with the grid still visible.
- New normal-mode HarmonyOS validation is pending; do not auto-close #12.

See [D4 result](../../docs/MEMBER_A_D4.md) and
[device handoff](../../docs/MEMBER_A_D4_DEVICE_HANDOFF.md).
