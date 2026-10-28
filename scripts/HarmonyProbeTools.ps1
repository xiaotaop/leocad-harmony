# Shared host-side helpers. Loading this file does not contact a device or service.
function Invoke-WithHarmonyProbeToolchain {
    param(
        [Parameter(Mandatory)][string]$SdkRoot,
        [Parameter(Mandatory)][scriptblock]$Action
    )

    $commandLineToolsRoot = Split-Path $SdkRoot -Parent
    $selectedEnvironment = [ordered]@{
        DEVECO_SDK_HOME = $SdkRoot
        DEVECO_CLI_CLT_PATH = $commandLineToolsRoot
        # DevEco CLI gives the Studio override higher priority than the CLT override.
        DEVECO_CLI_STUDIO_PATH = $null
        OHOS_SDK_ROOT = Join-Path $SdkRoot 'default\openharmony'
        QT_HARMONYOS_HVIGOR = Join-Path $commandLineToolsRoot 'bin\hvigorw.bat'
    }
    $previousEnvironment = @{}
    foreach ($name in $selectedEnvironment.Keys) {
        $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    }
    try {
        foreach ($name in $selectedEnvironment.Keys) {
            [Environment]::SetEnvironmentVariable($name, $selectedEnvironment[$name], 'Process')
        }
        & $Action
    }
    finally {
        foreach ($name in $selectedEnvironment.Keys) {
            [Environment]::SetEnvironmentVariable($name, $previousEnvironment[$name], 'Process')
        }
    }
}

function Wait-HarmonyGLProbeResult {
    param(
        [string[]]$BeforeLog = @(),
        [Parameter(Mandatory)][scriptblock]$ReadReportLog,
        [Parameter(Mandatory)][scriptblock]$ReadProcessLog,
        [ValidateRange(1, 300)][int]$TimeoutSeconds = 30,
        [ValidateRange(10, 5000)][int]$PollIntervalMilliseconds = 250
    )

    $elapsed = [Diagnostics.Stopwatch]::StartNew()
    while ($elapsed.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        $freshLines = @(& $ReadReportLog | Where-Object {
            $_ -match 'LEOCAD_GL_PROBE \{' -and $_ -notin $BeforeLog
        })
        if ($freshLines.Count -gt 1) {
            throw "Expected one fresh probe report, found $($freshLines.Count)."
        }
        if ($freshLines.Count -eq 1) {
            $freshLine = [string]$freshLines[0]
            if ($freshLine -notmatch '^\S+\s+\S+\s+(\d+)\s+\d+\s+.*LEOCAD_GL_PROBE (\{.*\})$') {
                throw 'Cannot parse fresh probe report process ID.'
            }
            $probeProcessId = [int]$Matches[1]
            $freshReport = $Matches[2] | ConvertFrom-Json -ErrorAction Stop
            $exitLines = @(& $ReadProcessLog $probeProcessId | Where-Object {
                $_ -match "\s$probeProcessId\s+\d+\s+.*'main'.*libleocad-render-probe.so.*returned -?[0-9]+"
            })
            if ($exitLines.Count -gt 0) {
                foreach ($line in $exitLines) {
                    if ($line -match 'returned (-?\d+)' -and [int]$Matches[1] -ne 0) {
                        throw "Probe PID $probeProcessId returned $($Matches[1])."
                    }
                }
                return [pscustomobject]@{
                    ReportLine = $freshLine
                    Report = $freshReport
                    ProcessId = $probeProcessId
                    ExitLines = $exitLines
                }
            }
        }
        Start-Sleep -Milliseconds $PollIntervalMilliseconds
    }
    throw "Timed out after $TimeoutSeconds seconds waiting for a fresh probe report and its native main exit."
}
