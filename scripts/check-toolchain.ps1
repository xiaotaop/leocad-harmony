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

if ($env:QT_HOST_PATH) {
    $hostQtConfig = Join-Path $env:QT_HOST_PATH 'lib\cmake\Qt6\Qt6Config.cmake'
    Write-Check 'Qt host tools' $env:QT_HOST_PATH (Test-Path -LiteralPath $hostQtConfig)
}

$targetQtErrors = [System.Collections.Generic.List[string]]::new()
$targetQtConfig = $null
if (-not $QtRoot) {
    $targetQtErrors.Add('QT_ROOT is not set. Point it at the HarmonyOS target Qt prefix, not QT_HOST_PATH.')
} elseif (-not (Test-Path -LiteralPath $QtRoot -PathType Container)) {
    $targetQtErrors.Add("Qt target prefix does not exist: $QtRoot")
} else {
    $QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
    $targetQtConfig = Join-Path $QtRoot 'lib\cmake\Qt6\Qt6Config.cmake'
    $targetQtTargets = Join-Path $QtRoot 'lib\cmake\Qt6\Qt6Targets.cmake'
    $targetQtToolchain = Join-Path $QtRoot 'lib\cmake\Qt6\qt.toolchain.cmake'
    $targetQtCore = Join-Path $QtRoot 'lib\libQt6Core.so'

    foreach ($requiredFile in @($targetQtConfig, $targetQtTargets, $targetQtToolchain, $targetQtCore)) {
        if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
            $targetQtErrors.Add("HarmonyOS Qt file not found: $requiredFile")
        }
    }

    if (Test-Path -LiteralPath $targetQtConfig -PathType Leaf) {
        $qtConfigText = Get-Content -LiteralPath $targetQtConfig -Raw
        $versionMatch = [regex]::Match(
            $qtConfigText,
            'QT_VERSION_MAJOR\s+(\d+).*?QT_VERSION_MINOR\s+(\d+).*?QT_VERSION_PATCH\s+(\d+)',
            [Text.RegularExpressions.RegexOptions]::Singleline
        )
        if (-not $versionMatch.Success) {
            $targetQtErrors.Add('Could not read the target Qt version from Qt6Config.cmake.')
        } else {
            $targetQtVersion = '{0}.{1}.{2}' -f $versionMatch.Groups[1].Value, $versionMatch.Groups[2].Value, $versionMatch.Groups[3].Value
            if ($versionMatch.Groups[1].Value -ne '6' -or $versionMatch.Groups[2].Value -ne '12') {
                $targetQtErrors.Add("Target Qt 6.12.x is required; found $targetQtVersion.")
            }
        }
    }

    if ((Test-Path -LiteralPath $targetQtTargets -PathType Leaf) -and
        -not ((Get-Content -LiteralPath $targetQtTargets -Raw) -match 'OHOS_PLATFORM=OHOS')) {
        $targetQtErrors.Add('Qt target metadata does not identify a HarmonyOS/OHOS build.')
    }

    if ((Test-Path -LiteralPath $targetQtToolchain -PathType Leaf) -and
        -not ((Get-Content -LiteralPath $targetQtToolchain -Raw) -match 'set\(OHOS_ARCH\s+"?arm64-v8a"?')) {
        $targetQtErrors.Add('Qt target toolchain is not configured for arm64-v8a.')
    }
}

if ($targetQtErrors.Count -eq 0) {
    Write-Check 'Qt for HarmonyOS' "$targetQtVersion / arm64-v8a ($QtRoot)" $true
} else {
    Write-Check 'Qt for HarmonyOS' 'target kit validation failed' $false
    if ($RequireQt) {
        foreach ($targetQtError in $targetQtErrors) {
            $failures.Add($targetQtError)
        }
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
