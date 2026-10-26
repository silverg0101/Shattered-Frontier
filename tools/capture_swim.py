"""Swim captures in Eastwood Pond (-game): swim-side, swim-front (moving) and tread (still).

  UnrealEditor.exe <project> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -RenderOffscreen -unattended -nosound -NoSplash
      -ResX=1920 -ResY=1080 "-ExecCmds=py <this file>" -SwimTag=before|after

Writes <tag>-<view>.png into SWIM_OUT (default C:/Projects/Shattered-Frontier/Artifacts/Evidence/swim-mixamo).
"""
import os, re, math
from pathlib import Path
import unreal as u

OUT = Path(os.environ.get('SWIM_OUT', 'C:/Projects/Shattered-Frontier/Artifacts/Evidence/swim-mixamo'))
OUT.mkdir(parents=True, exist_ok=True)
tag = (re.search(r'-SwimTag=(\w+)', u.SystemLibrary.get_command_line()) or [None, 'after'])[1]
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pc = u.GameplayStatics.get_player_controller(world, 0)
pawn = u.GameplayStatics.get_player_character(world, 0)
all_of = lambda cls: u.GameplayStatics.get_all_actors_of_class(world, cls)
camera = next(a for a in all_of(u.CameraActor) if a.get_actor_label() == 'ReviewCamera_Aerial')
pond = next(a for a in all_of(u.PhysicsVolume) if a.get_actor_label() == 'EastwoodPond_Swim')
V = u.Vector
o, e = pond.get_actor_bounds(False)
top = o.z + e.z
state = {'f': 0}


def aim(eye, look, fov):
    x, y, z = eye; tx, ty, tz = look
    yaw = math.degrees(math.atan2(ty - y, tx - x)); pitch = math.degrees(math.atan2(tz - z, math.hypot(tx - x, ty - y)))
    camera.set_actor_location(V(x, y, z), False, False); camera.set_actor_rotation(u.Rotator(pitch=pitch, yaw=yaw), False)
    camera.camera_component.set_field_of_view(fov); pc.set_view_target_with_blend(camera, 0)


def shot(name):
    u.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=' + str(OUT / f'{tag}-{name}.png').replace('\', '/'))
    u.log(f'SWIM_CAPTURE {tag}-{name} z={pawn.get_actor_location().z:.0f} surface={top:.0f} speed={pawn.get_velocity().length():.0f}')


for c in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0', 'r.MotionBlurQuality 0', 'r.ScreenPercentage 100'):
    u.SystemLibrary.execute_console_command(world, c)
pawn.character_movement.set_movement_mode(u.MovementMode.MOVE_WALKING)
pawn.set_actor_location(V(o.x - 600, o.y, top - 40), False, False)
pc.set_view_target_with_blend(pawn, 0)


def tick(_dt):
    state['f'] += 1; f = state['f']
    if f == 5: pawn.character_movement.set_movement_mode(u.MovementMode.MOVE_SWIMMING)
    if 20 < f <= 300:
        pawn.add_movement_input(V(1, 0, 0), 1.0)           # swim east
        pc.set_control_rotation(u.Rotator(pitch=0, yaw=0))
    if f in (230, 330, 500):
        l = pawn.get_actor_location()
        if f == 230:   aim((l.x + 20, l.y - 420, top + 40), (l.x, l.y, top - 20), 55)     # side
        elif f == 330: aim((l.x + 420, l.y + 20, top + 40), (l.x, l.y, top - 20), 55)     # front, pawn now still: treading
        elif f == 500: aim((l.x + 20, l.y - 420, top + 40), (l.x, l.y, top - 20), 55)     # tread side
    if f == 240: shot('swim-side')
    if f == 280: aim((pawn.get_actor_location().x + 420, pawn.get_actor_location().y + 20, top + 40), (pawn.get_actor_location().x, pawn.get_actor_location().y, top - 20), 55)
    if f == 290: shot('swim-front')
    if f == 520: shot('tread-side')
    if f == 560: shot('tread-front')
    if f == 550: l = pawn.get_actor_location(); aim((l.x + 420, l.y + 20, top + 40), (l.x, l.y, top - 20), 55)
    if f == 600:
        u.unregister_slate_post_tick_callback(state['h']); u.SystemLibrary.execute_console_command(world, 'quit')


state['h'] = u.register_slate_post_tick_callback(tick)
