# Early-exit resource diagnostics regression

The production `qt/qtmain.cpp` logs Harmony resource paths after `Initialize()`.
`--version` and `--help` return `Success` before creating the parts library, so
the diagnostic must represent `loaded_parts` as JSON null in those cases.

Build the actual desktop application with only this Harmony diagnostic block
enabled (not the other Harmony startup/GL paths):

```sh
cmake -S . -B build/desktop -G Ninja -DCMAKE_BUILD_TYPE=Release -DLEOCAD_TEST_RESOURCE_PATHS=ON
cmake --build build/desktop --parallel 2
ctest --test-dir build/desktop --output-on-failure
```

The option is off by default and has no effect on Harmony builds, which always
include the diagnostic. Host tests run the real application as subprocesses:

- `--version` and `--help` must print the requested text, exit 0 and emit exactly
  one resource report with `loaded_parts: null`.
- An unknown option must exit 1 with its error and no resource report.

Requiring the diagnostic prevents a normal desktop build that excludes the
Harmony block from falsely passing. A timeout, crash, missing output or missing
JSON field fails the test. Raw stdout/stderr and results are written to
`build/desktop/startup-results.json`, also uploaded by the desktop CI job.

The host test does not load a library or construct the main window. It covers
the startup lifecycle regression, not Harmony Ability behavior or device GL.
