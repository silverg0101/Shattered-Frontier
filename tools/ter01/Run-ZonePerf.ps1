<#
TER-01b: the 2.5 km Silverbrook zone on the voxel ground in one -game client on the RTX 3060 (-graphicsadapter=1), 1920x1080.
Default: tools/ter01/zone_perf.py (GPU per view, ProfileGPU dumps, seams). -Soak N: N random edits within 300 m with the tear
check after each (-SFTerrainSpikeScenario -SFTerrainSpikeZoneSoak=N). Hard timeout; stops on SPIKE_FELL_THROUGH; kills only its PID.
  pwsh tools/ter01/Run-ZonePerf.ps1 -Out docs/evidence/terrain/2026-10-22-ter01b/zone [-K 1] [-Soak 500] [-Seconds 900] [-Extra '...']
#>
param([string]$Out = 'docs/evidence/terrain/2026-10-22-ter01b/zone', [double]$K = 1, [int]$Soak = 0, [int]$Seconds = 900, [string]$Extra = '', [int]$Adapter = 1)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$exe = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$proj = "$root\GameClient\ShatteredFrontier.uproject"
$map = '/Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_Voxel'
$hf = "$root\GameClient\Saved\TerrainSpike\silverbrook-hf.bin"
if (-not (Test-Path $hf)) { py -3.12 "$root\tools\ter01\Export-ZoneHeightfield.py" $hf | Out-Host }
New-Item -ItemType Directory -Force "$root\$Out" | Out-Null
$log = "$root\$Out\client.log"
$al = @("`"$proj`"", $map, '-game', '-windowed', '-ResX=1920', '-ResY=1080', "-graphicsadapter=$Adapter", '-nosound', '-unattended', '-SFTerrainSpikeCompareMesh=/Game/Frontier/Environment/Greenlands/Terrain/SM_Silverbrook_Terrain.SM_Silverbrook_Terrain',
    "-SFTerrainSpikeZone=`"$hf`"", "-SFTerrainSpikeLodK=$K", "-SFTerrainSpikeTimeout=$Seconds", "-abslog=`"$log`"")
if ($Soak -gt 0) { $al += @('-SFTerrainSpikeScenario', "-SFTerrainSpikeZoneSoak=$Soak") } else { $al += @('-SFTerrainSpikeNoTearCheck', "`"-ExecCmds=py $root\tools\ter01\zone_perf.py $root\$Out`"") }
if ($Extra) { $al += $Extra.Split(' ') }
$p = Start-Process $exe -ArgumentList $al -PassThru
$deadline = (Get-Date).AddSeconds($Seconds + 60)
$done = if ($Soak -gt 0) { 'TERRAIN_SPIKE_ZONE_SOAK_DONE' } else { 'TER01B_ZONE_DONE' }
while ((Get-Date) -lt $deadline -and -not $p.HasExited) {
    Start-Sleep 5
    if (Test-Path $log) {
        if (Select-String -Path $log -Pattern 'SPIKE_FELL_THROUGH' -Quiet) { break }
        if (Select-String -Path $log -Pattern $done -Quiet) { Start-Sleep 10; break }
    }
}
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
Select-String -Path $log -Pattern 'TERRAIN_SPIKE_(READY|STATS|ZONE_SOAK_DONE|TIMEOUT)|SPIKE_FELL_THROUGH|SPIKE_SEAMS|TER01B_ZONE_DONE|SPIKE_TEARS [1-9]' | ForEach-Object { $l = $_.Line; if ($l.Length -gt 600) { $l.Substring(0, 600) } else { $l } }
