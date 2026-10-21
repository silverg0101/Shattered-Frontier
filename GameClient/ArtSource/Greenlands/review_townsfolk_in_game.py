"""Watch the townsfolk prototype (AI Townsfolk doc, section 9 steps 2-3) over the Market Square, at the north end of South Street.

Launch in -game (residents are on by USFTownsfolkSettings::bEnabled) with a fast prototype clock, for example:
  UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game
      "-ini:Game:[/Script/ShatteredFrontier.SFTownsfolkSettings]:GameDayMinutes=6" -ExecCmds="py <this file>"
Captures Townsfolk-<hour>h.png at the game hours below, then quits. The TOWNSFOLK_HOUR log lines say where everyone
is meant to be each hour, who is outdoors, pairs closer than 0.6 m summed over frames (ClosePairFrames), route counts and the processor's cost.
Add "-ini:Game:[/Script/ShatteredFrontier.SFTownsfolkSettings]:SidestepMaxMetres=0" to compare without sidesteps.
Real walking speed (1x): GameDayMinutes=60 with StartHour=7, and set the environment variables TOWNSFOLK_DAY_MINUTES=60,
TOWNSFOLK_START_HOUR=7 and TOWNSFOLK_HOURS=8,9,10,11 so the captures follow that clock (about 11 minutes).
"""
import os, time
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parent
DAY_MINUTES = float(os.environ.get('TOWNSFOLK_DAY_MINUTES', 6.0))   # must match the -ini overrides GameDayMinutes and StartHour
START_HOUR = float(os.environ.get('TOWNSFOLK_START_HOUR', 6.0))
# leaving for work and errands, at the market, walking home, night
HOURS = [float(h) for h in os.environ.get('TOWNSFOLK_HOURS', '7.5,8.5,10.0,11.5,23.0').split(',')]
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pc = u.GameplayStatics.get_player_controller(world, 0)
state = dict(frame=0, started=None, shots=0)


def tick(delta):
    state['frame'] += 1
    if state['frame'] < 120:
        return
    now = time.monotonic()
    if state['started'] is None:
        state['started'] = now
        # Over the Market Square from the south-east, where errand-runners stand at the stalls; the bridge camera, moved.
        cam = next(c for c in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor) if c.get_actor_label() == 'ReviewCamera_Bridge')
        hit = u.SystemLibrary.line_trace_single(world, u.Vector(4500, 2500, 50000), u.Vector(4500, 2500, -50000), u.TraceTypeQuery.ECC_VISIBILITY,
                                                False, [], u.DrawDebugTrace.NONE, True)
        z = hit.to_tuple()[4].z if hit else 2000
        cam.set_actor_location_and_rotation(u.Vector(4500, 2500, z + 2500), u.Rotator(roll=0, pitch=-35, yaw=-120), False, True)
        cam.camera_component.set_editor_property('field_of_view', 80)
        pc.set_view_target_with_blend(cam, 0)
        state['cam'] = cam
        return
    hour = START_HOUR + (now - state['started']) / (DAY_MINUTES * 60 / 24)
    cam = state.get('cam')
    if state['shots'] < len(HOURS) and hour >= HOURS[state['shots']]:
        name = f"Townsfolk-{HOURS[state['shots']]:04.1f}h.png".replace('.', '_', 1)
        # Frame the outdoor residents nearest the square from 9 m away, when anyone is out.
        out = u.SFTownsfolkLaneLibrary.outdoor_residents(world)
        if out:
            l = min(out, key=lambda p: p.length())
            cam.set_actor_location_and_rotation(u.Vector(l.x + 600, l.y + 600, l.z + 400), u.Rotator(roll=0, pitch=-20, yaw=-135), False, True)
        u.log(f'TOWNSFOLK_REVIEW {len(out)} residents outdoors')
        u.SystemLibrary.execute_console_command(world, 'HighResShot 1280x720 filename=' + str(ROOT / name).replace('\\', '/'))
        u.log(f'TOWNSFOLK_REVIEW capture {name} at game hour {hour:.2f}')
        state['shots'] += 1
    elif state['shots'] == len(HOURS) and hour >= HOURS[-1] + 0.5:
        u.unregister_slate_post_tick_callback(handle)
        u.SystemLibrary.execute_console_command(world, 'quit')


handle = u.register_slate_post_tick_callback(tick)
