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
