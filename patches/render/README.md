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
