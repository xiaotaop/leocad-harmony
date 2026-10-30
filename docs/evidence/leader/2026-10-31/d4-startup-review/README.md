# PR #26 startup review correction

Date: 2026-10-31 (Asia/Shanghai). Reviewed head: `101ed826`.
Corrected code: `5b79fdd6d2e2a64f8baa06d7be0bae699b60d1e3`.

This directory contains host lifecycle evidence and an ARM64 compilation log.
It contains no new device run or signed HAP. Original D4 device evidence remains
unchanged and belongs to `fcb8fe55` (and the earlier integrated baseline).

## Before

Build the full host application with `LEOCAD_TEST_RESOURCE_PATHS=ON`, enabling
only the new resource diagnostic block and its includes. The unsafe block
matches the reviewed entry point, with the two preprocessor changes recorded
in `before-host-instrumentation.patch`. The same CMake test setup was used for
the before and after builds; all other host startup/GL branches remain host
branches. The instrumented CRLF source hash is fixed in `validation.json`.

Run the resulting application under GDB with a command file containing:

```text
set pagination off
run --version
bt 6
```

Repeat with `run --help`. Both raw transcripts show requested output followed
by SIGSEGV in `main`. GDB stopped before Windows' crash dialog; GDB's own exit
status is not the application exit status. These are host crash reproductions,
not Harmony crash logs. The unguarded executable remains local and is hashed in
`validation.json`.

## After

The corrected diagnostic emits `loaded_parts: null` when no library exists.
Existing non-null library count behavior is retained. Full host rebuild and:

```text
ctest --test-dir build/windows-review --output-on-failure
```

passed 1/1 CTest test containing three real subprocess cases:
`--version` exit 0, `--help` exit 0, unknown option exit 1. The first two require
exactly one real diagnostic with a null count; the error path requires none.
Raw stdout/stderr are in `startup-results.json`; CTest and build logs are kept.
The desktop CI job runs the same test; see `tests/startup/README.md`.

ARM64 validation compiled and linked the actual `leocad` target, with the host
test switch OFF and the normal `Q_OS_HARMONY` block enabled. `arm64-build.log`
shows the rebuilt entry object and `libleocad.so` link. This did not run HAP
packaging, signing, installation or device execution. The `.so` hash is not a
replacement for the original D4 HAP hash.

`validation.json` fixes source blob hashes, local binary hashes, toolchain and
scope. `receipt.json` fixes every evidence file's original bytes except itself.
