"""GFX-01 WP5: granite domes at 700-1500 m plus a textured distant mountain ring that replaces the white range.

Blender (builds SM_MountainRing.fbx beside the dome family):
  "C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P GameClient/ArtSource/backdrop.py
Editor (imports the ring, places the SM_GraniteDomeFar_* family and the ring in the open Silverbrook map, hides the white `Backdrop`):
  python tools/Run-InEditor.py GameClient/ArtSource/backdrop.py
Values: silverbrook-backdrop.json. Materials reuse MI_GraniteDomeFar_00 (rock_08 + forest_ground_04 blended by vertex colour R = forested low flanks).
Vertex colour G marks the snow-line cap. No collision, Nanite, cosmetic only.
"""
import json, math, random
from pathlib import Path

ROOT = Path(__file__).resolve().parent
FAMILY = ROOT / 'Greenlands/NatureFamilies/GraniteDomeFar'
RING_FBX = FAMILY / 'SM_MountainRing.fbx'
NATURE = '/Game/Frontier/Environment/Greenlands/Nature'
ZONE = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'


def config():
    raw = json.loads((ROOT / 'silverbrook-backdrop.json').read_text(encoding='utf-8'))
    c = {}
    for k, v in raw.items():
        if k.startswith('_'):
            continue
        for key, e in ({'': v} if 'value' in v else {f'{k}.{kk}': vv for kk, vv in v.items()}).items():
            assert e['min'] <= e['value'] <= e['max'], f"{key or k}={e['value']} outside [{e['min']}, {e['max']}] {e['unit']}"
            c[key or k] = e['value']
    assert c['domes.radius_min_m'] <= c['domes.radius_max_m'] and c['domes.scale_min'] <= c['domes.scale_max']
    assert c['ring.inner_radius_m'] < c['ring.outer_radius_m']
    return c


def build_ring(c):
    import bpy, numpy as np
    bpy.ops.wm.read_factory_settings(use_empty=True)
    ri, ro, H, drop = c['ring.inner_radius_m'], c['ring.outer_radius_m'], c['ring.peak_height_m'], c['ring.inner_drop_m']
    na, nr = 720, 120
    rng = np.random.default_rng(int(c['seed']))
    ph = rng.uniform(0, 6.28, (8, 3))
    th = np.linspace(0, 2 * math.pi, na, endpoint=False)
    t = np.linspace(0, 1, nr)
    T, R = np.meshgrid(th, t)  # rows = radial
    r = ri + (ro - ri) * R
    # ridged angular noise (periodic in theta): ranges and gaps, not a wall
    ang = sum((1 / (k + 1)) * np.abs(np.sin((k + 2) * T + ph[k, 0] + 1.7 * R * (k + 1))) for k in range(6))
    ang = ang / ang.max()
    ridge = 0.35 + 0.65 * ang * (0.6 + 0.4 * np.sin(5 * T + 9 * R + ph[6, 1]) ** 2)
    prof = np.clip(np.sin(math.pi * np.clip(R * 1.15, 0, 1)), 0, 1) ** 0.8
    z = H * prof * ridge - drop * (1 - np.clip(R * 6, 0, 1))
    z = z + 25 * np.sin(T * 61 + ph[7, 0]) * np.sin(R * 83 + ph[7, 2]) * prof  # rock breakup
    V = np.stack([r * np.cos(T), r * np.sin(T), z], -1).reshape(-1, 3)
    idx = np.arange(nr * na).reshape(nr, na)
    a, b = idx[:-1], idx[1:]
    F = np.stack([a, np.roll(a, -1, 1), np.roll(b, -1, 1), b], -1).reshape(-1, 4)
    gy, gx = np.gradient(z)
    slope = np.hypot(gx / ((ro - ri) / nr), gy / (2 * math.pi * r / na)).reshape(-1)
    zn = np.clip(z.reshape(-1) / H, 0, 1)
    forest = np.clip((0.45 - zn) / 0.2, 0, 1) * np.clip((1.2 - slope) / 0.8, 0, 1)  # low and gentle
    snow = np.clip((zn - c['ring.snow_line_fraction']) / 0.1, 0, 1)
    me = bpy.data.meshes.new('SM_MountainRing')
    me.from_pydata(V.tolist(), [], F.tolist())
    me.update()
    for p in me.polygons:
        p.use_smooth = True
    uvl = me.uv_layers.new(name='UVMap')
    col = me.color_attributes.new(name='Color', type='BYTE_COLOR', domain='POINT')
    m = c['ring.uv_metres_per_tile']
    for i in range(len(V)):
        col.data[i].color = (float(forest[i]), float(snow[i]), 0.0, 1.0)
    for lp in me.loops:
        x, y, _ = V[lp.vertex_index]
        uvl.data[lp.index].uv = (x / m, y / m)
    me.materials.append(bpy.data.materials.new('MI_GraniteDomeFar_00'))
    o = bpy.data.objects.new('SM_MountainRing', me)
    bpy.context.collection.objects.link(o)
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=str(RING_FBX), use_selection=True, axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
    print('RING_BUILT', RING_FBX, len(V), 'verts')


def place(c):
    import unreal as u
    eal, eas = u.EditorAssetLibrary, u.get_editor_subsystem(u.EditorActorSubsystem)
    levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
    if levels.get_current_level().get_outermost().get_name() != ZONE:
        levels.load_level(ZONE)
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    task = u.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = str(RING_FBX), f'{NATURE}/Meshes', 'SM_MountainRing'
    task.automated, task.save, task.replace_existing = True, True, True
    o = u.FbxImportUI()
    o.import_materials = o.import_textures = o.import_as_skeletal = False
    d = o.static_mesh_import_data
    d.combine_meshes, d.auto_generate_collision, d.generate_lightmap_u_vs = True, False, False
    d.vertex_color_import_option = u.VertexColorImportOption.REPLACE
    task.options = o
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    ring = u.load_asset(f'{NATURE}/Meshes/SM_MountainRing')
    assert ring, 'ring import failed'
    mi = u.load_asset(f'{NATURE}/Materials/MI_GraniteDomeFar_00')
    assert mi, 'MI_GraniteDomeFar_00 missing; run NatureFamilies/import_nature.py GraniteDomeFar'
    for i in range(len(ring.static_materials)):
        ring.set_material(i, mi)
    ns = ring.get_editor_property('nanite_settings')
    ns.set_editor_property('enabled', True)
    ring.set_editor_property('nanite_settings', ns)
    eal.save_loaded_asset(ring)

    for a in eas.get_all_level_actors():  # idempotent: drop what this script placed before
        if a.get_folder_path() == 'Backdrop':
            eas.destroy_actor(a)

    def spawn(mesh, label, loc, yaw, scale):
        a = eas.spawn_actor_from_class(u.StaticMeshActor, loc, u.Rotator(0, 0, yaw))
        a.static_mesh_component.set_static_mesh(mesh)
        a.set_actor_scale3d(u.Vector(scale, scale, scale))
        a.static_mesh_component.set_collision_profile_name('NoCollision')
        a.static_mesh_component.set_cast_shadow(False)
        a.set_actor_label(label)
        a.set_folder_path('Backdrop')
        return a

    spawn(ring, 'MountainRing', u.Vector(0, 0, 0), 0, 1.0)
    rnd = random.Random(int(c['seed']))
    domes = [u.load_asset(f'{NATURE}/Meshes/SM_GraniteDomeFar_0{i}') for i in (0, 1)]
    n = int(c['domes.count'])
    for i in range(n):
        ang = (i + rnd.uniform(-0.3, 0.3)) / n * 2 * math.pi
        rad = rnd.uniform(c['domes.radius_min_m'], c['domes.radius_max_m'])
        s = rnd.uniform(c['domes.scale_min'], c['domes.scale_max'])
        x, y = rad * math.cos(ang) * 100, rad * math.sin(ang) * 100
        z = ground_z(u, world, x, y)
        mesh = domes[i % 2]
        h = mesh.get_bounding_box().max.z * s
        spawn(mesh, f'GraniteDomeFar_{i:02d}', u.Vector(x, y, z - h * c['domes.sink_fraction']), rnd.uniform(0, 360), s)
    for a in eas.get_all_level_actors():  # the white graybox range
        if a.get_actor_label() == 'Backdrop' and a.get_folder_path() == 'Ground':
            a.set_actor_hidden_in_game(True)
            a.set_is_temporarily_hidden_in_editor(True)
    assert levels.save_current_level()
    print('BACKDROP_PLACED', n, 'domes + ring')


def ground_z(u, world, x, y):
    """Terrain height (cm) under (x, y): a downward visibility trace; 0 where nothing is hit."""
    res = u.SystemLibrary.line_trace_single(world, u.Vector(x, y, 3e6), u.Vector(x, y, -3e6), u.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                            False, [], u.DrawDebugTrace.NONE, True)
    return res.to_tuple()[4].z if res else 0.0


try:
    import bpy  # noqa: F401
    IN_BLENDER = True
except ImportError:
    IN_BLENDER = False
if __name__ == '__main__' or IN_BLENDER:
    build_ring(config()) if IN_BLENDER else place(config())
