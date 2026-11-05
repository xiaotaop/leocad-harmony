param([int]$AppPid)
. $PSScriptRoot/d10-device-helpers.ps1
$start=[DateTimeOffset]::Now
$start.ToString('o') | Set-Content (Join-Path $ev 'stability-start.txt')
for ($minute=0;$minute -le 30;$minute++) {
    $target=$start.AddMinutes($minute)
    while ([DateTimeOffset]::Now -lt $target) { Start-Sleep -Seconds 1 }
    $actual=(& $h shell 'pidof io.github.xiaotaop.leocad').Trim()
    if ($actual -ne [string]$AppPid) { Mark "FAIL process changed at minute $minute, expected $AppPid got $actual"; throw 'Process changed; stop test' }
    $mem=& $h shell "hidumper --mem $AppPid"
    [IO.File]::WriteAllLines((Join-Path $ev ('memory-{0:00}.txt' -f $minute)),[string[]]$mem,[Text.UTF8Encoding]::new($false))
    if ($minute % 5 -eq 0) {
        Mark "Stability minute $minute same PID $AppPid; injected navigation/edit exercise start"
        Drag 1300 950 1550 1070 1
        & $h shell 'uinput -K -d 2047' | Out-Null
        try { Drag 1400 950 1460 1000 1 } finally { & $h shell 'uinput -K -u 2047' | Out-Null }
        & $h shell 'uinput -M -m 1400 950 -s -1' | Out-Null
        & $h shell 'uinput -M -m 1400 950 -s 1' | Out-Null
        Key '2015'
        Key '2072 2042'
        Key '2072 2041'
        Key '2072 2042'
        Shot ('stability-{0:00}' -f $minute)
        Mark "Stability minute $minute exercise completed; commands delivered, visual result requires inspection"
        Write-Output "Checkpoint minute $minute, same PID $AppPid"
    }
}
Mark '30 minutes elapsed after medium model loaded; stability harness finished'
