# Exercise the actual collector entry with an offline HDC function substitute.
# The SDK stub is empty; no HDC executable, signing service or device is used.
$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$collectorPath = Join-Path $PSScriptRoot 'collect-harmony-gl-probe.ps1'
$fixtureDirectory = Join-Path $repositoryRoot 'docs\evidence\leader\2026-10-27\d2-device-pass'
$fixtureLines = @(Get-Content (Join-Path $fixtureDirectory 'run-1.hilog.txt'))
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ('leocad-collector-test-' + [guid]::NewGuid().ToString('N'))
$fakeSdk = Join-Path $temporaryRoot 'sdk'
$fakeHdc = Join-Path $fakeSdk 'default\openharmony\toolchains\hdc.exe'
$functionPath = 'Function:global:' + $fakeHdc
$previousExitCode = $global:LASTEXITCODE
$null = New-Item -ItemType Directory -Path (Split-Path $fakeHdc -Parent) -Force
$null = New-Item -ItemType File -Path $fakeHdc
$passed = 0

try {
    foreach ($mode in @('snapshot_fail_marker', 'snapshot_nonzero_exit', 'empty_snapshot', 'old_and_new_reports')) {
        $state = @{
            Mode = $mode
            ReportReads = 0
            Calls = [Collections.Generic.List[string]]::new()
        }
        $oldReport = $fixtureLines[0]
        $oldExit = $fixtureLines[1]
        $newReport = $oldReport -replace '\b20760\b', '30760'
        $newExit = $oldExit -replace '\b20760\b', '30760'
        $mockHdc = {
            $command = $args -join ' '
            $state.Calls.Add($command)
            $global:LASTEXITCODE = 0
            if ($command -match 'shell hilog -x -e LEOCAD_GL_PROBE$') {
                $state.ReportReads++
                if ($state.ReportReads -eq 1) {
                    switch ($state.Mode) {
                        'snapshot_fail_marker' { '[Fail] simulated initial hilog snapshot failure'; return }
                        'snapshot_nonzero_exit' { $global:LASTEXITCODE = 23; return }
                        'empty_snapshot' { return }
                        'old_and_new_reports' { $oldReport; return }
                    }
                }
                if ($state.Mode -eq 'old_and_new_reports') { $oldReport; $newReport }
                else { $oldReport }
                return
            }
            if ($command -match 'shell aa start ') { 'start ability successfully.'; return }
            if ($command -match 'shell hilog -x -P (\d+)$') {
                if ($Matches[1] -eq '30760') { $newExit } else { $oldExit }
                return
            }
            if ($command -match 'file recv -b ') {
                $sourceName = if ($command -match 'render-probe\.json') { 'run-1.json' } else { 'run-1.png' }
                Copy-Item -LiteralPath (Join-Path $fixtureDirectory $sourceName) -Destination $args[-1]
                'FileTransfer finish'
                return
            }
            if ($command -match 'shell hilog -x -e libleocad-render-probe.so$') { $oldExit; return }
            throw "Unexpected offline HDC request: $command"
        }.GetNewClosure()
        Set-Item -LiteralPath $functionPath -Value $mockHdc
        $outputDirectory = Join-Path $temporaryRoot $mode
        $failureMessage = $null
        try {
            & $collectorPath -SdkRoot $fakeSdk -Device 'OFFLINE-TEST-DEVICE' -OutputDirectory $outputDirectory -Rounds 1 | Out-Null
        }
        catch { $failureMessage = $_.Exception.Message }

        if ($mode -in @('snapshot_fail_marker', 'snapshot_nonzero_exit')) {
            if (-not $failureMessage -or $failureMessage -notlike '*initial probe log snapshot*') {
                throw "${mode}: expected initial-snapshot failure, got '$failureMessage'."
            }
            if ($state.Calls.Count -ne 1 -or $state.ReportReads -ne 1) {
                throw "${mode}: collector continued after an invalid baseline."
            }
            if (@(Get-ChildItem -LiteralPath $outputDirectory -File).Count -ne 0) {
                throw "${mode}: collector wrote evidence after a failed baseline."
            }
            Write-Host "PASS: $mode stops before launch, file transfer and evidence output"
        }
        else {
            if ($failureMessage) { throw "${mode}: unexpected failure: $failureMessage" }
            $receipt = Get-Content (Join-Path $outputDirectory 'collection.json') -Raw | ConvertFrom-Json
            $expectedProcessId = if ($mode -eq 'old_and_new_reports') { 30760 } else { 20760 }
            if (-not $receipt.passed -or $receipt.process_id -ne $expectedProcessId) {
                throw "${mode}: wrong process accepted."
            }
            if (@($state.Calls | Where-Object { $_ -match 'shell aa start ' }).Count -ne 1 -or
                @($state.Calls | Where-Object { $_ -match 'file recv -b ' }).Count -ne 2) {
                throw "${mode}: expected one launch and two evidence transfers."
            }
            foreach ($extension in @('json', 'png')) {
                $actualHash = (Get-FileHash (Join-Path $outputDirectory "run-1.$extension") -Algorithm SHA256).Hash.ToLowerInvariant()
                if ($actualHash -ne $receipt."${extension}_sha256") { throw "${mode}: receipt hash mismatch." }
            }
            Write-Host "PASS: $mode accepts the matching result via the actual collector entry"
        }
        $passed++
        Remove-Item -LiteralPath $functionPath
    }
}
finally {
    Remove-Item -LiteralPath $functionPath -ErrorAction SilentlyContinue
    $global:LASTEXITCODE = $previousExitCode
    # Only remove this test's uniquely named temporary directory after containment checks.
    $resolvedRoot = [IO.Path]::GetFullPath($temporaryRoot)
    $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ([IO.Path]::GetDirectoryName($resolvedRoot) -ne $resolvedTemp -or
        [IO.Path]::GetFileName($resolvedRoot) -notlike 'leocad-collector-test-*') {
        throw 'Refusing to remove a directory outside the collector test workspace.'
    }
    Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
}
Write-Host "Offline collector entry tests: $passed passed. No real HDC or device was used."
