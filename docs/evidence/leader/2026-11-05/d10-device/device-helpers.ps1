$ErrorActionPreference='Stop'
$h='C:/Users/pxt/Tools/Huawei/26.0.0.851/command-line-tools/sdk/default/openharmony/toolchains/hdc.exe'
$ev=Join-Path $PSScriptRoot '../docs/evidence/leader/2026-11-05/d10-device'
New-Item -ItemType Directory -Path $ev -Force | Out-Null
function Tap([int]$X,[int]$Y) { & $h shell "uitest uiInput click $X $Y" | Out-Null }
function Key([string]$Keys) { & $h shell "uitest uiInput keyEvent $Keys" | Out-Null }
function Shot([string]$Name) {
 Start-Sleep -Milliseconds 400
 & $h shell "snapshot_display -f /data/local/tmp/leocad-d10-$Name.jpeg" | Out-Null
 & $h file recv "/data/local/tmp/leocad-d10-$Name.jpeg" (Join-Path $ev "$Name.jpeg") | Out-Null
 Add-Type -AssemblyName System.Drawing; $im=[Drawing.Image]::FromFile((Join-Path $ev "$Name.jpeg")); $thumb=[Drawing.Bitmap]::new($im,1560,1040); $thumb.Save((Join-Path $PSScriptRoot "d10-view.jpeg"),[Drawing.Imaging.ImageFormat]::Jpeg); $thumb.Dispose(); $im.Dispose()
 Write-Output (Join-Path $PSScriptRoot 'd10-view.jpeg')
}
function SaveLog([string]$Name,[int]$AppPid) {
 $lines=& $h shell "hilog -x -P $AppPid"
 [IO.File]::WriteAllLines((Join-Path $ev "$Name.hilog.txt"),[string[]]$lines,[Text.UTF8Encoding]::new($false))
 $lines | Select-String 'GL_RENDERER|programs|returned 0|not alive|picker|Picker|DocumentView' | Select-Object -Last 14
}
function Mark([string]$Text) { ('{0} {1}' -f (Get-Date -Format o),$Text) | Add-Content -Encoding utf8 (Join-Path $ev 'steps.txt') }
function Mouse([int]$X,[int]$Y) { & $h shell "uinput -M -m $X $Y -c 0" | Out-Null }
function Drag([int]$X,[int]$Y,[int]$ToX,[int]$ToY,[int]$Button=0) { & $h shell "uinput -M -m $X $Y -d $Button -m $ToX $ToY 700 -u $Button" | Out-Null }

