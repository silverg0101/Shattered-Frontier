<#
TER-01 spike: dedicated server + one -game client on the TerrainSpike_Voxel dev map.
The server runs the scripted dig/raise sequence (-SFTerrainSpikeScenario); the client applies the replicated op log and,
with -ClientPy, runs tools/ter01/client_checks.py (traces, crack count, captures). Stops only the PIDs it started.
  pwsh tools/ter01/Run-TerrainSpikeNet.ps1 -Out docs/evidence/terrain/2026-10-22-ter01 [-ClientPy] [-Port 17877]
#>
param([string]$Out = 'docs/evidence/terrain/2026-10-22-ter01', [switch]$ClientPy, [int]$Port = 17877, [int]$Seconds = 150, [switch]$LegacyWinding)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$exe = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$proj = "$root\GameClient\ShatteredFrontier.uproject"
$map = '/Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_Voxel'
New-Item -ItemType Directory -Force "$root\$Out" | Out-Null
$slog = "$root\$Out\server.log"; $clog = "$root\$Out\client.log"
$extra = if ($LegacyWinding) { @('-SFTerrainSpikeLegacyWinding') } else { @() }
$sargs = @("`"$proj`"", $map, '-server', "-port=$Port", '-SFTerrainSpikeScenario', "-SFTerrainSpikeTimeout=$Seconds", '-unattended', '-nosound', "-abslog=`"$slog`"") + $extra
$server = Start-Process $exe -ArgumentList $sargs -PassThru
Start-Sleep 20
$cargs = @("`"$proj`"", "127.0.0.1:$Port", '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-nosound', '-unattended', "-SFTerrainSpikeTimeout=$Seconds", "-abslog=`"$clog`"") + $extra
if ($ClientPy) { $cargs += "`"-ExecCmds=py $root\tools\ter01\client_checks.py $root\$Out`"" }
$client = Start-Process $exe -ArgumentList $cargs -PassThru
$deadline = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $deadline) {
    Start-Sleep 5
    $done = (Test-Path $slog) -and (Select-String -Path $slog -Pattern 'TERRAIN_SPIKE_SCENARIO_DONE' -Quiet)
    $cdone = -not $ClientPy -or ((Test-Path $clog) -and (Select-String -Path $clog -Pattern 'TER01_CLIENT_DONE' -Quiet))
    if ($done -and $cdone) { Start-Sleep 5; break }
    if ((Test-Path $clog) -and (Select-String -Path $slog, $clog -Pattern 'SPIKE_FELL_THROUGH' -Quiet)) { break }  # never leave a fallen client running
}
foreach ($p in @($client, $server)) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Select-String -Path $slog, $clog -Pattern 'TERRAIN_SPIKE_(READY|PAWN_SEEN|PAWN_SNAPPED|STATS|SCENARIO_DONE|TIMEOUT)|SPIKE_FELL_THROUGH|TER01_' | ForEach-Object { $_.Line }
