"""Silverbrook -game capture: -ExecCmds="py C:/Projects/Shattered-Frontier/docs/evidence/graphics/2026-10-21-wp5-backdrop/capture_game.py"."""
import math
from pathlib import Path
import unreal as u

OUT = Path(__file__).resolve().parent
VIEWS = [
    ('02-main-street-game', (22, 330), (0, -30), 4),
    ('edge-west', (-1100, 0), (-2300, 0), None),
    ('edge-east', (1100, 0), (2300, 0), None),
    ('edge-south', (0, 1100), (0, 2300), None),
    ('high-ground-northeast', (900, -900), (1900, -1900), None),
]
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
camera = next(c for c in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor)
              if c.get_actor_label() == 'ReviewCamera_Aerial')
pawn = u.GameplayStatics.get_player_character(world, 0)
pc = u.GameplayStatics.get_player_controller(world, 0)
for command in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0',
                'r.MotionBlurQuality 0', 'r.ScreenPercentage 100', 'ShowFlag.TextRender 0'):
    u.SystemLibrary.execute_console_command(world, command)
if pawn:
    pawn.character_movement.disable_movement()
    pawn.set_actor_hidden_in_game(True)
st = dict(frame=0, index=0)


def ground(x, y):
    hit = u.SystemLibrary.line_trace_single(
        world, u.Vector(x * 100, y * 100, 300000),
        u.Vector(x * 100, y * 100, -300000), u.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, [pawn] if pawn else [], u.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 0


def tick(_delta):
    st['frame'] += 1
    if st['frame'] < 90:
        return
    local = (st['frame'] - 90) % 150
    name, (x, y), (tx, ty), look_height = VIEWS[st['index']]
    if local == 0:
        z = ground(x, y) + 170
        tz = ground(tx, ty) + look_height * 100 if look_height is not None else z
        camera.set_actor_location(u.Vector(x * 100, y * 100, z), False, False)
        camera.set_actor_rotation(u.Rotator(
            pitch=math.degrees(math.atan2(tz - z, math.hypot(tx - x, ty - y) * 100)),
            yaw=math.degrees(math.atan2(ty - y, tx - x)), roll=0), False)
        camera.camera_component.set_field_of_view(70)
        pc.set_view_target_with_blend(camera, 0)
    if local == 125:
        path = OUT / (name + '.png')
        u.SystemLibrary.execute_console_command(world,
            'HighResShot 1920x1080 filename=' + path.as_posix())
        u.log('WP5_CAPTURE ' + str(path))
    if local == 149:
        st['index'] += 1
        if st['index'] == len(VIEWS):
            u.unregister_slate_post_tick_callback(st['h'])
            u.SystemLibrary.execute_console_command(world, 'quit')


st['h'] = u.register_slate_post_tick_callback(tick)
