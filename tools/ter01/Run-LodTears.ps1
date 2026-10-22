<#
TER-01b: the spike's 51-edit sequence in standalone -game with the tear check after every edit, once per leaf set:
uniform LOD 1, uniform LOD 2, and mixed LOD 0-2 (K 0.6, focus at the origin corner) so transition faces cross the edits.
Hard timeout per run; stops on SPIKE_FELL_THROUGH; kills only its PIDs.
  pwsh tools/ter01/Run-LodTears.ps1 -Out docs/evidence/terrain/2026-10-22-ter01b/lod-tears
#>
param([string]$Out = 'docs/evidence/terrain/2026-10-22-ter01b/lod-tears', [int]$Seconds = 150)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$exe = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$proj = "$root\GameClient\ShatteredFrontier.uproject"
New-Item -ItemType Directory -Force "$root\$Out" | Out-Null
$runs = [ordered]@{ 'lod1' = @('-SFTerrainSpikeMinLod=1'); 'lod2' = @('-SFTerrainSpikeMinLod=2'); 'mixed' = @('-SFTerrainSpikeLodK=0.6', '-SFTerrainSpikeFocus=0,0,0') }
foreach ($name in $runs.Keys) {
    $log = "$root\$Out\$name.log"
    $al = @("`"$proj`"", '/Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_Voxel', '-game', '-windowed', '-ResX=1280', '-ResY=720', '-nosound', '-unattended',
        '-SFTerrainSpikeScenario', "-SFTerrainSpikeTimeout=$Seconds", "-abslog=`"$log`"") + $runs[$name]
    $p = Start-Process $exe -ArgumentList $al -PassThru
    $deadline = (Get-Date).AddSeconds($Seconds + 30)
    while ((Get-Date) -lt $deadline -and -not $p.HasExited) {
        Start-Sleep 3
        if ((Test-Path $log) -and (Select-String -Path $log -Pattern 'SCENARIO_DONE|SPIKE_FELL_THROUGH' -Quiet)) { Start-Sleep 3; break }
    }
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
    $zero = (Select-String -Path $log -Pattern 'SPIKE_TEARS 0 ').Count; $bad = (Select-String -Path $log -Pattern 'SPIKE_TEARS [1-9]').Count
    $seams = (Select-String -Path $log -Pattern 'SPIKE_SEAMS').Line -replace '.*SPIKE_SEAMS netmode=\d ', ''
    "$name tear_checks_zero=$zero tear_checks_nonzero=$bad seams=$seams"
}
