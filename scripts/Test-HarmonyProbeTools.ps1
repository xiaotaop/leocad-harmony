# Offline host tests: callbacks below never invoke HDC, signing, or a real device.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'HarmonyProbeTools.ps1')
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$fixturePath = Join-Path $repositoryRoot 'docs\evidence\leader\2026-10-27\d2-device-pass\run-1.hilog.txt'
$fixture = @(Get-Content -LiteralPath $fixturePath)
$reportLine = $fixture[0]
$exitLine = $fixture[1]
$passed = 0

function Assert-True([bool]$Value, [string]$Message) {
    if (-not $Value) { throw $Message }
}

function Assert-Throws([scriptblock]$Action, [string]$Expected) {
    $message = $null
    try { & $Action | Out-Null }
    catch { $message = $_.Exception.Message }
    if (-not $message -or $message -notlike $Expected) {
        throw "Expected error '$Expected', got '$message'."
    }
}

$coldStart = [Diagnostics.Stopwatch]::StartNew()
$completed = Wait-HarmonyGLProbeResult -TimeoutSeconds 10 -PollIntervalMilliseconds 50 -ReadReportLog {
    if ($coldStart.Elapsed.TotalSeconds -ge 5.2) { $reportLine }
} -ReadProcessLog { param($probeId) $exitLine }
Assert-True ($completed.Report.passed -and $coldStart.Elapsed.TotalSeconds -ge 5.2) 'Cold startup was rejected before the report arrived.'
Write-Host 'PASS: successful report arriving after five seconds'
$passed++

$delayedExit = @{ reads = 0 }
$completed = Wait-HarmonyGLProbeResult -TimeoutSeconds 2 -PollIntervalMilliseconds 10 -ReadReportLog {
    $reportLine
} -ReadProcessLog {
    param($probeId)
    $delayedExit.reads++
    if ($delayedExit.reads -ge 3) { $exitLine }
}
Assert-True ($delayedExit.reads -eq 3 -and $completed.ExitLines.Count -eq 1) 'Did not wait for the matching exit.'
Write-Host 'PASS: report precedes native main exit'
$passed++

Assert-Throws {
    Wait-HarmonyGLProbeResult -BeforeLog @($reportLine) -TimeoutSeconds 1 -PollIntervalMilliseconds 25 -ReadReportLog {
        $reportLine
    } -ReadProcessLog { param($probeId) $exitLine }
} '*Timed out*'
Write-Host 'PASS: an old report does not satisfy the deadline'
$passed++

Assert-Throws {
    Wait-HarmonyGLProbeResult -TimeoutSeconds 1 -ReadReportLog { $reportLine } -ReadProcessLog {
        param($probeId)
        $exitLine -replace 'returned 0', 'returned 1'
    }
} '*returned 1*'
Write-Host 'PASS: nonzero native main exit is rejected'
$passed++

Assert-Throws {
    Wait-HarmonyGLProbeResult -TimeoutSeconds 1 -ReadReportLog { $reportLine } -ReadProcessLog {
        param($probeId)
        $exitLine -replace '\s20760\s', ' 99999 '
    }
} '*Timed out*'
Write-Host 'PASS: an unrelated process exit is rejected'
$passed++

Assert-Throws {
    Wait-HarmonyGLProbeResult -TimeoutSeconds 1 -ReadReportLog { $reportLine; $reportLine } -ReadProcessLog {
        param($probeId) $exitLine
    }
} '*found 2*'
Write-Host 'PASS: concurrent reports are rejected'
$passed++

Assert-Throws {
    Wait-HarmonyGLProbeResult -TimeoutSeconds 1 -ReadReportLog { 'LEOCAD_GL_PROBE {}' } -ReadProcessLog {
        param($probeId) $exitLine
    }
} '*Cannot parse*'
Write-Host 'PASS: malformed report header is rejected'
$passed++

$environmentNames = @('DEVECO_SDK_HOME', 'DEVECO_CLI_CLT_PATH', 'DEVECO_CLI_STUDIO_PATH', 'OHOS_SDK_ROOT', 'QT_HARMONYOS_HVIGOR')
$initialEnvironment = @{}
foreach ($name in $environmentNames) {
    $initialEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
try {
    foreach ($name in $environmentNames) { [Environment]::SetEnvironmentVariable($name, 'old-sentinel', 'Process') }
    $selectedSdk = Join-Path $repositoryRoot 'build\offline-toolchain\command-line-tools\sdk'
    Invoke-WithHarmonyProbeToolchain -SdkRoot $selectedSdk -Action {
        Assert-True ($env:DEVECO_SDK_HOME -eq $selectedSdk) 'SDK override not propagated.'
        Assert-True ($env:DEVECO_CLI_CLT_PATH -eq (Split-Path $selectedSdk -Parent)) 'CLI tools override not propagated.'
        Assert-True ([string]::IsNullOrEmpty($env:DEVECO_CLI_STUDIO_PATH)) 'Higher-priority Studio override was not cleared.'
        Assert-True ($env:OHOS_SDK_ROOT -eq (Join-Path $selectedSdk 'default\openharmony')) 'Qt SDK override not propagated.'
        Assert-True ($env:QT_HARMONYOS_HVIGOR -eq (Join-Path (Split-Path $selectedSdk -Parent) 'bin\hvigorw.bat')) 'Hvigor override not propagated.'
        # Verify a real child process inherits the selected toolchain without running CLI.
        $inheritedSdk = & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -Command '$env:DEVECO_SDK_HOME'
        Assert-True ($LASTEXITCODE -eq 0 -and $inheritedSdk -eq $selectedSdk) 'Child process did not inherit the SDK override.'
    }
    foreach ($name in $environmentNames) {
        Assert-True ([Environment]::GetEnvironmentVariable($name, 'Process') -eq 'old-sentinel') "Did not restore $name after success."
    }
    Write-Host 'PASS: one toolchain propagated to child processes; old environment restored'
    $passed++

    Assert-Throws { Invoke-WithHarmonyProbeToolchain -SdkRoot $selectedSdk -Action { throw 'simulated-build-failure' } } '*simulated-build-failure*'
    foreach ($name in $environmentNames) {
        Assert-True ([Environment]::GetEnvironmentVariable($name, 'Process') -eq 'old-sentinel') "Did not restore $name after failure."
    }
    Write-Host 'PASS: old environment restored after an exception'
    $passed++

    foreach ($name in $environmentNames) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
    Invoke-WithHarmonyProbeToolchain -SdkRoot $selectedSdk -Action { }
    foreach ($name in $environmentNames) {
        Assert-True ([string]::IsNullOrEmpty([Environment]::GetEnvironmentVariable($name, 'Process'))) "Did not restore unset $name."
    }
    Write-Host 'PASS: initially unset environment restored'
    $passed++
}
finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $initialEnvironment[$name], 'Process')
    }
}
Write-Host "Offline host tests: $passed passed. No device or signing service was contacted."
