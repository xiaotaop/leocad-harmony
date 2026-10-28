# Localisation tooling

Owner: Member C.

## Workflow

```bash
source .devtools/env.sh
# Target the Chinese catalogue only. `lupdate leocad.pro` rewrites all eight
# language files, which would drag unrelated churn into the pull request.
lupdate common qt -no-obsolete -locations none -ts resources/leocad_zh_CN.ts
python3 scripts/l10n/fill_zh_cn.py                 # fill known translations (idempotent)
lrelease resources/leocad_zh_CN.ts -qm resources/leocad_zh_CN.qm
python3 scripts/l10n/check_translations.py         # gate
```

## Why the checked-in `.ts` needed regenerating

The file shipped in the repository was produced from an older revision: every
`<location line=...>` was stale and several user-facing strings were simply
absent (`Filter Parts`, `Change Selection Color`, `Save Project`, the empty-state
messages). Its `type="unfinished"` count read zero, which made it look complete.
`check_translations.py` therefore compares against the strings that actually exist
in the sources rather than trusting that attribute.

## Why stored names are never translated

Colour and category names live in fixed-size `char` buffers and are written back
out by the exporters (`SafeName` becomes an MTL/COLLADA/POV/CSV identifier).
Putting CJK there would corrupt exported files and truncate mid-character. Both
`lcGetColorDisplayName()` and `lcGetCategoryDisplayName()` therefore translate
only at display time, and `test_translations.py` asserts the stored buffers are
untouched.

## Plural messages

Qt expects `<numerusform>` children for `numerus="yes"` messages. Writing plain
text into them makes `lrelease` fail with "Unexpected characters". `fill_zh_cn.py`
normalises them on every run so this cannot regress.

## Deliberately untranslated

Command-line option help (the HarmonyOS build is launched from the home screen,
never with arguments) and Blender/POV-Ray strings (those dialogs are out of
first-release scope). Both are recorded in `docs/release/已知问题.md`.
