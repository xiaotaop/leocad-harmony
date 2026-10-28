#!/usr/bin/env bash
# Run every check Member C owns, in the order a pull request should pass them.
#
# Usage:
#   scripts/ci/run_library_checks.sh              # all checks
#   scripts/ci/run_library_checks.sh --quick      # skip the parts-library build
#
# The parts-library build is skipped by default when the cache is absent, so this
# script is usable on a machine that has never downloaded the official library;
# pass --build-library to require it.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

QUICK=0
BUILD_LIBRARY=0
for argument in "$@"; do
    case "$argument" in
        --quick) QUICK=1 ;;
        --build-library) BUILD_LIBRARY=1 ;;
        *) echo "unknown option: $argument" >&2; exit 2 ;;
    esac
done

FAILED=0
step() {
    printf '\n=== %s ===\n' "$1"
}

run() {
    local description="$1"; shift
    if "$@"; then
        printf 'PASS  %s\n' "$description"
    else
        printf 'FAIL  %s\n' "$description"
        FAILED=1
    fi
}

step "Parts library artefact"
if [ "$BUILD_LIBRARY" -eq 1 ]; then
    run "build curated library" python3 scripts/library/build_library.py
elif [ "$QUICK" -eq 0 ]; then
    run "verify curated library" python3 scripts/library/verify_library.py
else
    echo "skipped (--quick)"
fi

step "Qt resources"
run "qrc resources exist" python3 scripts/library/check_qrc_resources.py

step "Translations"
run "translation coverage" python3 scripts/l10n/check_translations.py

step "Unit tests"
run "library tests" python3 -m unittest discover -s tests/library -t . -v

step "Source formatting"
# The repository normalises these files in .gitattributes; a CRLF or BOM in a
# generated file makes review diffs unreadable and breaks lrelease's strictness.
python3 - <<'PY'
import sys
from pathlib import Path

problems = []
checked = [
    Path("resources/leocad_zh_CN.ts"),
    Path("assets/ldraw-lite/manifest.json"),
    Path("assets/ldraw-lite/selection.txt"),
]
for path in checked:
    if not path.exists():
        problems.append(f"{path}: missing")
        continue
    raw = path.read_bytes()
    if raw.startswith(b"\xef\xbb\xbf"):
        problems.append(f"{path}: UTF-8 BOM")
    if b"\r\n" in raw:
        problems.append(f"{path}: CRLF line endings")

for problem in problems:
    print(f"  {problem}")
sys.exit(1 if problems else 0)
PY
if [ $? -eq 0 ]; then
    printf 'PASS  generated files use UTF-8 + LF\n'
else
    printf 'FAIL  generated files use UTF-8 + LF\n'
    FAILED=1
fi

printf '\n'
if [ "$FAILED" -ne 0 ]; then
    echo "RESULT: FAILED"
    exit 1
fi
echo "RESULT: PASSED"
