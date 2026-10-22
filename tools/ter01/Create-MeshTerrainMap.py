"""TER-01 spike: /Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_MeshTerrain (World Partition, refuses to overwrite).

Imports the same base terrain as ASFTerrainSpike (96 m x 96 m, 0.5 m vertex spacing, 6 x 6 sections = 16 m) as a UE 5.8
Mesh Terrain (Mesh Partition) through the dev-only TerrainSpikeMeshTerrain plugin, sets MI_GN_Ground on its definition,
adds sun/sky/fog and a PlayerStart. Needs the plugin enabled in the uproject (spike only). Then build compiled sections:
    UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=tools/ter01/Create-MeshTerrainMap.py -unattended
    UnrealEditor-Cmd.exe <uproject> /Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_MeshTerrain -run=WorldPartitionBuilderCommandlet -Builder=WorldPartitionMeshPartitionBuilder -SCCProvider=None -unattended
"""
import math, struct, time, zlib
from pathlib import Path
import unreal as u

MAP = '/Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_MeshTerrain'
RES = 193  # 0.5 m spacing over 96 m
PNG = Path(u.Paths.project_saved_dir()).resolve() / 'TerrainSpike' / 'base_height.png'


def base_h(x, y):  # metres; mirrors ASFTerrainSpike::BaseHeightM
    mound = 7.0 * math.exp(-((x - 30) ** 2 + (y - 60) ** 2) / 150.0)
    return 3.0 * math.sin(x / 23.0) * math.cos(y / 31.0) + 1.5 * math.sin((x + 2 * y) / 11.0) + 0.04 * (x - 48) + mound


def write_png16(path, w, h, rows):
    raw = b''.join(b'\x00' + struct.pack('>%dH' % w, *r) for r in rows)
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 16, 0, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


if u.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError('MeshTerrain spike map exists; refusing overwrite.')
hs = [[base_h(i * 0.5, j * 0.5) for i in range(RES)] for j in range(RES)]  # row j = +Y
lo = min(min(r) for r in hs); hi = max(max(r) for r in hs)
write_png16(PNG, RES, RES, [[round((v - lo) / (hi - lo) * 65535) for v in r] for r in hs])
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
if not levels.new_level(MAP, True):
    raise RuntimeError('Could not create ' + MAP)
t0 = time.perf_counter()
mp, err = u.TerrainSpikeMeshTerrainLibrary.import_heightmap(str(PNG), RES, RES, u.Vector(9600, 9600, (hi - lo) * 100), 6, 6)
if not mp:
    raise RuntimeError('Mesh Terrain import failed: ' + err)
import_s = time.perf_counter() - t0
mp.set_actor_location(u.Vector(4800, 4800, lo * 100), False, False)
mp.set_actor_label('MeshTerrain')
# Never edit the engine's MPD_Default: duplicate it under /Game/Frontier and point the actor at the copy.
DEF = '/Game/Frontier/Maps/Dev/TerrainSpike/MPD_TerrainSpike'
defn = u.EditorAssetLibrary.duplicate_asset(mp.get_mesh_partition_definition().get_path_name(), DEF) if not u.EditorAssetLibrary.does_asset_exist(DEF) else u.load_asset(DEF)
defn.set_editor_property('material', u.load_asset('/Game/Frontier/Environment/Greenlands/Nature/Materials/MI_GN_Ground'))
mp.set_editor_property('mega_mesh_definition', defn)
assert mp.get_mesh_partition_definition().get_path_name().startswith('/Game/Frontier/'), 'definition must live under /Game/Frontier'
actors.spawn_actor_from_class(u.PlayerStart, u.Vector(2400, 1200, base_h(24, 12) * 100 + 150), u.Rotator(yaw=90))
sun = actors.spawn_actor_from_class(u.DirectionalLight, u.Vector(0, 0, 3000), u.Rotator(pitch=-35, yaw=-50))
sun.light_component.set_atmosphere_sun_light(True)
actors.spawn_actor_from_class(u.SkyLight, u.Vector(0, 0, 2000)).light_component.set_editor_property('real_time_capture', True)
actors.spawn_actor_from_class(u.SkyAtmosphere, u.Vector(0, 0, 0))
actors.spawn_actor_from_class(u.ExponentialHeightFog, u.Vector(0, 0, 0))
cam = actors.spawn_actor_from_class(u.CameraActor, u.Vector(-2500, -2500, 4500))  # capture camera for mesh_terrain_checks.py
cam.camera_component.set_editor_property('field_of_view', 70.0)
cam.set_editor_property('is_spatially_loaded', False)
# Save only /Game packages (map, external actors, definition); never an engine/plugin package.
S = u.EditorLoadingAndSavingUtils
dirty = list(S.get_dirty_map_packages()) + list(S.get_dirty_content_packages())
skipped = [p.get_name() for p in dirty if not p.get_name().startswith('/Game/')]
S.save_packages([p for p in dirty if p.get_name().startswith('/Game/')], True)
u.log('MESH_TERRAIN_SAVE saved=%d skipped_non_game=%s' % (len(dirty) - len(skipped), skipped))
u.log('MESH_TERRAIN_MAP_CREATED %s import_s=%.2f lo_m=%.2f hi_m=%.2f definition=%s class=%s' % (MAP, import_s, lo, hi, defn.get_path_name() if defn else None, mp.get_class().get_name()))
