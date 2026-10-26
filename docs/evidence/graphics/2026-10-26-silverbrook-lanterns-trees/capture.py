"""Silverbrook door lanterns, realistic trees, felling art (owner decisions 2026-10-26). Run with -game:

  UnrealEditor.exe <project> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -RenderOffscreen -unattended -nosound -NoSplash
      -ResX=1920 -ResY=1080 "-ExecCmds=py <this file>" -LTTag=before|after [-LTOnly=street,tree,forest,pine,broadleaf,oak,rock]

Writes <tag>-<view>.png into LT_OUT (default C:/Projects/Shattered-Frontier/Artifacts/Evidence/silverbrook-lanterns-trees) and
<tag>-frametime.json (average and p95 frame time over 300 frames at the main-street view, VSync and the frame cap off).
Chop sequences call the node's DevPreviewFelling (0 = standing, 1..2 = scars, 3 = the fall) under slomo; the 'before' tag skips
them (the old build had no felling art: the node simply vanished on the last hit).
"""
import json, math, os, re
from pathlib import Path
import unreal as u

OUT = Path(os.environ.get('LT_OUT', 'C:/Projects/Shattered-Frontier/Artifacts/Evidence/silverbrook-lanterns-trees'))
OUT.mkdir(parents=True, exist_ok=True)
cmd = u.SystemLibrary.get_command_line()
tag = (re.search(r'-LTTag=(\w+)', cmd) or [None, 'after'])[1]
only = (re.search(r'-LTOnly=([\w,-]+)', cmd) or [None, ''])[1].split(',')
want = lambda n: only == [''] or n in only
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pc = u.GameplayStatics.get_player_controller(world, 0)
pawn = u.GameplayStatics.get_player_character(world, 0)
all_of = lambda cls: u.GameplayStatics.get_all_actors_of_class(world, cls)
smas = all_of(u.StaticMeshActor)
camera = next(a for a in all_of(u.CameraActor) if a.get_actor_label() == 'ReviewCamera_Aerial')
terrain = next(a for a in smas if a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name() == 'SM_Silverbrook_Terrain')
V = u.Vector
ROOT = Path(u.Paths.project_dir()).parent
exe = lambda c: u.SystemLibrary.execute_console_command(world, c)


def ground(x, y):
    hit = terrain.static_mesh_component.line_trace_component(V(x, y, 1e5), V(x, y, -1e5), True, False)
    assert hit, (x, y)
    return hit[0].z


def aim(eye, look, fov):
    x, y, z = eye
    tx, ty, tz = look
    yaw = math.degrees(math.atan2(ty - y, tx - x))
    pitch = math.degrees(math.atan2(tz - z, math.hypot(tx - x, ty - y)))
    camera.set_actor_location(V(x, y, z), False, False)
    camera.set_actor_rotation(u.Rotator(pitch=pitch, yaw=yaw), False)
    camera.camera_component.set_field_of_view(fov)
    pc.set_view_target_with_blend(camera, 0)


def shot(name):
    exe('HighResShot 1920x1080 filename=' + str(OUT / f'{tag}-{name}.png').replace('\\', '/'))
    u.log(f'LT_CAPTURE {tag}-{name}')


# Dressing candidates (metres) from the saved candidate files: role -> [(x, y, z)]
cands = {}
for f in sorted((ROOT / 'GameClient/ArtSource/Greenlands').glob('silverbrook-*-candidates.json')):
    for c in json.loads(f.read_text(encoding='utf-8'))['candidates']:
        cands.setdefault(c['role'], []).append(tuple(c['at']))
GATE = (520.0, 0.0)   # the East Gate road: take the candidate nearest it so the node is easy to reach


def nearest(role):
    return min(cands[role], key=lambda p: (p[0] - GATE[0]) ** 2 + (p[1] - GATE[1]) ** 2)


species = [(n, nearest(r)) for n, r in (('pine', 'tree_pine_tall'), ('broadleaf', 'tree_broadleaf_young'), ('oak', 'tree_oak'), ('rock', 'boulder_large_moss'))
           if want(n) and r in cands]
pines = cands['tree_pine_tall']
crowd = lambda p: sum(1 for q in pines if (q[0] - p[0]) ** 2 + (q[1] - p[1]) ** 2 < 900)
forest_pt = max(pines[::7], key=crowd)
forest_nb = [q for q in pines if 4 < math.hypot(q[0] - forest_pt[0], q[1] - forest_pt[1]) < 30]
fx = sum(q[0] for q in forest_nb) / len(forest_nb)
fy = sum(q[1] for q in forest_nb) / len(forest_nb)
oak_pt = nearest('tree_oak') if 'tree_oak' in cands else None

views = []
if want('street'):
    views += [('main-street', lambda: ((2900, 33600, ground(2900, 33600) + 170), (450, -3000, ground(450, -3000) + 400), 70)),
              ('house-frontage', lambda: ((2223, 31163, ground(2223, 31163) + 220), (1700, 31590, ground(1700, 31590) + 240), 72.4))]
if want('tree') and oak_pt:
    views.append(('tree-close', lambda: ((oak_pt[0] * 100 - 1250, oak_pt[1] * 100 - 300, oak_pt[2] * 100 + 170),
                                         (oak_pt[0] * 100, oak_pt[1] * 100, oak_pt[2] * 100 + 550), 62), oak_pt))
if want('forest'):
    views.append(('forest', lambda: ((forest_pt[0] * 100 + 300, forest_pt[1] * 100, forest_pt[2] * 100 + 170),
                                     (fx * 100, fy * 100, forest_pt[2] * 100 + 600), 85), forest_pt))
state = {'frame': 0, 'index': 0, 'phase': 'views', 'times': [], 'sp': 0, 'step': 0}

for c in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0', 'r.MotionBlurQuality 0', 'r.ScreenPercentage 100', 'r.VSync 0', 't.MaxFPS 0'):
    exe(c)
pawn.set_actor_hidden_in_game(True)
pawn.character_movement.disable_movement()
# Tuning aid: LT_DOOR_CD (candela), LT_DOOR_OUT (cm further out of the wall), LT_DOOR_TILT (deg down) and LT_DOOR_RGB ("r,g,b")
# override every door lantern light for this run only.
if os.environ.get('LT_DOOR_CD'):
    for a in all_of(u.SpotLight):
        if 'DoorLight' in a.get_actor_label():
            lc = a.get_component_by_class(u.SpotLightComponent)
            lc.set_intensity(float(os.environ['LT_DOOR_CD']))
            if os.environ.get('LT_DOOR_RGB'):
                r, g, b = [int(v) for v in os.environ['LT_DOOR_RGB'].split(',')]
                lc.set_light_color(u.Color(r=r, g=g, b=b, a=255))
            fw = a.get_actor_forward_vector()
            flat = V(fw.x, fw.y, 0)
            flat = flat / flat.length()
            a.add_actor_world_offset(flat * float(os.environ.get('LT_DOOR_OUT', 0)), False, False)
            if os.environ.get('LT_DOOR_TILT'):
                a.set_actor_rotation(u.MathLibrary.make_rot_from_x(V(flat.x, flat.y, -math.tan(math.radians(float(os.environ['LT_DOOR_TILT']))))), False)


def node_near(p):
    nodes = [a for a in all_of(u.Actor) if a.get_class().get_name() == 'SFGatheringPilotNode']
    if not nodes:
        return None
    n = min(nodes, key=lambda a: (a.get_actor_location().x - p[0] * 100) ** 2 + (a.get_actor_location().y - p[1] * 100) ** 2)
    l = n.get_actor_location()
    return n if math.hypot(l.x - p[0] * 100, l.y - p[1] * 100) < 100 else None


def move_pawn_near(p):
    pawn.character_movement.set_movement_mode(u.MovementMode.MOVE_WALKING)
    pawn.set_actor_location(V(p[0] * 100 - 600, p[1] * 100, p[2] * 100 + 150), False, False)


def tick(dt):
    state['frame'] += 1
    f = state['frame']
    ph = state['phase']
    if ph == 'views':
        if f < 90:
            return
        local = (f - 90) % 330
        if state['index'] >= len(views):
            state['phase'] = 'chop'
            state['frame'] = 0
            return
        v = views[state['index']]
        name = v[0]
        if local == 0:
            if len(v) > 2:
                move_pawn_near(v[2])   # the nodes around it stream in and replace the dressing there
            eye, look, fov = v[1]()
            aim(eye, look, fov)
        if name == 'main-street' and 20 <= local < 320:
            state['times'].append(dt)
        if local == 300:
            shot(name)
        if local == 329:
            if name == 'main-street':
                t = sorted(state['times'])
                n = len(t)
                (OUT / f'{tag}-frametime.json').write_text(json.dumps({'frames': n, 'avg_ms': 1000 * sum(t) / n, 'p95_ms': 1000 * t[int(n * .95)], 'max_ms': 1000 * t[-1]}))
                u.log(f'LT_FRAMETIME avg {1000 * sum(t) / n:.2f} ms p95 {1000 * t[int(n * .95)]:.2f} ms over {n} frames')
            state['index'] += 1
    elif ph == 'chop':
        chop_tick(f)
    elif ph == 'done':
        u.unregister_slate_post_tick_callback(state['handle'])
        exe('quit')


def chop_tick(f):
    """Per species: wait for the node, then standing (wide), scars 1 and 2 (close), the fall (side-on, slomo), the felled spot."""
    if state['sp'] >= len(species):
        state['phase'] = 'done'
        return
    name, p = species[state['sp']]
    s = state['step']
    t0 = state.setdefault('t0', f)
    r = f - t0
    n = state.get('node')
    base = (p[0] * 100, p[1] * 100, p[2] * 100)
    if s == 0:
        if r == 1:
            move_pawn_near(p)
        if r > 60 and r % 20 == 0:
            n = node_near(p)
            if n:
                state['node'] = n
                state['step'] = 1
                state['t0'] = f
                u.log(f'LT_NODE {name} {n.get_name()}')
        if r > 700:
            u.log(f'LT_NODE_MISSING {name}')
            state['sp'] += 1
            state['t0'] = f
        return
    if s == 1:   # standing, wide
        if r == 1:
            n.dev_preview_felling(0)
        if r == 2:
            point, fall, h = n.dev_felling_info()
            state['h'] = h or 1500
            d = max(state['h'] * 1.3, 700)
            aim((base[0] - d, base[1] - d * .25, base[2] + 170), (base[0], base[1], base[2] + state['h'] * .45), 60 if state['h'] < 3000 else 70)
        if r == 40:
            shot(f'{name}-0-standing')
            state['step'] = 2 if tag != 'before' else 0   # the old build has no felling art: the standing view is all there is
            state['sp'] += tag == 'before'
            state['node'] = state['node'] if tag != 'before' else None
            state['t0'] = f
    elif s in (2, 3):   # scars 1 and 2, close
        k = s - 1
        if r == 1:
            n.dev_preview_felling(k)
        if r == 4:
            point, fall, h = n.dev_felling_info()
            out = V(point.x - base[0], point.y - base[1], 0)
            out = out / out.length() if out.length() else V(1, 0, 0)
            dist = 330 if name != 'rock' else 380
            aim((point.x + out.x * dist, point.y + out.y * dist, point.z + 30), (point.x, point.y, point.z), 50)
        if r == 40:
            shot(f'{name}-{k}-notch')
            state['step'] += 1
            state['t0'] = f
    elif s == 4:   # the fall or crumble, side-on, slowed
        if r == 1:
            exe('slomo 0.2')
            n.dev_preview_felling(3)
        if r == 2:
            point, fall, h = n.dev_felling_info()
            hh = state['h']
            side = V(-fall.y, fall.x, 0) if fall.length() else V(0, 1, 0)
            d = max(hh * 1.5, 700)
            tx, ty = base[0] + fall.x * hh * .35, base[1] + fall.y * hh * .35
            aim((tx + side.x * d, ty + side.y * d, base[2] + hh * .4), (tx, ty, base[2] + hh * .35), 55)
        for i, at in enumerate((6, 30, 70, 120, 200, 330)):
            if r == at:
                shot(f'{name}-3-fall{i}')
        if r == 345:
            exe('slomo 1')
            state['step'] = 5
            state['t0'] = f
    elif s == 5:   # felled: the stump or what is left of the rock
        if r == 1:
            aim((base[0] - 380, base[1] - 120, base[2] + 140), (base[0], base[1], base[2] + 40), 55)
        if r == 700:
            shot(f'{name}-4-felled')
            state['sp'] += 1
            state['step'] = 0
            state['t0'] = f
            state.pop('node', None)


state['handle'] = u.register_slate_post_tick_callback(tick)
