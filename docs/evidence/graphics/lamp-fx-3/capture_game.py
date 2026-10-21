"""Silverbrook lampfx3 game capture; launched only after the owning editor closes."""
import json, math, time
from pathlib import Path
import unreal as u

OUT = Path(__file__).resolve().parent
OUT.mkdir(parents=True, exist_ok=True)
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pc = u.GameplayStatics.get_player_controller(world, 0)
camera = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor)
              if a.get_actor_label() == 'ReviewCamera_Aerial')
lamp = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.Actor)
            if a.get_actor_label() == 'StreetLamp_002')
sun = u.GameplayStatics.get_all_actors_of_class(world, u.DirectionalLight)[0]
sky = u.GameplayStatics.get_all_actors_of_class(world, u.SkyLight)[0]
fx = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.Actor)
      if a.get_actor_label().startswith('FX_StreetLamp_')]
glass = next(a.get_actor_location() for a in fx if a.get_actor_label() == 'FX_StreetLamp_Light_StreetLamp_002_0')
pawn = u.GameplayStatics.get_player_character(world, 0)
if pawn:
    pawn.set_actor_hidden_in_game(True)
    pawn.character_movement.disable_movement()
for cmd in ('r.MotionBlurQuality 0', 'r.ScreenPercentage 100', 'r.EyeAdaptationQuality 2',
            'r.DefaultFeature.AutoExposure 1', 'ShowFlag.TextRender 0'):
    u.SystemLibrary.execute_console_command(world, cmd)

def pose(close):
    eye = (u.Vector(glass.x - 355, glass.y - 470, glass.z - 60) if close else
           u.Vector(glass.x - 300, glass.y + 650, glass.z - 130))
    look = (u.Vector(glass.x, glass.y, glass.z - 55) if close else
            u.Vector(glass.x - 200, glass.y - 1400, glass.z - 180))
    d = look - eye
    camera.set_actor_location(eye, False, False)
    camera.set_actor_rotation(u.Rotator(pitch=math.degrees(math.atan2(d.z, math.hypot(d.x,d.y))),
                                        yaw=math.degrees(math.atan2(d.y,d.x))), False)
    camera.camera_component.set_field_of_view(50 if close else 66)
    pc.set_view_target_with_blend(camera, 0)

def shot(name):
    u.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=' + (OUT / name).as_posix())
    u.log('LAMPFX3_SHOT ' + name)

plan = [
    ('day-close.png', -40, 10.0, 0.8, True),
    ('dusk-close.png', -5, 2.0, 1.0, True),
    ('dusk-street.png', -5, 2.0, 1.0, False),
    ('night-close.png', 10, 0.9, 1.3, True),
    ('night-street.png', 10, 0.9, 1.3, False),
]
state = {'frame': 0, 'index': 0, 'sequence': [], 'phase_start': None}

def tick(_delta):
    state['frame'] += 1
    if state['frame'] < 80: return
    now = time.monotonic()
    if state['index'] < len(plan):
        name, pitch, sun_intensity, sky_intensity, close = plan[state['index']]
        if state['phase_start'] is None:
            sun.set_actor_rotation(u.Rotator(pitch=pitch, yaw=-60, roll=0), False)
            sun.light_component.set_editor_property('intensity', sun_intensity)
            sky.light_component.set_editor_property('intensity', sky_intensity)
            pose(close)
            state['phase_start'] = now
        elif now - state['phase_start'] > 2.5:
            shot(name)
            state['index'] += 1
            state['phase_start'] = None
        return
    if state['phase_start'] is None:
        pose(True)
        state['phase_start'] = now + 2
        return
    if now < state['phase_start']: return
    if len(state['sequence']) < 5:
        target = state['phase_start'] + len(state['sequence']) * .2
        if now >= target:
            n = len(state['sequence'])
            shot(f'flicker-{n:02d}.png')
            state['sequence'].append(round(now, 3))
        return
    (OUT / 'capture-times.json').write_text(json.dumps(state['sequence'], indent=2))
    u.unregister_slate_post_tick_callback(state['handle'])
    u.SystemLibrary.execute_console_command(world, 'quit')

state['handle'] = u.register_slate_post_tick_callback(tick)
