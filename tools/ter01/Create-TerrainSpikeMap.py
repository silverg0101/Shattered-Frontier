"""TER-01 spike: create /Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_Voxel (refuses to overwrite).

One ASFTerrainSpike at the origin (96 m x 96 m of 0.5 m voxels in 16 m chunks plus a 1 m heightfield strip at x -48..0 m),
a PlayerStart on the surface, sun, sky, atmosphere and fog. The project default ASFGameMode admits /Game/Frontier/Maps/Dev/.
Run headless (no editor holding the map):
    UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=tools/ter01/Create-TerrainSpikeMap.py -unattended
"""
import math
import unreal as u

MAP = '/Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_Voxel'


def base_h(x, y):  # metres; mirrors ASFTerrainSpike::BaseHeightM
    mound = 7.0 * math.exp(-((x - 30) ** 2 + (y - 60) ** 2) / 150.0)
    return 3.0 * math.sin(x / 23.0) * math.cos(y / 31.0) + 1.5 * math.sin((x + 2 * y) / 11.0) + 0.04 * (x - 48) + mound


if u.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError('TerrainSpike map exists; refusing overwrite.')
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
if not levels.new_level(MAP):
    raise RuntimeError('Could not create ' + MAP)
spike = actors.spawn_actor_from_class(u.SFTerrainSpike, u.Vector(0, 0, 0))
spike.set_actor_label('TerrainSpike')
actors.spawn_actor_from_class(u.PlayerStart, u.Vector(2400, 1200, base_h(24, 12) * 100 + 150), u.Rotator(yaw=90))
sun = actors.spawn_actor_from_class(u.DirectionalLight, u.Vector(0, 0, 3000), u.Rotator(pitch=-35, yaw=-50))
sun.light_component.set_atmosphere_sun_light(True)
actors.spawn_actor_from_class(u.SkyLight, u.Vector(0, 0, 2000)).light_component.set_editor_property('real_time_capture', True)
actors.spawn_actor_from_class(u.SkyAtmosphere, u.Vector(0, 0, 0))
actors.spawn_actor_from_class(u.ExponentialHeightFog, u.Vector(0, 0, 0))
if not levels.save_current_level():
    raise RuntimeError('Could not save ' + MAP)
u.log('TERRAIN_SPIKE_MAP_CREATED ' + MAP)
