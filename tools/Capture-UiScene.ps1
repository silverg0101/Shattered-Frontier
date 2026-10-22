# One UI evidence scene: a hidden -server plus a 1920x1080 -game client. -Final3 drives a named state into
# docs/evidence/ui-v5/final3/<name>-live.png; without it, Capture-TimedShots.py writes artifact frames.
# Only the PIDs this script starts are stopped.
# Examples (flags as the gathering/combat scenarios use them):
#   .\tools\Capture-UiScene.ps1 -Name gathering -Map Greenlands/Greenlands_Silverbrook -ServerFlags '-GatheringPilot','-GatheringPilotScenario','-QuestPilot' -Seconds 40
#   .\tools\Capture-UiScene.ps1 -Name defeated  -Map Greenlands/Greenlands_Silverbrook -ServerFlags '-CombatPilot','-CombatPilotScenario','-QuestPilot' -Seconds 45
#   .\tools\Capture-UiScene.ps1 -Name station   -Map Greenlands_ConstructionSandbox -ServerFlags '-ConstructionNetworkSandbox','-GatheringPilot','-GatheringRefiningScenario' -ClientFlags '-GatheringPilot','-GatheringRefiningScenario' -Seconds 35
param([Parameter(Mandatory)][string]$Name, [string]$Map = 'Greenlands/Greenlands_Silverbrook', [string[]]$ServerFlags = @(), [string[]]$ClientFlags = @(), [int]$Seconds = 40, [double]$Interval = 0.5, [int]$Port = 17931, [switch]$Final3)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$ed = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$up = Join-Path $root 'GameClient\ShatteredFrontier.uproject'
$out = Join-Path $root "Artifacts\ui-v5-final\$Name"; $null = New-Item -ItemType Directory -Force $out
if ($Final3) {
    $evidence = Join-Path $root 'docs\evidence\ui-v5\final3'
    $null = New-Item -ItemType Directory -Force $evidence
    $shot = Join-Path $evidence "$Name-live.png"
    if (Test-Path -LiteralPath $shot) { Remove-Item -LiteralPath $shot }
    if ($Name -eq '9-loading-screen') {
        $client = Start-Process $ed -ArgumentList @("`"$up`"", '/Engine/Maps/Entry', '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-nosound', '-nop4', "-UserDir=`"$out\client-user`"", "-abslog=`"$out\client.log`"", "-LoadingShot=`"$shot`"", "-ExecCmds=`"open /Game/Frontier/Maps/$Map`"") -PassThru
        try {
            $until = (Get-Date).AddSeconds(150)
            while (-not ((Test-Path -LiteralPath $shot) -and (Select-String -LiteralPath "$out\client.log" -Pattern 'UI_LOADING_SHOT_SAVED' -Quiet -ErrorAction SilentlyContinue)) -and -not $client.HasExited -and (Get-Date) -lt $until) { Start-Sleep -Seconds 2 }
        } finally { if (-not $client.HasExited) { Stop-Process -Id $client.Id -Force } }
        if (-not ((Test-Path -LiteralPath $shot) -and (Select-String -LiteralPath "$out\client.log" -Pattern 'UI_LOADING_SHOT_SAVED' -Quiet))) { throw 'Loading widget screenshot was not saved.' }
        Write-Output "Loading screenshot saved: $shot"
        return
    }
    switch ($Name) {
        'gameplay-hud' { $ServerFlags = @('-CombatPilot','-CombatPilotScenario','-GatheringPilot','-QuestPilot','-UiEvidenceHud'); $ClientFlags = @('-CombatPilot','-QuestPilot') }
        'gathering' { $ServerFlags = @('-GatheringPilot','-GatheringPilotScenario','-QuestPilot'); $ClientFlags = @('-GatheringPilot','-GatheringPilotScenario') }
        'crafting-station' { $ServerFlags = @('-GatheringPilot','-QuestPilot','-UiEvidenceStation'); $ClientFlags = @('-GatheringPilot') }
        'defeated' { $ServerFlags = @('-CombatPilot','-CombatPilotScenario','-QuestPilot','-UiEvidenceDefeat'); $ClientFlags = @('-CombatPilot') }
        default { throw "Unsupported final3 state: $Name" }
    }
}
$server = Start-Process $ed -ArgumentList (@("`"$up`"", "/Game/Frontier/Maps/$Map`?game=/Script/ShatteredFrontier.SFGameMode", '-server', "-port=$Port", '-multihome=127.0.0.1', '-SFDevUnlimitedStock', '-unattended', '-NullRHI', '-nosound', '-nop4', "-UserDir=`"$out\server-user`"", "-abslog=`"$out\server.log`"") + $ServerFlags) -PassThru -WindowStyle Hidden
try {
    Start-Sleep 45
    $exec = if ($Final3) { "-ExecCmds=`"py $root/tools/Capture-UiV5.py $Name $evidence`"" } else { "-ExecCmds=`"py $root/tools/Capture-TimedShots.py $out/frames $Seconds $Interval`"" }
    $client = Start-Process $ed -ArgumentList (@("`"$up`"", "127.0.0.1:$Port", '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-nosound', '-nop4', "-UserDir=`"$out\client-user`"", "-abslog=`"$out\client.log`"", $exec) + $ClientFlags) -PassThru
    try { $client.WaitForExit(($Seconds + 150) * 1000) | Out-Null } finally { if (-not $client.HasExited) { Stop-Process -Id $client.Id -Force } }
} finally { Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue }
if ($Final3) { if (-not (Test-Path -LiteralPath $shot)) { throw "Missing UI capture: $shot" }; Write-Output "Screenshot saved: $shot" }
else { (Get-ChildItem "$out\frames" -Filter *.png -ErrorAction SilentlyContinue | Measure-Object).Count.ToString() + " frames in $out\frames" }
