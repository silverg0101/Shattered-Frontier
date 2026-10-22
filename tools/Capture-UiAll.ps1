# Recaptures all nine v5 UI screens into docs/evidence/ui-v5/final3/<name>-live.png at 1920x1080 (-game), then prints the pixel-diff summary.
# Stops only the Unreal PIDs it starts. Needs a built editor target and no editor of this checkout open.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$ed = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$up = Join-Path $root 'GameClient\ShatteredFrontier.uproject'
$out = Join-Path $root 'docs\evidence\ui-v5\final3'; $null = New-Item -ItemType Directory -Force $out
# Standalone session: gameplay hud, build, inventory (cursor on bag cell 1), market, trade (my offer locked).
$ud = Join-Path $env:TEMP 'ui5-user'
$p = Start-Process $ed -ArgumentList @("`"$up`"", '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook', '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-nosound', '-nop4', '-ExchangeFake', "-UserDir=`"$ud`"", "-ExecCmds=`"py $root/tools/Capture-UiV5.py all $out`"") -PassThru
try { $p.WaitForExit(240000) | Out-Null } finally { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
# Scenario sessions (server + client): these four overwrite gameplay-hud with the targeted-boar state.
foreach ($name in 'gameplay-hud', 'gathering', 'crafting-station', 'defeated', '9-loading-screen') { & (Join-Path $PSScriptRoot 'Capture-UiScene.ps1') -Name $name -Final3 -Seconds 45 }
py -3 (Join-Path $PSScriptRoot 'ui_pixel_diff.py') --all
