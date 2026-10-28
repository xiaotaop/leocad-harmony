# Curated LDraw library

Owner: Member C.

The first release packages a curated offline subset of the official LDraw parts
library: **1160 top-level parts** (plus 669 subparts, 911 primitives and the
colour configuration), 2741 files in a 3.66 MB archive.

## Contents

| Path | Purpose |
|---|---|
| `selection.txt` | Seed part ids, generated per LeoCAD category. Do not hand edit. |
| `library-lite.zip` | The packaged subset. Committed; this is what ships. |
| `manifest.json` | Single source of truth: provenance, per-file SHA-256, licence, categories. |

The full official archive (~140 MB) is **not** committed. It is cached outside
the repository at `~/.cache/leocad-harmony/ldraw/` (override with
`LEOCAD_LDRAW_CACHE`).

## Regenerating

```bash
python3 scripts/library/fetch_ldraw.py          # download or reuse the cache
python3 scripts/library/generate_selection.py   # rewrite selection.txt
python3 scripts/library/build_library.py        # rewrite library-lite.zip + manifest.json
python3 scripts/library/verify_library.py       # six checks
```

Builds are byte-reproducible: same seed list + same official archive produces an
identical zip.

## Why a generator instead of a hand-written list

LeoCAD's parts tree comes from the category expressions in
`common/lc_category.cpp` matched against part descriptions, not from the parts'
`!CATEGORY` metadata (which is sparse: only 6470 of 24985 top-level parts carry
it). `generate_selection.py` reads those expressions straight out of the C++
source, so the seed list and the tree cannot drift apart.

## Archive layout is not free-form

`lcPiecesLibrary::OpenArchive()` requires an `ldraw/` root with `parts/` and `p/`
below it, and `LoadColors()` extracts `ldraw/ldconfig.ldr` from the same archive.
An archive that does not follow this silently loses parts or colours.
`tests/library/test_library_manifest.py` asserts the layout.

## Licensing

The parts are redistributed under **CC BY 4.0** (193 files are dual-licensed
CC BY 2.0 + 4.0). Attribution obligations and the per-file record are documented
in [`docs/library/零件库清单与许可.md`](../../docs/library/零件库清单与许可.md).

Do not add part files directly: add them to the selection generator, rebuild, and
let the manifest record their provenance and licence.
