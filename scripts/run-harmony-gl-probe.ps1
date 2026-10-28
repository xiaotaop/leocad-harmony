[CmdletBinding()]
param(
    [string]$SdkRoot = $env:DEVECO_SDK_HOME,
    [string]$QtRoot = $env:QT_ROOT,
    [string]$BuildDirectory,
    [string]$Device,
    [switch]$GenerateSignature,
    [switch]$Run
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'HarmonyProbeTools.ps1')
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path

if (-not $SdkRoot) {
    throw 'DEVECO_SDK_HOME is not set. Pass -SdkRoot or set the environment variable.'
}
if (-not $QtRoot) {
    throw 'QT_ROOT is not set. Pass -QtRoot pointing to Qt 6.12 for HarmonyOS arm64-v8a.'
}
$SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
$QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $repositoryRoot 'build\harmony-render-probe'
}

$qtCMake = Join-Path $QtRoot 'bin\qt-cmake.bat'
$cmake = Join-Path $SdkRoot 'default\openharmony\native\build-tools\cmake\bin\cmake.exe'
$ninja = Join-Path $SdkRoot 'default\openharmony\native\build-tools\cmake\bin\ninja.exe'
$hvigor = Join-Path (Split-Path $SdkRoot -Parent) 'bin\hvigorw.bat'
foreach ($required in @($qtCMake, $cmake, $ninja, $hvigor)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required tool not found: $required"
    }
}

if ($Run -and -not $GenerateSignature) {
    throw '-Run requires -GenerateSignature so the commercial device receives a trusted debug package.'
}

Invoke-WithHarmonyProbeToolchain -SdkRoot $SdkRoot -Action {
    & (Join-Path $PSScriptRoot 'check-toolchain.ps1') -SdkRoot $SdkRoot -QtRoot $QtRoot -RequireQt
    if (-not $?) {
        throw 'Toolchain validation failed.'
    }

    $sourceDirectory = Join-Path $repositoryRoot 'tests\render'
    & $qtCMake -S $sourceDirectory -B $BuildDirectory -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja"
    if ($LASTEXITCODE -ne 0) { throw 'Qt/CMake configuration failed.' }

    & $cmake --build $BuildDirectory --target leocad-render-probe_make_hap --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'HarmonyOS HAP build failed.' }

    $unsignedHap = Join-Path $BuildDirectory 'harmonyos-build\entry\build\default\outputs\default\entry-default-unsigned.hap'
    if (-not (Test-Path -LiteralPath $unsignedHap -PathType Leaf)) {
        throw "Expected unsigned HAP was not produced: $unsignedHap"
    }
    $hash = Get-FileHash -LiteralPath $unsignedHap -Algorithm SHA256
    Write-Host ''
    Write-Host "Unsigned HAP: $unsignedHap"
    Write-Host "SHA-256:     $($hash.Hash)"

    if ($GenerateSignature) {
        $devEcoCli = Get-Command devecocli -ErrorAction SilentlyContinue
        if (-not $devEcoCli) { throw 'devecocli was not found. Install @deveco/deveco-cli and sign in first.' }
        $harmonyProject = Join-Path $BuildDirectory 'harmonyos-build'
        Push-Location $harmonyProject
        try {
            & $devEcoCli.Source signature generate --force --product default
            if ($LASTEXITCODE -ne 0) {
                throw 'DevEco CLI could not generate a trusted debug signature. Check login, developer verification, bundle name and device registration.'
            }
            if ($Run) {
                $arguments = @('run', '--module', 'entry@default', '--product', 'default', '--build-mode', 'debug', '--ability', 'QAbility')
                if ($Device) { $arguments += @('--device', $Device) }
                & $devEcoCli.Source @arguments
                if ($LASTEXITCODE -ne 0) { throw 'DevEco CLI build/install/launch failed.' }
            }
        }
        finally { Pop-Location }
    }
}
