"""Five fixed Silverbrook WP6d views (no TextRender show-flag override: labels must be hidden in game by bHiddenInGame); run with -game and -ExecCmds=py <this script>."""
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
    ('main-street', (29.0, 336, 1.7), (4.5, -30, 4), 70),
    ('market', (30, -62, 3.0), (0, -30, 1.2), 70),
    ('house-frontage', (23, 311, 2.2), (17, 315.9, 2.4), 65),
    ('foundation-close', (23, 311, 1.0), (17, 315.9, 0.65), 48),
]

smokes = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor) if a.get_actor_label().startswith('ChimneySmoke_')]
if smokes:  # extra view: the smoke card nearest the frontage house, seen from the street
    sp = min(smokes, key=lambda a: (a.get_actor_location().x/100 - 17)**2 + (a.get_actor_location().y/100 - 316)**2).get_actor_location()
    # Eye 16 m away at pot height (absolute z), aimed at the 2.2 x 6 m card centre.
    views.append(('chimney-smoke', (sp.x/100 + 3, sp.y/100 + 15.7, None, sp.z - 280), (sp.x/100, sp.y/100, None, sp.z), 55))
lamps = [a.get_actor_location() for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
         if a.get_actor_label().startswith('StreetLamp')]

def ground(x, y):
    hit = terrain.static_mesh_component.line_trace_component(u.Vector(x*100, y*100, 100000),
                                                               u.Vector(x*100, y*100, -100000), True, False)
    assert hit, (x, y)
    return hit[0].z

for cmd in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0', 'r.MotionBlurQuality 0',
            'r.ScreenPercentage 100'):
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
        x, y, h = eye[:3]; tx, ty, th = look[:3]
        z = eye[3] if len(eye) > 3 else ground(x, y) + h*100
        tz = look[3] if len(look) > 3 else ground(tx, ty) + th*100
        d = u.Vector(tx*100 - x*100, ty*100 - y*100, tz - z); d = d / d.length()
        e = u.Vector(x*100, y*100, z)
        block = [l for l in lamps if 0 < (l - e).dot(d) < 1000 and (l - e).cross(d).length() < 150]
        assert not block, (name, 'street lamp within 1.5 m of the sight line in the first 10 m', block)
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
