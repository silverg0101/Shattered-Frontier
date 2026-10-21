"""1440p paired stat gpu samples in a separate -game process; no map writes."""
import math, shutil, time
from pathlib import Path
import unreal as u

OUT = Path(__file__).resolve().parent
SHOTS = Path('C:/Projects/Shattered-Frontier/GameClient/Saved/Screenshots/WindowsEditor')
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pc = u.GameplayStatics.get_player_controller(world, 0)
camera = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor)
              if a.get_actor_label() == 'ReviewCamera_Aerial')
sun = u.GameplayStatics.get_all_actors_of_class(world, u.DirectionalLight)[0]
sky = u.GameplayStatics.get_all_actors_of_class(world, u.SkyLight)[0]
lights = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.PointLight)
          if a.get_actor_label().startswith('FX_StreetLamp_')]
cards = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.Actor)
             if a.get_actor_label() == 'FX_StreetLamp_Cards')
glass = next(a.get_actor_location() for a in lights
             if a.get_actor_label() == 'FX_StreetLamp_Light_StreetLamp_002_0')
eye = u.Vector(glass.x - 300, glass.y + 650, glass.z - 130)
look = u.Vector(glass.x - 200, glass.y - 1400, glass.z - 180)
d = look - eye
camera.set_actor_location(eye, False, False)
camera.set_actor_rotation(u.Rotator(pitch=math.degrees(math.atan2(d.z, math.hypot(d.x,d.y))),
                                    yaw=math.degrees(math.atan2(d.y,d.x))), False)
camera.camera_component.set_field_of_view(66)
pc.set_view_target_with_blend(camera, 0)
sun.set_actor_rotation(u.Rotator(pitch=-5, yaw=-60), False)
sun.light_component.set_editor_property('intensity', 2.0)
sky.light_component.set_editor_property('intensity', 1.0)
for cmd in ('r.MotionBlurQuality 0','r.ScreenPercentage 100','stat gpu'):
    u.SystemLibrary.execute_console_command(world, cmd)

def enabled(value):
    for a in lights:
        a.light_component.set_visibility(value)
    for c in cards.get_components_by_class(u.HierarchicalInstancedStaticMeshComponent):
        c.set_visibility(value)

phases = [('on-1',True), ('off-1',False), ('on-2',True), ('off-2',False)]
state = {'frame':0,'index':0,'start':None,'pending':None,'before':set()}

def tick(_delta):
    state['frame'] += 1
    if state['frame'] < 90:return
    now = time.monotonic()
    if state['pending'] is not None:
        new = sorted((p for p in SHOTS.glob('*.png') if p.name not in state['before']),
                     key=lambda p:p.stat().st_mtime)
        if new:
            shutil.copy2(new[-1], OUT / ('gpu-' + state['pending'] + '.png'))
            state['pending'] = None
            state['index'] += 1
            state['start'] = None
        elif now - state['start'] > 6:
            raise RuntimeError('Shot did not write a screenshot')
        return
    if state['index'] == len(phases):
        u.unregister_slate_post_tick_callback(state['handle'])
        u.SystemLibrary.execute_console_command(world, 'quit')
        return
    name, value = phases[state['index']]
    if state['start'] is None:
        enabled(value)
        state['start'] = now
    elif now - state['start'] > 4:
        state['before'] = set(p.name for p in SHOTS.glob('*.png'))
        u.log('LAMPFX3_GPU_SAMPLE ' + name)
        u.SystemLibrary.execute_console_command(world, 'profilegpu')
        u.SystemLibrary.execute_console_command(world, 'Shot')
        state['pending'] = name

state['handle'] = u.register_slate_post_tick_callback(tick)
