<#
TER-01 spike, Mesh Terrain path. Requires the dev-only plugin GameClient/Plugins/TerrainSpikeMeshTerrain enabled in the
uproject and built (/build-editor). Steps: create the map (unless it exists), build compiled sections (WP builder
commandlet, timed), cook + stage Win64, run the C++ SFTerrainSpike.Probe in the staged game on both spike maps (it quits
itself; this script also kills its own PID after -Seconds). Stops only the PIDs it started.
  pwsh tools/ter01/Run-MeshTerrainSpike.ps1 -Out docs/evidence/terrain/2026-10-22-ter01
#>
param([string]$Out = 'docs/evidence/terrain/2026-10-22-ter01', [int]$Seconds = 200)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$bin = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64'
$proj = "$root\GameClient\ShatteredFrontier.uproject"
$map = '/Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_MeshTerrain'
New-Item -ItemType Directory -Force "$root\$Out" | Out-Null
if (-not (Test-Path "$root\GameClient\Content\Frontier\Maps\Dev\TerrainSpike\TerrainSpike_MeshTerrain.umap")) {
    Start-Process "$bin\UnrealEditor-Cmd.exe" -ArgumentList "`"$proj`"", '-run=pythonscript', "-script=`"$root\tools\ter01\Create-MeshTerrainMap.py`"", '-unattended', '-nosplash', "-abslog=`"$root\$Out\meshterrain-create.log`"" -Wait -NoNewWindow -RedirectStandardOutput "$env:TEMP\ter01-mt-create.out"
}
$t0 = Get-Date
Start-Process "$bin\UnrealEditor-Cmd.exe" -ArgumentList "`"$proj`"", $map, '-run=WorldPartitionBuilderCommandlet', '-Builder=WorldPartitionMeshPartitionBuilder', '-SCCProvider=None', '-unattended', '-nosplash', "-abslog=`"$root\$Out\meshterrain-build.log`"" -Wait -NoNewWindow -RedirectStandardOutput "$env:TEMP\ter01-mt-build.out"
"MESH_TERRAIN_BUILD_WALL_S $([math]::Round(((Get-Date) - $t0).TotalSeconds, 1))"
# Uncooked -game/-server crash on this map in 5.8.3 (WorldPartitionActorDesc.cpp:944 ComponentNativeClass: the Mesh Partition
# actor carries an editor-module component). Measure on a cooked, staged Win64 build instead (game target must be built).
Start-Process "$bin\UnrealEditor-Cmd.exe" -ArgumentList "`"$proj`"", '-run=cook', '-targetplatform=Windows', "-map=$map+/Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_Voxel", '-unattended', '-nosplash' -Wait -NoNewWindow -RedirectStandardOutput "$env:TEMP\ter01-cook.out"
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun "-project=$proj" -platform=Win64 -clientconfig=Development -skipbuild -skipcook -stage -pak "-map=$map+/Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_Voxel" -unattended *> "$env:TEMP\ter01-stage.log"
$staged = "$root\GameClient\Saved\StagedBuilds\Windows"
foreach ($m in @($map, '/Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_Voxel')) {
    $g = Start-Process "$staged\ShatteredFrontier.exe" -ArgumentList $m, '-windowed', '-ResX=1920', '-ResY=1080', '-nosound', '-unattended', '-ExecCmds="SFTerrainSpike.Probe"' -PassThru
    if (-not $g.WaitForExit($Seconds * 1000)) { Stop-Process -Id $g.Id -Force; 'TER01 hard timeout: killed own PID ' + $g.Id }
}
Copy-Item "$staged\ShatteredFrontier\Saved\TerrainSpike\*" "$root\$Out\" -Force
Select-String -Path "$root\$Out\meshterrain-build.log" -Pattern 'Finished Build for World|Execution took|QEM simplified' | ForEach-Object { $_.Line }
Get-Content "$root\$Out\probe-TerrainSpike_MeshTerrain.json"