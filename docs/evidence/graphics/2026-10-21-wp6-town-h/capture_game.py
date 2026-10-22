"""Silverbrook WP6h views, run with -game and -ExecCmds=py <this script> (editor closed). No show-flag overrides.

Five review views (all recaptured: dress_all inputs changed this pass), the 14-frame chimney motion set (0.3 s apart,
world time logged to motion-times.json), and context views (docs/visual-review.md): two high overviews, four seeded
ground spots (one on each road out of town, one at the town edge), side and rear views of two townhouses, and a
lantern-lit wall in daylight.
"""
import json, math, random
from pathlib import Path
import unreal as u

OUT = Path(__file__).resolve().parent
LAYOUT = json.loads((OUT.parents[3] / 'GameClient/ArtSource/Greenlands/silverbrook-town-layout.json').read_text(encoding='utf-8'))
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pc = u.GameplayStatics.get_player_controller(world, 0)
pawn = u.GameplayStatics.get_player_character(world, 0)
camera = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor) if a.get_actor_label() == 'ReviewCamera_Aerial')
terrain = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
               if a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name() == 'SM_Silverbrook_Terrain')
statics = u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
props = [(a.get_actor_label(),) + tuple(a.get_actor_bounds(False)) for a in statics
         if str(a.get_folder_path()).startswith('Silverbrook/Dressing') or a.get_actor_label().startswith('StreetLamp')]
houses = [a for a in statics if str(a.get_folder_path()).startswith('Districts/') and a.static_mesh_component.static_mesh
          and a.static_mesh_component.static_mesh.get_name().startswith('SM_Silverbrook') and '_Door' not in a.static_mesh_component.static_mesh.get_name()]


def ground(x, y):   # metres in, cm out
    hit = terrain.static_mesh_component.line_trace_component(u.Vector(x*100, y*100, 100000), u.Vector(x*100, y*100, -100000), True, False)
    assert hit, (x, y)
    return hit[0].z


def clear(e, t, subject=''):   # no dressing prop or lamp within 1.5 m of the sight line in the first 10 m (bounds sphere)
    d = t - e; d = d / d.length()
    return [l for l, c, ext in props if (not subject or subject not in l) and 0 < (c - e).dot(d) < 1000 and (c - e).cross(d).length() < 150 + ext.length()]


def open_sky(e):   # a random spot must not be inside or under a building, bridge or gate (WP6h: one landed inside)
    hit = u.SystemLibrary.line_trace_single(camera, e, e + u.Vector(0, 0, 5000), u.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], u.DrawDebugTrace.NONE, True)
    return not hit


views = []   # (name, eye cm Vector, target cm Vector, fov, subject)
def add(name, eye, look, fov, subject=''):
    views.append((name, eye, look, fov, subject))
def at(x, y, h):   # metres, height above ground in metres
    return u.Vector(x*100, y*100, ground(x, y) + h*100)

add('main-street', at(29.0, 336, 1.7), at(4.5, -30, 4), 70)
add('market', at(30, -62, 3.0), at(0, -30, 1.2), 70)
add('house-frontage', at(22.23, 311.63, 2.2), at(17, 315.9, 2.4), 72.4)
add('foundation-close', at(22.23, 311.63, 1.0), at(17, 315.9, 0.65), 54.1)
smokes = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.NiagaraActor) if a.get_actor_label().startswith('ChimneySmoke_')]
sp = min(smokes, key=lambda a: (a.get_actor_location().x/100 - 17)**2 + (a.get_actor_location().y/100 - 316)**2).get_actor_location()
# WP6g eye: 14 m out, 3.2 m above the pots (the NiagaraActor is pot top + 20 cm), looking 1.5 m down; FOV 55.
chimney = (u.Vector(sp.x + 300, sp.y + 1370, sp.z + 300), u.Vector(sp.x, sp.y, sp.z + 150))
add('chimney-smoke', chimney[0], chimney[1], 55)
MOTION = len(views) - 1   # the motion set follows this view without moving the camera

rng = random.Random(20261022)
# Overviews: 150-250 m above the ground, 35-50 deg down, one from the south-east and one from the north-west.
for name, (bx, by), alt, pitch in (('context-overview-se', (520, 640), 230, 38), ('context-overview-nw', (-560, -620), 200, 42)):
    e = at(bx, by, alt); tx, ty = 0, -30; dist = alt / math.tan(math.radians(pitch))
    dx, dy = tx - bx, ty - by; n = math.hypot(dx, dy); look = u.Vector((bx + dx/n*dist)*100, (by + dy/n*dist)*100, e.z - alt*100)
    add(name, e, look, 80)
# Ground spots on each road out of town (outside the wall), random point and random heading along the road.
roads = LAYOUT['roads']['primary']
for road in ('Silvermere Road', 'Everwood Road', 'Valemere Road'):
    pts = roads[road]
    for _ in range(40):
        i = rng.randrange(len(pts) - 1); f = rng.uniform(.1, .9) if i else rng.uniform(.3, .9)
        (x0, y0), (x1, y1) = pts[i], pts[i + 1]; x, y = x0 + (x1 - x0)*f, y0 + (y1 - y0)*f
        if abs(x) > 1150 or abs(y) > 1150: continue
        sgn = rng.choice((-1, 1)); dx, dy = (x1 - x0)*sgn, (y1 - y0)*sgn; n = math.hypot(dx, dy)
        e, t = at(x, y, 1.7), at(x + dx/n*40, y + dy/n*40, 1.2)
        if open_sky(e) and not clear(e, t): break
    add('context-road-' + road.split()[0].lower(), e, t, 75)
# Town edge: a random point 25 m outside the wall ellipse, looking back at the wall with a random +-30 deg swing.
w = LAYOUT['wall']
for _ in range(40):
    a = rng.uniform(0, 2*math.pi); x, y = (w['semi_axis_x'] + 25)*math.cos(a), (w['semi_axis_y'] + 25)*math.sin(a)
    h = math.atan2(-y, -x) + math.radians(rng.uniform(-30, 30))
    e, t = at(x, y, 1.7), at(x + 40*math.cos(h), y + 40*math.sin(h), 3)
    if open_sky(e) and not clear(e, t): break
add('context-town-edge', e, t, 75)
# Townhouse sides and rear at ground level (owner: floating houses seen from the side): eye 1.7 m, 9 m out.
th = sorted([a for a in houses if a.static_mesh_component.static_mesh.get_name().startswith('SM_SilverbrookTownhouse')], key=lambda a: a.get_actor_label())
picked = 0
for a in rng.sample(th, len(th)):
    if picked == 3: break
    side = ('side-a', 'side-b', 'side-rear')[picked]
    yaw = math.radians(a.get_actor_rotation().yaw); loc = a.get_actor_location()
    lx, ly = ((1, 0) if side == 'side-a' else (-1, 0) if side == 'side-b' else (0, -1))   # mesh +y is the street front
    dx, dy = lx*math.cos(yaw) - ly*math.sin(yaw), lx*math.sin(yaw) + ly*math.cos(yaw)
    x, y = loc.x/100 + dx*9, loc.y/100 + dy*9
    e = at(x, y, 1.7); t = u.Vector(loc.x, loc.y, e.z - 70)
    if not open_sky(e) or clear(e, t, a.get_actor_label()): continue
    add('context-house-' + side, e, t, 70, a.get_actor_label()); picked += 1
# A lantern-lit wall in daylight: the light of a wall lantern on a townhouse, from 6 m in front of it.
lights = [l for l in u.GameplayStatics.get_all_actors_of_class(world, u.PointLight) if '_townhouse_' in l.get_actor_label()]
for l in rng.sample(lights, len(lights)):
    p = l.get_actor_location(); owner = l.get_attach_parent_actor(); c = owner.get_actor_location() if owner else p
    dx, dy = p.x - c.x, p.y - c.y; n = math.hypot(dx, dy) or 1
    x, y = (p.x + dx/n*600)/100, (p.y + dy/n*600)/100; e = at(x, y, 1.7)
    if not clear(e, p, owner.get_actor_label() if owner else ''):
        add('context-lamp-wall', e, p, 60, owner.get_actor_label() if owner else ''); break

for cmd in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0', 'r.MotionBlurQuality 0', 'r.ScreenPercentage 100'):
    u.SystemLibrary.execute_console_command(world, cmd)
pawn.set_actor_hidden_in_game(True)
pawn.character_movement.disable_movement()
state = {'frame': 0, 'index': 0, 'motion': None, 'times': []}
u.log('WP6H_VIEWS ' + json.dumps([v[0] for v in views]))


def shot(name):
    u.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=' + str(OUT / (name + '.png')).replace('\\', '/'))
    u.log('WP6_CAPTURE ' + name)


def tick(_delta):
    state['frame'] += 1
    if state['frame'] < 60: return
    if state['motion'] is not None:   # 14 frames 0.3 s apart in world time
        now = u.GameplayStatics.get_time_seconds(world); k = len(state['times'])
        if now >= state['motion'] + 0.3*k:
            shot(f'motion-{k:02d}'); state['times'].append(round(now, 4))
            if len(state['times']) == 14:
                (OUT / 'motion-times.json').write_text(json.dumps({'world_time_s': state['times']}, indent=1) + '\n')
                state['motion'] = None; state['frame'] = 60 + 138   # local 139 on the next tick: advance
        return
    local = (state['frame'] - 60) % 140
    name, e, t, fov, subject = views[state['index']]
    if local == 0:
        if not name.startswith('context-overview'):
            block = clear(e, t, subject or min(houses, key=lambda a: (a.get_actor_location() - t).length()).get_actor_label())
            assert not block, (name, 'prop within 1.5 m of the sight line in the first 10 m', block[:3])
        u.log(f'WP6_SIGHTLINE_CLEAR {name} props {len(props)}')
        d = t - e
        camera.set_actor_location(e, False, False)
        camera.set_actor_rotation(u.Rotator(pitch=math.degrees(math.atan2(d.z, math.hypot(d.x, d.y))), yaw=math.degrees(math.atan2(d.y, d.x))), False)
        camera.camera_component.set_field_of_view(fov)
        pc.set_view_target_with_blend(camera, 0)
    if local == 115:
        shot(name)
    if local == 139:
        if state['index'] == MOTION and not state['times']:
            state['motion'] = u.GameplayStatics.get_time_seconds(world) + .5; return
        state['index'] += 1
        if state['index'] == len(views):
            u.unregister_slate_post_tick_callback(state['handle'])
            u.SystemLibrary.execute_console_command(world, 'quit')


state['handle'] = u.register_slate_post_tick_callback(tick)
