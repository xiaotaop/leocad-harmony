[CmdletBinding()]
param(
    [string]$SdkRoot = $env:DEVECO_SDK_HOME,
    [Parameter(Mandatory)][string]$Device,
    [string]$OutputDirectory,
    [ValidateRange(1, 10)][int]$Rounds = 2
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $SdkRoot) { throw 'Pass -SdkRoot or set DEVECO_SDK_HOME.' }
$hdc = Join-Path $SdkRoot 'default\openharmony\toolchains\hdc.exe'
if (-not (Test-Path -LiteralPath $hdc -PathType Leaf)) { throw "HDC not found: $hdc" }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repositoryRoot 'build\harmony-render-probe\evidence' }
$null = New-Item -ItemType Directory -Force -Path $OutputDirectory
$bundle = 'io.github.xiaotaop.leocad.glprobe'
$results = @()

for ($round = 1; $round -le $Rounds; $round++) {
    $beforeLog = @(& $hdc -t $Device shell hilog -x -e LEOCAD_GL_PROBE 2>&1)
    $startedAt = [DateTimeOffset]::Now
    $launch = (& $hdc -t $Device shell aa start -a QAbility -b $bundle 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0 -or $launch -notmatch 'start ability successfully') {
        throw "Round ${round}: launch failed: $launch"
    }
    # The probe keeps the second rendered frame visible for three seconds.
    Start-Sleep -Seconds 5
    $afterLog = @(& $hdc -t $Device shell hilog -x -e LEOCAD_GL_PROBE 2>&1)
    $freshLines = @($afterLog | Where-Object { $_ -match 'LEOCAD_GL_PROBE \{' -and $_ -notin $beforeLog })
    if ($freshLines.Count -ne 1) {
        throw "Round ${round}: expected one fresh report, found $($freshLines.Count)."
    }
    $freshLine = [string]$freshLines[0]
    if ($freshLine -notmatch '^\S+\s+\S+\s+(\d+)\s+\d+\s+.*LEOCAD_GL_PROBE (\{.*\})$') {
        throw "Round ${round}: cannot parse report process ID."
    }
    $probeProcessId = $Matches[1]
    $freshReport = $Matches[2] | ConvertFrom-Json
    $processLog = @(& $hdc -t $Device shell hilog -x -P $probeProcessId 2>&1)
    $exitLines = @($processLog | Where-Object { $_ -match "'main'.*libleocad-render-probe.so.*returned 0" })
    if ($exitLines.Count -eq 0) { throw "Round ${round}: no successful native main exit for PID $probeProcessId." }
    $jsonPath = Join-Path $OutputDirectory "run-$round.json"
    $pngPath = Join-Path $OutputDirectory "run-$round.png"
    foreach ($file in @(@('render-probe.json', $jsonPath), @('render-probe.png', $pngPath))) {
        $transfer = (& $hdc -t $Device file recv -b $bundle "/data/storage/el2/base/files/$($file[0])" $file[1] 2>&1) -join "`n"
        if ($LASTEXITCODE -ne 0 -or $transfer -match '\[Fail\]' -or -not (Test-Path -LiteralPath $file[1])) {
            throw "Round ${round}: evidence transfer failed: $transfer"
        }
    }
    $report = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
    if (($report | ConvertTo-Json -Depth 20 -Compress) -cne ($freshReport | ConvertTo-Json -Depth 20 -Compress)) {
        throw "Round ${round}: sandbox report does not match the fresh process log."
    }
    if (-not $report.passed -or -not $report.screenshot_saved -or
        @($report.leocad_shaders).Count -ne 9 -or
        @($report.leocad_shaders | Where-Object { -not $_.linked }).Count -ne 0) {
        throw "Round ${round}: probe assertions failed; inspect $jsonPath"
    }
    $results += [ordered]@{
        round = $round
        started_at = $startedAt.ToString('o')
        retrieved_at = [DateTimeOffset]::Now.ToString('o')
        process_id = [int]$probeProcessId
        native_main_exit_code = 0
        passed = [bool]$report.passed
        json_sha256 = (Get-FileHash -LiteralPath $jsonPath -Algorithm SHA256).Hash.ToLowerInvariant()
        png_sha256 = (Get-FileHash -LiteralPath $pngPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    @($freshLine) + $exitLines | Set-Content -LiteralPath (Join-Path $OutputDirectory "run-$round.hilog.txt") -Encoding utf8
    Write-Host "Round ${round}: PASS ($($report.context.renderer), $($report.context.gl_version))"
}

$log = (& $hdc -t $Device shell hilog -x -e LEOCAD_GL_PROBE 2>&1) -join "`n"
$exitLog = (& $hdc -t $Device shell hilog -x -e libleocad-render-probe.so 2>&1) -join "`n"
$exitLines = ($exitLog -split "`n" | Where-Object { $_ -match "'main'.*returned [0-9]+" }) -join "`n"
Set-Content -LiteralPath (Join-Path $OutputDirectory 'probe.hilog.txt') -Value $log -Encoding utf8
Set-Content -LiteralPath (Join-Path $OutputDirectory 'exit.hilog.txt') -Value $exitLines -Encoding utf8
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'collection.json') -Encoding utf8
Write-Host "Evidence: $OutputDirectory"
