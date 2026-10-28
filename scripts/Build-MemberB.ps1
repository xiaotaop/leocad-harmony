param(
    [ValidateSet('App', 'Tests')][string]$Target = 'Tests',
    [string]$QtRoot = '',
    [switch]$RunTests,
    [switch]$Clean
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$QtRoot) { $QtRoot = Join-Path $root '.local\Qt\6.8.3\msvc2022_64' }
$qmake = Join-Path $QtRoot 'bin\qmake.exe'
if (!(Test-Path -LiteralPath $qmake)) { throw "Qt not found: $qmake. See docs/MEMBER_B_START.md." }
if ($RunTests -and $Target -ne 'Tests') { throw '-RunTests requires -Target Tests.' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ build tools were not found.' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$savedEnvironment = @{}
Get-ChildItem Env: | ForEach-Object { $savedEnvironment[$_.Name] = $_.Value }
$build = Join-Path $root ".local\build-$($Target.ToLowerInvariant())"
New-Item -ItemType Directory -Force -Path $build, (Join-Path $root '.local\tmp') | Out-Null
try {
    $vcEnvironment = & $env:ComSpec /d /c "call `"$vcvars`" >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize MSVC.' }
    foreach ($line in $vcEnvironment) {
        if ($line -match '^([^=]+)=(.*)$') { Set-Item -LiteralPath "Env:$($matches[1])" -Value $matches[2] }
    }
    $env:PATH = (Join-Path $QtRoot 'bin') + ';' + $env:PATH
    $env:TEMP = Join-Path $root '.local\tmp'
    $env:TMP = $env:TEMP
    Remove-Item Env:QT_PLUGIN_PATH, Env:QT_QPA_PLATFORM_PLUGIN_PATH -ErrorAction SilentlyContinue
    Push-Location $build
    try {
        $project = if ($Target -eq 'Tests') { 'member-b-tests.pro' } else { 'leocad.pro' }
        # qmake does not invalidate existing moc outputs when DEFINES change.
        if ($Clean -and (Test-Path -LiteralPath 'Makefile')) {
            & nmake.exe /NOLOGO clean *> (Join-Path $build 'clean.log')
            if ($LASTEXITCODE -ne 0) { throw "Clean failed; see $build\clean.log" }
        }
        & $qmake (Join-Path $root $project) 'CONFIG+=release' 'CONFIG-=debug_and_release' 'QMAKE_CXXFLAGS+=/MP8'
        if ($LASTEXITCODE -ne 0) { throw 'qmake failed.' }
        & nmake.exe /NOLOGO *> (Join-Path $build 'build.log')
        if ($LASTEXITCODE -ne 0) {
            Get-Content (Join-Path $build 'build.log') -Tail 45
            throw "Build failed; see $build\build.log"
        }
        Write-Output "Build passed: $Target; log: $build\build.log"
        if ($Target -eq 'Tests') {
            $dependencies = & dumpbin.exe /NOLOGO /DEPENDENTS (Join-Path $build 'build\release\leocad-model-tests.exe')
            if ($LASTEXITCODE -ne 0) { throw 'Could not inspect model test dependencies.' }
            $dependencies | Set-Content -LiteralPath (Join-Path $build 'dependencies.txt')
            if ($dependencies -match '(?i)Qt[56]Network(?:d)?\.dll|WININET\.dll') {
                throw 'Offline model tests still import a network library.'
            }
        }
        if ($RunTests) {
            # No visible window, but upstream library teardown requires desktop GL.
            $env:QT_QPA_PLATFORM = 'windows'
            & (Join-Path $build 'build\release\leocad-model-tests.exe') -o "$(Join-Path $build 'test-results.txt'),txt"
            $testExit = $LASTEXITCODE
            Get-Content (Join-Path $build 'test-results.txt')
            if ($testExit -ne 0) { throw "Tests failed with exit code $testExit." }
        }
    } finally { Pop-Location }
} finally {
    Get-ChildItem Env: | Where-Object { !$savedEnvironment.ContainsKey($_.Name) } | Remove-Item
    foreach ($entry in $savedEnvironment.GetEnumerator()) { Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value }
}
