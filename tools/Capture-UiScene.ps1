# One UI evidence scene: a hidden -server (NullRHI, scenario flags) plus a -game client at 1920x1080 that screenshots itself with
# tools/Capture-TimedShots.py. Only the PIDs it starts are stopped. Output: Artifacts/ui-v5-final/<name>/frames/shot-NNN.png
# Examples (flags as the gathering/combat scenarios use them):
#   .\tools\Capture-UiScene.ps1 -Name gathering -Map Greenlands/Greenlands_Silverbrook -ServerFlags '-GatheringPilot','-GatheringPilotScenario','-QuestPilot' -Seconds 40
#   .\tools\Capture-UiScene.ps1 -Name defeated  -Map Greenlands/Greenlands_Silverbrook -ServerFlags '-CombatPilot','-CombatPilotScenario','-QuestPilot' -Seconds 45
#   .\tools\Capture-UiScene.ps1 -Name station   -Map Greenlands_ConstructionSandbox -ServerFlags '-ConstructionNetworkSandbox','-GatheringPilot','-GatheringRefiningScenario' -ClientFlags '-GatheringPilot','-GatheringRefiningScenario' -Seconds 35
param([Parameter(Mandatory)][string]$Name, [string]$Map = 'Greenlands/Greenlands_Silverbrook', [string[]]$ServerFlags = @(), [string[]]$ClientFlags = @(), [int]$Seconds = 40, [double]$Interval = 0.5, [int]$Port = 17931)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$ed = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$up = Join-Path $root 'GameClient\ShatteredFrontier.uproject'
$out = Join-Path $root "Artifacts\ui-v5-final\$Name"; $null = New-Item -ItemType Directory -Force $out
$server = Start-Process $ed -ArgumentList (@("`"$up`"", "/Game/Frontier/Maps/$Map`?game=/Script/ShatteredFrontier.SFGameMode", '-server', "-port=$Port", '-multihome=127.0.0.1', '-unattended', '-NullRHI', '-nosound', '-nop4', "-UserDir=`"$out\server-user`"", "-abslog=`"$out\server.log`"") + $ServerFlags) -PassThru -WindowStyle Hidden
try {
    Start-Sleep 60
    $exec = "-ExecCmds=`"py $root/tools/Capture-TimedShots.py $out/frames $Seconds $Interval`""
    $client = Start-Process $ed -ArgumentList (@("`"$up`"", "127.0.0.1:$Port", '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-nosound', '-nop4', "-UserDir=`"$out\client-user`"", "-abslog=`"$out\client.log`"", $exec) + $ClientFlags) -PassThru
    try { $client.WaitForExit(($Seconds + 150) * 1000) | Out-Null } finally { if (-not $client.HasExited) { Stop-Process -Id $client.Id -Force } }
} finally { Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue }
(Get-ChildItem "$out\frames" -Filter *.png -ErrorAction SilentlyContinue | Measure-Object).Count.ToString() + " frames in $out\frames"
