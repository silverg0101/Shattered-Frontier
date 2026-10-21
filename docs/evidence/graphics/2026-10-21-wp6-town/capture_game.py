"""Three fixed Silverbrook WP6 views; run with -game and -ExecCmds=py <this script>."""
import math
from pathlib import Path
import unreal as u

OUT = Path(__file__).resolve().parent
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pc = u.GameplayStatics.get_player_controller(world, 0)
pawn = u.GameplayStatics.get_player_character(world, 0)
camera = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor)
              if a.get_actor_label() == 'ReviewCamera_Aerial')
terrain = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
               if a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name() == 'SM_Silverbrook_Terrain')
views = [
    ('main-street', (22, 330, 1.7), (0, -30, 4), 70),
    ('market', (55, -82, 5), (0, -30, 0), 70),
    ('house-frontage', (23, 311, 2.2), (17, 315.9, 2.4), 65),
    ('armour-stall', (31, -34, 2.3), (25.5, -25.9, 2), 65),
]

def ground(x, y):
    hit = terrain.static_mesh_component.line_trace_component(u.Vector(x*100, y*100, 100000),
                                                               u.Vector(x*100, y*100, -100000), True, False)
    assert hit, (x, y)
    return hit[0].z

for cmd in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0', 'r.MotionBlurQuality 0',
            'r.ScreenPercentage 100', 'ShowFlag.TextRender 0'):
    u.SystemLibrary.execute_console_command(world, cmd)
pawn.set_actor_hidden_in_game(True)
pawn.character_movement.disable_movement()
state = {'frame': 0, 'index': 0}

def tick(_delta):
    state['frame'] += 1
    if state['frame'] < 60: return
    local = (state['frame'] - 60) % 140
    name, eye, look, fov = views[state['index']]
    if local == 0:
        x, y, h = eye; tx, ty, th = look
        z, tz = ground(x, y) + h*100, ground(tx, ty) + th*100
        yaw = math.degrees(math.atan2(ty-y, tx-x))
        pitch = math.degrees(math.atan2(tz-z, math.hypot(tx-x, ty-y)*100))
        camera.set_actor_location(u.Vector(x*100, y*100, z), False, False)
        camera.set_actor_rotation(u.Rotator(pitch=pitch, yaw=yaw), False)
        camera.camera_component.set_field_of_view(fov)
        pc.set_view_target_with_blend(camera, 0)
    if local == 115:
        u.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=' + str(OUT / (name+'.png')).replace('\\','/'))
        u.log('WP6_CAPTURE ' + name)
    if local == 139:
        state['index'] += 1
        if state['index'] == len(views):
            u.unregister_slate_post_tick_callback(state['handle'])
            u.SystemLibrary.execute_console_command(world, 'quit')

state['handle'] = u.register_slate_post_tick_callback(tick)
