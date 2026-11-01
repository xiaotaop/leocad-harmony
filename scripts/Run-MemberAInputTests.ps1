param([string]$QtRoot = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$QtRoot) { $QtRoot = Join-Path $root '.local\Qt\6.8.3\msvc2022_64' }
$qmake = Join-Path $QtRoot 'bin\qmake.exe'
if (!(Test-Path -LiteralPath $qmake)) { throw "Qt not found: $qmake" }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ build tools were not found.' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$savedEnvironment = @{}
Get-ChildItem Env: | ForEach-Object { $savedEnvironment[$_.Name] = $_.Value }
$build = Join-Path $root '.local\build-input'
New-Item -ItemType Directory -Force -Path $build | Out-Null
try {
    $vcEnvironment = & $env:ComSpec /d /c "call `"$vcvars`" >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize MSVC.' }
    foreach ($line in $vcEnvironment) {
        if ($line -match '^([^=]+)=(.*)$') { Set-Item -LiteralPath "Env:$($matches[1])" -Value $matches[2] }
    }
    $env:PATH = (Join-Path $QtRoot 'bin') + ';' + $env:PATH
    $env:QT_QPA_PLATFORM = 'windows'
    $env:QT_OPENGL = 'software'
    Remove-Item Env:QT_PLUGIN_PATH, Env:QT_QPA_PLATFORM_PLUGIN_PATH -ErrorAction SilentlyContinue
    Push-Location $build
    try {
        & $qmake (Join-Path $root 'member-a-input-tests.pro') 'CONFIG+=release' 'CONFIG-=debug_and_release' 'QMAKE_CXXFLAGS+=/MP8' *> build.log
        if ($LASTEXITCODE -ne 0) { throw 'qmake failed for input tests.' }
        & nmake.exe /NOLOGO *>> build.log
        if ($LASTEXITCODE -ne 0) { Get-Content build.log -Tail 40; throw 'Input test build failed.' }
        $binary = Join-Path $build 'build\release\leocad-input-tests.exe'
        foreach ($mode in @('normal', 'dpr2', 'basic-dpr2')) {
            $env:QT_SCALE_FACTOR = if ($mode -eq 'normal') { '1' } else { '2' }
            $env:LEOCAD_SAFE_RENDER = if ($mode -eq 'basic-dpr2') { '1' } else { '0' }
            & $binary -o "$mode-results.txt,txt" -o "$mode-results.xml,junitxml" *> "$mode-renderer.log"
            $testExit = $LASTEXITCODE
            Get-Content "$mode-results.txt"
            if ($testExit -ne 0) { throw "Input tests failed: $mode exit $testExit" }
            if (Select-String -Path "$mode-results.txt" -Pattern '^FAIL!|^SKIP' -Quiet) { throw "Input suite failed or skipped a case: $mode" }
        }
    } finally { Pop-Location }
} finally {
    Get-ChildItem Env: | Where-Object { !$savedEnvironment.ContainsKey($_.Name) } | Remove-Item
    foreach ($entry in $savedEnvironment.GetEnumerator()) { Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value }
}
