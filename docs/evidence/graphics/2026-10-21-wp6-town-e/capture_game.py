"""Five fixed Silverbrook WP6e views (no TextRender show-flag override: labels must be hidden in game by bHiddenInGame); run with -game and -ExecCmds=py <this script>."""
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
    # WP6e: eye 1 m closer along the same line, FOV widened to keep the house width: a street group stands 0.8 m in front of
    # the WP6d eye (StreetGroup_SouthStreet_068_L), now behind the camera.
    ('house-frontage', (22.23, 311.63, 2.2), (17, 315.9, 2.4), 72.4),
    ('foundation-close', (22.23, 311.63, 1.0), (17, 315.9, 0.65), 54.1),
]

smokes = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor) if a.get_actor_label().startswith('ChimneySmoke_')]
if smokes:  # extra view: the smoke card nearest the frontage house, seen from the street
    sp = min(smokes, key=lambda a: (a.get_actor_location().x/100 - 17)**2 + (a.get_actor_location().y/100 - 316)**2).get_actor_location()
    # Eye 14 m out at the card centre height, looking 1.5 m down so the stack is in frame (WP6d review fix 1e).
    views.append(('chimney-smoke', (sp.x/100 + 3, sp.y/100 + 13.7, None, sp.z), (sp.x/100, sp.y/100, None, sp.z - 150), 55))
# Every dressing prop and street lamp, as bounds (WP6d review fix 4b): the sight line is tested against the sphere, not the pivot.
props = [(a.get_actor_label(),) + tuple(a.get_actor_bounds(False)) for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
         if str(a.get_folder_path()).startswith('Silverbrook/Dressing') or a.get_actor_label().startswith('StreetLamp')]
houses = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor) if str(a.get_folder_path()).startswith('Districts/')
          and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name().startswith('SM_Silverbrook')
          and '_Door' not in a.static_mesh_component.static_mesh.get_name()]

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
        # The framed house's own details (sill planter, tubs, sign, ivy) are the subject, not obstructions.
        t = u.Vector(tx*100, ty*100, tz)
        subject = min(houses, key=lambda a: (a.get_actor_location() - t).length()).get_actor_label()
        block = [(l, c) for l, c, ext in props if subject not in l and 0 < (c - e).dot(d) < 1000 and (c - e).cross(d).length() < 150 + ext.length()]
        assert not block, (name, 'prop within 1.5 m of the sight line in the first 10 m', block[:3])
        u.log(f'WP6_SIGHTLINE_CLEAR {name} props {len(props)} subject {subject}')
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
