"""Silverbrook water / bridge / swim / harvest-tree / hay captures (owner playtest 2026-10-26). Run with -game:

  UnrealEditor.exe <project> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -RenderOffscreen -unattended -nosound -NoSplash
      -ResX=1920 -ResY=1080 "-ExecCmds=py <this file>" -WaterTag=before|after [-WaterOnly=a,b]

Writes <tag>-<view>.png into WATER_OUT (default C:/Projects/Shattered-Frontier/Artifacts/Evidence/silverbrook-water). Views are derived from the
placed actors (bridges, water strips, Eastwood Pond, nearest gathering node, hay bales), so they survive a map rebuild.
"""
import json, math, os, re
from pathlib import Path
import unreal as u

OUT = Path(os.environ.get('WATER_OUT', 'C:/Projects/Shattered-Frontier/Artifacts/Evidence/silverbrook-water'))
OUT.mkdir(parents=True, exist_ok=True)
cmd = u.SystemLibrary.get_command_line()
tag = (re.search(r'-WaterTag=(\w+)', cmd) or [None, 'after'])[1]
only = (re.search(r'-WaterOnly=([\w,-]+)', cmd) or [None, ''])[1].split(',')
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pc = u.GameplayStatics.get_player_controller(world, 0)
pawn = u.GameplayStatics.get_player_character(world, 0)
all_of = lambda cls: u.GameplayStatics.get_all_actors_of_class(world, cls)
smas = all_of(u.StaticMeshActor)
camera = next(a for a in all_of(u.CameraActor) if a.get_actor_label() == 'ReviewCamera_Aerial')
terrain = next(a for a in smas if a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name() == 'SM_Silverbrook_Terrain')
V = u.Vector


def ground(x, y):
    hit = terrain.static_mesh_component.line_trace_component(V(x, y, 1e5), V(x, y, -1e5), True, False)
    assert hit, (x, y)
    return hit[0].z


def folder(a): return str(a.get_folder_path())


water_planes = [a for a in smas if folder(a) == 'Water' and a.get_actor_label() != 'Sea']


def water_z(x, y):
    p = min(water_planes, key=lambda a: (a.get_actor_location().x - x) ** 2 + (a.get_actor_location().y - y) ** 2)
    return p.get_actor_location().z


bridges = [a for a in smas if folder(a) == 'Bridges' and re.fullmatch(r'(SilverbrookBridge|Bridge_\w+)', a.get_actor_label())]
main = next((a for a in bridges if a.get_actor_label() == 'SilverbrookBridge'), bridges[0])
side = next(a for a in bridges if a is not main)


def bridge_frame(a):
    """(centre cm, unit direction along the deck, half length cm)."""
    o, e = a.get_actor_bounds(False)
    yaw = math.radians(a.get_actor_rotation().yaw)
    d = V(math.cos(yaw), math.sin(yaw), 0)
    return o, d, abs(d.x) * e.x + abs(d.y) * e.y


def end_view(a, which):
    o, d, half = bridge_frame(a)
    s = 1 if which == 'b' else -1
    end = V(o.x + s * d.x * half, o.y + s * d.y * half, 0)
    eye = V(o.x + s * d.x * (half - 8), o.y + s * d.y * (half - 8), 0)
    gz = lambda p: ground(p.x, p.y)
    return (eye.x, eye.y, gz(eye) + 260), (end.x + s * d.x * 500, end.y + s * d.y * 500, gz(end) + 30), 75


def river_view(a):
    o, d, half = bridge_frame(a)
    n = V(-d.y, d.x, 0)  # across the deck, i.e. along the river
    eye = V(o.x + n.x * 2500, o.y + n.y * 2500, 0)
    wz = water_z(eye.x, eye.y)
    return (eye.x, eye.y, wz + 130), (o.x, o.y, o.z + 100), 65


def under_view(a):
    o, d, half = bridge_frame(a)
    n = V(-d.y, d.x, 0)
    eye = V(o.x + n.x * 1500, o.y + n.y * 1500, 0)
    wz = water_z(eye.x, eye.y)
    return (eye.x, eye.y, wz - 80), (o.x, o.y, wz + 10), 75


def surface_look(a):
    o, d, half = bridge_frame(a)
    n = V(-d.y, d.x, 0)
    eye = V(o.x + n.x * 1500, o.y + n.y * 1500, 0)
    wz = water_z(eye.x, eye.y)
    return (eye.x, eye.y, wz - 60), (o.x + n.x * 6000, o.y + n.y * 6000, wz + 50), 80


def aerial_end(a, which):
    """The bridge end seen from the bank, 16 m out and 7 m up, looking at the deck end."""
    o, d, half = bridge_frame(a)
    s = 1 if which == 'b' else -1
    end = V(o.x + s * d.x * half, o.y + s * d.y * half, 0)
    eye = V(end.x + s * d.x * 1600, end.y + s * d.y * 1600 + 300, 0)
    return (eye.x, eye.y, ground(eye.x, eye.y) + 700), (end.x, end.y, ground(end.x, end.y) + 50), 70


views = [
    ('bridge-end-main-a', lambda: aerial_end(main, 'a')), ('bridge-end-main-b', lambda: aerial_end(main, 'b')),
    ('bridge-end-side-a', lambda: aerial_end(side, 'a')), ('bridge-end-side-b', lambda: aerial_end(side, 'b')),
    ('river-level-main', lambda: river_view(main)), ('river-level-side', lambda: river_view(side)),
    ('main-street', lambda: ((2900, 33600, ground(2900, 33600) + 170), (450, -3000, ground(450, -3000) + 400), 70)),
    ('house-frontage', lambda: ((2223, 31163, ground(2223, 31163) + 220), (1700, 31590, ground(1700, 31590) + 240), 72.4)),
    ('under-water', lambda: under_view(main)), ('under-surface-look', lambda: surface_look(main)),
]
views = [v for v in views if only == [''] or v[0] in only or any(v[0].startswith(o) for o in only)]

pond = next((a for a in all_of(u.PhysicsVolume) if a.get_actor_label() == 'EastwoodPond_Swim'), None)
state = {'frame': 0, 'index': 0, 'phase': 'views'}


def aim(eye, look, fov):
    x, y, z = eye; tx, ty, tz = look
    yaw = math.degrees(math.atan2(ty - y, tx - x)); pitch = math.degrees(math.atan2(tz - z, math.hypot(tx - x, ty - y)))
    camera.set_actor_location(V(x, y, z), False, False); camera.set_actor_rotation(u.Rotator(pitch=pitch, yaw=yaw), False)
    camera.camera_component.set_field_of_view(fov); pc.set_view_target_with_blend(camera, 0)


for c in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0', 'r.MotionBlurQuality 0', 'r.ScreenPercentage 100'):
    u.SystemLibrary.execute_console_command(world, c)
pawn.set_actor_hidden_in_game(True)
pawn.character_movement.disable_movement()


def shot(name):
    u.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=' + str(OUT / f'{tag}-{name}.png').replace('\\', '/'))
    u.log(f'WATER_CAPTURE {tag}-{name}')


def tick(_dt):
    state['frame'] += 1
    f = state['frame']
    if state['phase'] == 'views':
        if f < 90: return
        local = (f - 90) % 150
        if state['index'] >= len(views):
            state['phase'] = 'swim'; state['frame'] = 0; start_swim(); return
        name, fn = views[state['index']]
        if local == 0:
            eye, look, fov = fn(); aim(eye, look, fov)
            if name == 'bridge-end-main-a':  # debug: what is under the pale box in the river (pixel 690,78 of 1920x1080)
                x, y, z = eye; tx, ty, tz = look
                fw = V(tx - x, ty - y, tz - z); fw = fw / fw.length(); rt = V(-fw.y, fw.x, 0); rt = rt / rt.length(); up = rt.cross(fw) * -1
                th = math.tan(math.radians(fov / 2)); dx = (690 - 960) / 960 * th; dy = (540 - 78) / 960 * th
                d = fw + rt * dx + up * dy
                hit = u.SystemLibrary.line_trace_single(world, V(x, y, z), V(x, y, z) + d * 20000, u.TraceTypeQuery.ECC_VISIBILITY, True, [], u.DrawDebugTrace.NONE, True)
                t = hit.to_tuple(); h = t[9]; u.log(f'WATER_BOXHIT {h.get_actor_label()} {h.get_folder_path()} {h.static_mesh_component.static_mesh.get_name()} {h.get_actor_scale3d()} {t[4]}')
        if local == 120: shot(name)
        if local == 149: state['index'] += 1
    elif state['phase'] == 'swim': swim_tick(f)
    elif state['phase'] == 'props': prop_tick(f)
    elif state['phase'] == 'done':
        u.unregister_slate_post_tick_callback(state['handle']); u.SystemLibrary.execute_console_command(world, 'quit')


def start_swim():
    if not pond or (only != [''] and 'swim' not in only and not any(o.startswith('swim') for o in only)):
        state['phase'] = 'props'; start_props(); return
    o, e = pond.get_actor_bounds(False)
    state['pond'] = (o, e)
    top = o.z + e.z
    pawn.set_actor_hidden_in_game(False)
    pawn.character_movement.set_movement_mode(u.MovementMode.MOVE_WALKING)
    pawn.set_actor_location(V(o.x, o.y, top - 40), False, False)
    pc.set_view_target_with_blend(pawn, 0)


def swim_tick(f):
    o, e = state['pond']
    if f == 5: pawn.character_movement.set_movement_mode(u.MovementMode.MOVE_SWIMMING)
    if f > 20:
        pawn.add_movement_input(V(1, 0, 0), 1.0)  # swim east; the pose follows the swim flag
        pc.set_control_rotation(u.Rotator(pitch=-4, yaw=90 if f < 200 else 180))
    if f in (145, 255):
        l = pawn.get_actor_location(); side = f == 145
        eye = (l.x + (430 if side else 100), l.y + (30 if side else 430), o.z + e.z + 45); aim(eye, (l.x, l.y, o.z + e.z - 20), 60)
    if f in (150, 260):
        top = o.z + e.z
        u.log(f'WATER_SWIM z={pawn.get_actor_location().z:.0f} surface={top:.0f} mode={pawn.character_movement.movement_mode}')
        shot('swim-side' if f == 150 else 'swim-front')
    if f == 300:
        pawn.set_actor_hidden_in_game(True); pawn.character_movement.disable_movement()
        state['phase'] = 'props'; state['frame'] = 0; start_props()


def start_props():
    state['pidx'] = 0
    if only != [''] and not any(o in ('tree', 'hay') or o.startswith(('tree', 'hay')) for o in only):
        state['phase'] = 'done'; return
    # nearest gathering oak to the arrival point from the zone record, then wait for its node to spawn
    rec = json.loads(Path(u.Paths.project_dir()).parent.joinpath('content/world/zones/greenlands_silverbrook.json').read_text())
    pts = [p['position'] for g in rec['spawn_groups'] for d in g.get('definitions', []) if d['definition_id'].endswith('oak_tree') for p in g['points']]
    pts.sort(key=lambda p: p[0] ** 2 + (p[1] + 30) ** 2)
    state['tree_pts'] = pts[:1]


def prop_tick(f):
    if f == 1 and state['pidx'] == 0:
        t = state['tree_pts'][0]
        pawn.set_actor_hidden_in_game(True)
        pawn.character_movement.set_movement_mode(u.MovementMode.MOVE_WALKING)
        pawn.set_actor_location(V(t[0] * 100 - 600, t[1] * 100, t[2] * 100 + 150), False, False)
    if f == 240 and state['pidx'] == 0:
        t = state['tree_pts'][0]
        nodes = [a for a in all_of(u.Actor) if a.get_class().get_name() == 'SFGatheringPilotNode']
        n = min(nodes, key=lambda a: (a.get_actor_location().x - t[0] * 100) ** 2 + (a.get_actor_location().y - t[1] * 100) ** 2)
        l = n.get_actor_location()
        u.log(f'WATER_NODE {n.get_name()} at {l.x:.0f},{l.y:.0f},{l.z:.0f}')
        for m in nodes:  # what each streamed node shows: its own dressing mesh, or the shared harvest art
            ml = m.get_actor_location(); d = math.hypot(ml.x - l.x, ml.y - l.y) / 100
            if d < 60: u.log(f'WATER_NODE_MESHES {m.get_name()} d={d:.0f}m ' + ','.join(sorted({c.static_mesh.get_name() for c in m.get_components_by_class(u.StaticMeshComponent) if c.static_mesh and c.is_visible()})))
        aim((l.x - 520, l.y - 100, l.z + 120), (l.x, l.y, l.z + 260), 60)
    if f == 330 and state['pidx'] == 0:
        shot('tree-node'); state['pidx'] = 1; state['hf'] = f
    if state['pidx'] == 1 and f == state['hf'] + 10:
        hay = [a for a in smas if a.static_mesh_component.static_mesh and 'HayBales' in a.static_mesh_component.static_mesh.get_name()]
        if not hay: state['phase'] = 'done'; return
        h = min(hay, key=lambda a: (a.get_actor_location().x) ** 2 + (a.get_actor_location().y + 3000) ** 2)
        l = h.get_actor_location(); state['hay'] = l
        aim((l.x + 330, l.y - 330, l.z + 190), (l.x, l.y, l.z + 50), 55)
    if state['pidx'] == 1 and f == state['hf'] + 100: shot('hay-bales'); state['phase'] = 'done'


state['handle'] = u.register_slate_post_tick_callback(tick)
