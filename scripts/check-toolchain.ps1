[CmdletBinding()]
param(
    [string]$SdkRoot = $env:DEVECO_SDK_HOME,
    [string]$QtRoot = $env:QT_ROOT,
    [switch]$RequireQt
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()

function Write-Check([string]$Name, [string]$Value, [bool]$Ok) {
    $mark = if ($Ok) { '[OK]' } else { '[--]' }
    Write-Host ("{0,-4} {1,-24} {2}" -f $mark, $Name, $Value)
}

if (-not $SdkRoot) {
    $failures.Add('DEVECO_SDK_HOME is not set. Use -SdkRoot or set the environment variable.')
} else {
    $SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
    $sdkPackage = Join-Path $SdkRoot 'default\sdk-pkg.json'
    if (-not (Test-Path -LiteralPath $sdkPackage)) {
        $failures.Add("SDK metadata not found: $sdkPackage")
    } else {
        $sdk = Get-Content -LiteralPath $sdkPackage -Raw | ConvertFrom-Json
        Write-Check 'HarmonyOS SDK' "$($sdk.data.version) (API $($sdk.data.apiVersion))" $true
    }
}

$nativeRoot = if ($SdkRoot) { Join-Path $SdkRoot 'default\openharmony\native' } else { '' }
$tools = @(
    @{ Name = 'CMake'; Path = Join-Path $nativeRoot 'build-tools\cmake\bin\cmake.exe'; Args = '--version' },
    @{ Name = 'Ninja'; Path = Join-Path $nativeRoot 'build-tools\cmake\bin\ninja.exe'; Args = '--version' },
    @{ Name = 'Clang'; Path = Join-Path $nativeRoot 'llvm\bin\clang.exe'; Args = '--version' }
)

foreach ($tool in $tools) {
    $exists = $tool.Path -and (Test-Path -LiteralPath $tool.Path)
    if ($exists) {
        $firstLine = (& $tool.Path $tool.Args 2>&1 | Select-Object -First 1)
        Write-Check $tool.Name $firstLine $true
    } else {
        Write-Check $tool.Name 'not found' $false
        $failures.Add("$($tool.Name) not found in HarmonyOS native SDK.")
    }
}

$hvigorCandidates = @()
if ($SdkRoot) {
    $commandLineToolsRoot = Split-Path $SdkRoot -Parent
    $hvigorCandidates += Join-Path $commandLineToolsRoot 'bin\hvigorw.bat'
}
if ($env:DEVECO_CLI_CLT_PATH) {
    $hvigorCandidates += Join-Path $env:DEVECO_CLI_CLT_PATH 'bin\hvigorw.bat'
    $cliVersionRoot = Split-Path $env:DEVECO_CLI_CLT_PATH -Parent
    $cliInstallRoot = Split-Path $cliVersionRoot -Parent
    $hvigorCandidates += Join-Path $cliInstallRoot 'command-line-tools\bin\hvigorw.bat'
}
$hvigor = $hvigorCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if ($hvigor) {
    $hvigorVersion = (& $hvigor --version 2>&1 | Select-Object -First 1)
    Write-Check 'Hvigor' "$hvigorVersion ($hvigor)" $true
} else {
    Write-Check 'Hvigor' 'not found' $false
    $failures.Add('hvigorw.bat not found. Check DEVECO_CLI_CLT_PATH.')
}

$qtCandidates = @($QtRoot, $env:QT_HOST_PATH)
if ($env:CMAKE_PREFIX_PATH) {
    $qtCandidates += $env:CMAKE_PREFIX_PATH -split [IO.Path]::PathSeparator
}
$qtConfig = $qtCandidates |
    Where-Object { $_ } |
    ForEach-Object { Get-ChildItem -LiteralPath $_ -Filter Qt6Config.cmake -Recurse -ErrorAction SilentlyContinue } |
    Select-Object -First 1

if ($qtConfig) {
    Write-Check 'Qt for HarmonyOS' $qtConfig.FullName $true
} else {
    Write-Check 'Qt for HarmonyOS' '6.12.x not found' $false
    if ($RequireQt) {
        $failures.Add('Qt for HarmonyOS is required but was not found.')
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot '..\CMakeLists.txt'))) {
    $failures.Add('Root CMakeLists.txt is missing.')
}

if ($failures.Count -gt 0) {
    Write-Host ''
    $failures | ForEach-Object { Write-Error $_ -ErrorAction Continue }
    exit 1
}

Write-Host ''
Write-Host 'Toolchain baseline checks passed.'
