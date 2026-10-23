# Player MetaHuman frame-cost gate: a standalone -game run on Silverbrook driven by tools/measure_player_crowd.py.
# Prints the CROWD_SAMPLE lines and copies the phase screenshots to Artifacts\CrowdGate\<stamp>\. Needs a built player face
# (tools/Build-PlayerMetaHumans.py) and the editor target built. Kills only the process it started.
param([int]$TimeoutSec = 600, [int]$ResX = 1920, [int]$ResY = 1080)
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root ("Artifacts\CrowdGate\" + (Get-Date -Format 'yyyyMMdd-HHmmss')); New-Item -ItemType Directory -Force $out | Out-Null
$uproject = Join-Path $root 'GameClient\ShatteredFrontier.uproject'
$script = (Join-Path $PSScriptRoot 'measure_player_crowd.py') -replace '\\', '/'
$log = Join-Path $out 'game.log'
$shots = Join-Path $root 'GameClient\Saved\Screenshots\WindowsEditor'
$before = @(Get-ChildItem $shots -Filter *.png -ErrorAction SilentlyContinue | ForEach-Object FullName)
$p = Start-Process -PassThru 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList @(
    "`"$uproject`"", '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook', '-game', '-windowed', "-ResX=$ResX", "-ResY=$ResY", '-ForceRes',
    '-RenderOffscreen', '-unattended', '-nosound', '-nop4', "-abslog=`"$log`"", "-ExecCmds=`"py $script`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "TIMEOUT (killed own PID $($p.Id))" }
Get-ChildItem $shots -Filter *.png -ErrorAction SilentlyContinue | Where-Object { $before -notcontains $_.FullName } | Copy-Item -Destination $out
Select-String -Path $log -Pattern 'CROWD_SAMPLE|CROWD_TEST|PLAYER_METAHUMAN_MISSING' | ForEach-Object { $_.Line -replace '^.*?(CROWD_|PLAYER_)', '$1' }
Write-Host "Evidence: $out"
