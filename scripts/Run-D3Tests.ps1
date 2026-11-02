# Build and run the D3 integration tests on Windows (MSVC), for CI.
#
# Why this exists: the D3 suites live in four separate qmake projects, and the
# existing model CI never built them, so a green run said nothing about them
# (review P2). This script is the CI entry point; scripts/dev/run-d3-tests.sh is
# the bash equivalent for local Linux work.
#
# Usage:
#   ./scripts/Run-D3Tests.ps1 -QtRoot $env:QT_ROOT_DIR
#
# Exits non-zero if any suite fails. Each suite's log is kept under
# .local/build-d3-<suite>/ for the artifact upload.

param(
    [string]$QtRoot = '',
    [string[]]$Suite = @('archive', 'priority', 'menu', 'modellist', 'panel', 'preferences', 'theme', 'chain', 'localization', 'files')
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$QtRoot) { $QtRoot = Join-Path $root '.local\Qt\6.8.3\msvc2022_64' }
$qmake = Join-Path $QtRoot 'bin\qmake.exe'
if (!(Test-Path -LiteralPath $qmake)) { throw "Qt not found: $qmake." }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ build tools were not found.' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'

# Regenerate the broken-archive fixtures from the current release archive so they
# cannot go stale.
& python (Join-Path $root 'tests\library\d3\prepare_variants.py')
if ($LASTEXITCODE -ne 0) { throw 'Could not generate archive variants.' }

$failed = @()

foreach ($name in $Suite) {
    # The D4 panel suite lives beside the D3 ones but belongs to the next
    # milestone, so it keeps its own project name rather than being numbered D3.
    if ($name -eq 'panel') { $project = "d4-$name-tests.pro"; $target = "leocad-d4-$name" }
    elseif ($name -eq 'preferences' -or $name -eq 'theme') { $project = "d5-$name-tests.pro"; $target = "leocad-d5-$name" }
    elseif ($name -eq 'chain') { $project = "d6-$name-tests.pro"; $target = "leocad-d6-$name" }
    elseif ($name -eq 'localization') { $project = "d7-$name-tests.pro"; $target = "leocad-d7-$name" }
    elseif ($name -eq 'files') { $project = "d7-files-tests.pro"; $target = "leocad-d7-files" }
    else { $project = "d3-$name-tests.pro"; $target = "leocad-d3-$name" }
    if (!(Test-Path -LiteralPath (Join-Path $root $project))) { throw "No such suite: $name" }

    $build = Join-Path $root ".local\build-d3-$name"
    Remove-Item -Recurse -Force $build -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $build | Out-Null

    $previousEnvironment = @{}
    Get-ChildItem Env: | ForEach-Object { $previousEnvironment[$_.Name] = $_.Value }
    try {
        $vcEnvironment = & $env:ComSpec /d /c "call `"$vcvars`" >nul && set"
        if ($LASTEXITCODE -ne 0) { throw 'Could not initialize MSVC.' }
        foreach ($line in $vcEnvironment) {
            if ($line -match '^([^=]+)=(.*)$') { Set-Item -LiteralPath "Env:$($matches[1])" -Value $matches[2] }
        }
        $env:PATH = (Join-Path $QtRoot 'bin') + ';' + $env:PATH
        # These suites open a window (or a dialog) but must not need a desktop.
        $env:QT_QPA_PLATFORM = 'offscreen'
        $env:QT_OPENGL = 'software'

        Push-Location $build
        try {
            & $qmake (Join-Path $root $project) 'CONFIG+=release' 'CONFIG-=debug_and_release' 'QMAKE_CXXFLAGS+=/MP8' *> build.log
            if ($LASTEXITCODE -ne 0) { Get-Content build.log -Tail 40; throw "qmake failed for $name" }
            & nmake.exe /NOLOGO *>> build.log
            if ($LASTEXITCODE -ne 0) { Get-Content build.log -Tail 40; throw "build failed for $name" }
        }
        finally { Pop-Location }

        $binary = Join-Path $build "build\release\$target.exe"
        if (!(Test-Path -LiteralPath $binary)) { throw "binary not produced for $name" }

        # QtTest's own -o, not PowerShell '*>' redirection: on the runner, native
        # command output through '*>' landed in a zero-byte file, so every suite
        # "passed" from an empty test-results.txt (an empty file contains no
        # '^FAIL!' line) and the only real signal left was the exit code — a
        # failing suite said nothing about what failed. Build-MemberB.ps1 already
        # writes results with -o and its file is never empty. The console line
        # below exists because a crash before the first flush leaves the file
        # empty, and then the exit code is all there is to go on.
        $results = Join-Path $build 'test-results.txt'
        & $binary -o "$results,txt"
        $exit = $LASTEXITCODE
        Write-Output ('{0}: exit={1}, {2} bytes of results' -f $name, $exit, (Get-Item $results -ErrorAction SilentlyContinue).Length)
        Get-Content $results -Tail 3

        # QtTest exits non-zero on failure; also treat a printed FAIL! as failure so
        # a crash after the summary cannot pass.
        if ($exit -ne 0 -or (Select-String -Path $results -Pattern '^FAIL!' -Quiet)) {
            $failed += $name
        }
    }
    finally {
        foreach ($key in $previousEnvironment.Keys) { Set-Item -LiteralPath "Env:$key" -Value $previousEnvironment[$key] }
    }
}

if ($failed.Count -gt 0) { throw "D3 suites failed: $($failed -join ', ')" }
Write-Output "All D3 suites passed: $($Suite -join ', ')"
