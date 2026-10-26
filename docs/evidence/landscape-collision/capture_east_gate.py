"""Fixed views of Silverbrook's East Gate (trees, rocks, the wall beside the gatehouse). Run with -game and
-ExecCmds="py <this script>"; PNGs go to %SF_CAPTURE_OUT% (prefix %SF_CAPTURE_TAG%, e.g. before/after)."""
import math, os
from pathlib import Path
import unreal as u

OUT = Path(os.environ.get('SF_CAPTURE_OUT', Path(__file__).resolve().parent))
TAG = os.environ.get('SF_CAPTURE_TAG', 'shot')
OUT.mkdir(parents=True, exist_ok=True)
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pc = u.GameplayStatics.get_player_controller(world, 0)
pawn = u.GameplayStatics.get_player_character(world, 0)
camera = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor)
              if a.get_actor_label() == 'ReviewCamera_Aerial')
terrain = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
               if a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name() == 'SM_Silverbrook_Terrain')
GATE = (379, -170)
# (name, eye x/y/height m, look-at x/y/height m, fov)
views = [
    ('gate-outside', (GATE[0] + 34, GATE[1] - 6, 2.0), (GATE[0], GATE[1], 4), 75),
    ('wall-north-of-gate', (GATE[0] + 22, GATE[1] - 30, 2.0), (GATE[0] - 2, GATE[1] - 22, 4), 70),
    ('wall-south-of-gate', (GATE[0] + 22, GATE[1] + 30, 2.0), (GATE[0] - 2, GATE[1] + 22, 4), 70),
    ('gate-aerial', (GATE[0] + 60, GATE[1], 45), (GATE[0] - 5, GATE[1], 0), 70),
    ('woods-east', (GATE[0] + 25, GATE[1] + 4, 1.7), (GATE[0] + 70, GATE[1] + 10, 2), 80),
    ('pine-base-a', (414, -157, 1.4), (419.9, -161.6, 0.5), 60),   # the two pines nearest the gate road
    ('pine-base-b', (423, -158, 1.4), (428.55, -161.4, 0.5), 60),
]


def ground(x, y):
    hit = terrain.static_mesh_component.line_trace_component(u.Vector(x*100, y*100, 100000),
                                                             u.Vector(x*100, y*100, -100000), True, False)
    return hit[0].z if hit else 0.0


for cmd in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0', 'r.MotionBlurQuality 0', 'r.ScreenPercentage 100'):
    u.SystemLibrary.execute_console_command(world, cmd)
if pawn:
    pawn.set_actor_hidden_in_game(True)
    pawn.character_movement.disable_movement()
state = {'frame': 0, 'index': 0}


def tick(_delta):
    state['frame'] += 1
    if state['frame'] < 120: return
    local = (state['frame'] - 120) % 160
    name, eye, look, fov = views[state['index']]
    if local == 0:
        x, y, h = eye; tx, ty, th = look
        z, tz = ground(x, y) + h*100, ground(tx, ty) + th*100
        camera.set_actor_location(u.Vector(x*100, y*100, z), False, False)
        camera.set_actor_rotation(u.Rotator(pitch=math.degrees(math.atan2(tz-z, math.hypot(tx-x, ty-y)*100)),
                                            yaw=math.degrees(math.atan2(ty-y, tx-x))), False)
        camera.camera_component.set_field_of_view(fov)
        pc.set_view_target_with_blend(camera, 0)
    if local == 130:
        u.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=' + str(OUT / f'{TAG}-{name}.png').replace('\\', '/'))
        u.log('EASTGATE_CAPTURE ' + name)
    if local == 159:
        state['index'] += 1
        if state['index'] == len(views):
            u.unregister_slate_post_tick_callback(state['handle'])
            u.SystemLibrary.execute_console_command(world, 'quit')


state['handle'] = u.register_slate_post_tick_callback(tick)
