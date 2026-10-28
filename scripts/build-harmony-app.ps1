[CmdletBinding()]
param(
    [string]$SdkRoot = $env:DEVECO_SDK_HOME,
    [string]$QtRoot = $env:QT_ROOT,
    [string]$BuildDirectory,
    [ValidateSet('App', 'ModelTests')][string]$Target = 'App',
    [switch]$GenerateSignature,
    [switch]$Run,
    [string]$Device
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'HarmonyProbeTools.ps1')
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $SdkRoot -or -not $QtRoot) { throw 'Pass -SdkRoot and -QtRoot, or set DEVECO_SDK_HOME and QT_ROOT.' }
$SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
$QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
if ($Run -and -not $GenerateSignature) { throw '-Run requires -GenerateSignature.' }
if ($Run -and $Target -eq 'ModelTests') {
    throw 'Model tests are intentionally short-lived. Build/sign here, then use HDC and inspect model-tests.txt; CLI liveness is not a test verdict.'
}
if (-not $BuildDirectory) {
    $directoryName = if ($Target -eq 'App') { 'harmony-d3' } else { 'harmony-d3-modeltests' }
    $BuildDirectory = Join-Path $repositoryRoot "build/$directoryName"
}
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$qtCMake = Join-Path $QtRoot 'bin/qt-cmake.bat'
$cmake = Join-Path $SdkRoot 'default/openharmony/native/build-tools/cmake/bin/cmake.exe'
$ninja = Join-Path $SdkRoot 'default/openharmony/native/build-tools/cmake/bin/ninja.exe'
$hvigor = Join-Path (Split-Path $SdkRoot -Parent) 'bin/hvigorw.bat'
foreach ($tool in @($qtCMake, $cmake, $ninja, $hvigor)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "Tool missing: $tool" }
}
$modelTests = if ($Target -eq 'ModelTests') { 'ON' } else { 'OFF' }
$buildTarget = if ($Target -eq 'ModelTests') { 'leocad-model-tests_make_hap' } else { 'leocad_make_hap' }

Invoke-WithHarmonyProbeToolchain -SdkRoot $SdkRoot -Action {
    & $qtCMake -S $repositoryRoot -B $BuildDirectory -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" `
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "-DLEOCAD_BUILD_MODEL_TEST_HAP=$modelTests"
    if ($LASTEXITCODE -ne 0) { throw 'HarmonyOS CMake configuration failed.' }
    & $cmake --build $BuildDirectory --target $buildTarget --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'HarmonyOS HAP build failed.' }

    if ($GenerateSignature) {
        $cli = Get-Command devecocli -ErrorAction Stop
        Push-Location (Join-Path $BuildDirectory 'harmonyos-build')
        try {
            # harmonydeployqt regenerates build-profile.json5. Generate signing
            # configuration AFTER deployment, even if this directory was signed before.
            & $cli.Source signature generate --product default
            if ($LASTEXITCODE -ne 0) { throw 'Official debug-signing setup failed.' }
            if ($Run) {
                $arguments = @('run', '--module', 'entry@default', '--product', 'default',
                    '--build-mode', 'debug', '--ability', 'QAbility')
                if ($Device) { $arguments += @('--device', $Device) }
                & $cli.Source @arguments
                if ($LASTEXITCODE -ne 0) { throw 'Build/install/launch failed.' }
            } else {
                & $hvigor assembleHap -p buildMode=debug --no-daemon
                if ($LASTEXITCODE -ne 0) { throw 'Signed HAP build failed.' }
            }
        } finally { Pop-Location }
    }
    $name = if ($GenerateSignature) { 'entry-default-signed.hap' } else { 'entry-default-unsigned.hap' }
    $hap = Join-Path $BuildDirectory "harmonyos-build/entry/build/default/outputs/default/$name"
    if (-not (Test-Path -LiteralPath $hap -PathType Leaf)) { throw "Missing HAP: $hap" }
    Get-FileHash -LiteralPath $hap -Algorithm SHA256
}
