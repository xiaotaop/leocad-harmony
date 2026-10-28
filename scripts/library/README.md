# Parts library tooling

Owner: Member C.

Scripts that build, validate and package the curated LDraw subset shipped with
the first release. Standard library only; no Qt, no pip install.

## Pipeline

```text
fetch_ldraw.py          -> ~/.cache/leocad-harmony/ldraw/complete.zip (outside the repo)
generate_selection.py   -> assets/ldraw-lite/selection.txt
build_library.py        -> assets/ldraw-lite/library-lite.zip + manifest.json
verify_library.py       -> six checks, exit non-zero on any problem
check_qrc_resources.py  -> every .qrc entry exists (.qm excepted, they are built)
```

## Notes that matter

- **`library.ldraw.org` rejects the default urllib User-Agent with 403**, and
  answers HEAD with 403 as well. `fetch_ldraw.py` sends an explicit User-Agent and
  probes with a one-byte ranged GET instead of HEAD.
- **The archive layout is dictated by the loader.** `lcPiecesLibrary::OpenArchive`
  needs an `ldraw/` root with `parts/`, `parts/s/` and `p/`; `LoadColors()` reads
  `ldraw/ldconfig.ldr` from the archive. `build_library.py` asserts this.
- **Builds are byte-reproducible.** Zip entries are sorted, timestamps fixed,
  compression level fixed. Two runs produce the same SHA-256.
- **Dependency closure is enforced.** Every type-1 reference is resolved
  transitively; a missing include aborts the build rather than shipping a part
  that renders with holes.
- **Category seeds follow LeoCAD, not `!CATEGORY`.** `generate_selection.py` parses
  `DefaultCategories[]` out of `common/lc_category.cpp`.

See [`docs/library/零件库清单与许可.md`](../../docs/library/零件库清单与许可.md)
for the numbers and the licensing obligations.
