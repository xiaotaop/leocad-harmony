# Library, UI and localisation tests

Owner: Member C.

Covers the checks the development plan assigns to Member C: parts-library
manifest integrity, Qt resource wiring, translation coverage, and the
first-release UI scope.

## Running

```bash
python3 -m unittest discover -s tests/library -t . -v
```

No third-party dependencies: the tests use the standard library only, so they run
on a machine that has never built Qt.

## What is covered

| File | Focus |
|---|---|
| `test_library_manifest.py` | File existence, id uniqueness, licence completeness, category coverage, dependency closure, archive layout, reproducibility, manifest provenance |
| `test_qrc_resources.py` | `.qrc` entries exist (`.qm` files are build-generated), curated library alias, bundled-library probe contract |
| `test_translations.py` | Required contexts fully translated, plurals use `<numerusform>`, UTF-8 + LF, translations never leak into stored colour names |
| `test_ui_scope.py` | Removed commands stay removed, core commands stay reachable, dock pointers not nulled, no startup-time ordering regression, no network path |

## Notes

- `test_ui_scope.py::NetworkScopeTests` currently **skips** on one assertion. That
  is deliberate: it tracks the known gap that `LC_DISABLE_UPDATE_CHECK=1` is only
  defined for the macOS build, so the HarmonyOS build would attempt an update
  check. See `docs/release/权限说明.md` §3.1. Once the team lead adds the define,
  the skip turns into a passing assertion automatically.
- These are static/structural tests. The runtime half of the UI check lives in
  `scripts/dev/verify_menu_scope.py` and needs a display.
