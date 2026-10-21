"""Diagnose the player shadow trail (stale VSM pages) on a network client, in the owner's setup.

Server: UnrealEditor.exe <uproject> "/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook?game=/Script/ShatteredFrontier.SFGameMode"
        -server -port=<p> -multihome=127.0.0.1 -ConstructionNetworkSandbox -GatheringPilot
Client: UnrealEditor.exe <uproject> 127.0.0.1:<p> -game -windowed -ResX=1920 -ResY=1080 -ExchangeUiDemo
        "-ExecCmds=py <this file> <evidence folder>"
Lists every shadow-casting primitive within 6 m of the local pawn and every ASFE1Character in the client world,
then twice walks the pawn forward for about 6 s and looks back along the path: once with the VSM cache on
(A, the default) and once with r.Shadow.Virtual.Cache 0 (B). A trail in A but not in B is a cache-invalidation
miss. Writes shadow-trail.json and captures into the evidence folder.
"""
import json, math, sys
from pathlib import Path
import unreal as u

OUT = Path(sys.argv[1]); OUT.mkdir(parents=True, exist_ok=True)
state = dict(casters=[], characters=[], passes=[])
tick = dict(frame=0, start=None, yaw=0.0)


def survey(w, pawn):
    here = pawn.get_actor_location()
    for actor in u.GameplayStatics.get_all_actors_of_class(w, u.Actor):
        for c in actor.get_components_by_class(u.PrimitiveComponent):
            try:
                if not c.get_editor_property('cast_shadow') or c.get_world_location().distance(here) > 600:
                    continue
                state['casters'].append(dict(
                    actor=actor.get_name(), actor_class=actor.get_class().get_name(), component=c.get_name(),
                    component_class=c.get_class().get_name(), visible=bool(c.is_visible()),
                    hidden_in_game=bool(c.get_editor_property('hidden_in_game')),
                    cast_hidden_shadow=bool(c.get_editor_property('cast_hidden_shadow')),
                    owner_no_see=bool(c.get_editor_property('owner_no_see')),
                    mobility=str(c.get_editor_property('mobility')),
                    shadow_cache_invalidation=str(c.get_editor_property('shadow_cache_invalidation_behavior'))))
            except Exception as error:
                state['casters'].append(dict(actor=actor.get_name(), component=c.get_name(), error=repr(error)))
    for ch in u.GameplayStatics.get_all_actors_of_class(w, u.Character):
        state['characters'].append(dict(name=ch.get_name(), cls=ch.get_class().get_name(),
                                        role=str(ch.get_editor_property('role')), local=ch == pawn,
                                        location_cm=[round(v) for v in ch.get_actor_location().to_tuple()]))


# Frame plan per pass (relative): 0 camera turns to face the pawn from ahead, 1..600 run toward the camera (about 40 m), 400 capture mid-run.
PASSES = [("A-default", "r.Shadow.Virtual.Cache 1"), ("B-vsm-cache-off", "r.Shadow.Virtual.Cache 0"),
          ("C-lumen-gi-off", "r.Shadow.Virtual.Cache 1 | r.DynamicGlobalIlluminationMethod 0")]


def find_world():
    # The client connects after ExecCmds runs, so look for the loaded map with a local controller each tick.
    for candidate in u.ObjectIterator(u.World):
        if candidate.get_name() == "Greenlands_Silverbrook" and u.GameplayStatics.get_player_controller(candidate, 0):
            return candidate
    return None


def step(delta):
    if tick["world"] is None:
        tick["world"] = find_world()
        if tick["world"] is None:
            return
    pc = u.GameplayStatics.get_player_controller(tick['world'], 0)
    pawn = pc.get_controlled_pawn() if pc else None
    if not pawn:
        return
    if tick['start'] is None:
        tick['start'] = tick['frame'] + 120  # let the map settle after possession
        return
    f = tick["frame"] - tick["start"]
    if f < 0:
        return
    if f == 0:
        survey(tick['world'], pawn)
        tick['yaw'] = pc.get_control_rotation().yaw
    index, local = divmod(f, 760)
    if index >= len(PASSES):
        (OUT / 'shadow-trail.json').write_text(json.dumps(state, indent=2))
        u.log('SHADOW_TRAIL ' + json.dumps(dict(casters=len(state['casters']), characters=len(state['characters']), passes=state['passes'])))
        u.unregister_slate_post_tick_callback(tick['handle'])
        u.SystemLibrary.execute_console_command(tick['world'], 'quit')
        return
    name, cvar = PASSES[index]
    if local == 0:
        for command in cvar.split(" | "):
            u.SystemLibrary.execute_console_command(tick["world"], command)
        # Camera looks back along the direction of travel, so the path the pawn just crossed stays in view while it runs.
        pc.set_control_rotation(u.Rotator(pitch=-35, yaw=tick["yaw"] + 180, roll=0))
    if 1 <= local <= 600:
        pawn.add_movement_input(u.Vector(math.cos(math.radians(tick['yaw'])), math.sin(math.radians(tick['yaw'])), 0), 1.0, False)
    if local == 400:  # mid-run: temporal artifacts (Lumen, TSR) fade once the pawn stops
        shot = f'{name}.png'
        u.SystemLibrary.execute_console_command(tick['world'], 'HighResShot 1920x1080 filename=' + str(OUT / shot).replace('\\', '/'))
        state['passes'].append(dict(name=name, cvar=cvar, shot=shot, pawn_cm=[round(v) for v in pawn.get_actor_location().to_tuple()]))


def on_tick(delta):
    tick['frame'] += 1
    try:
        step(delta)
    except Exception as error:  # never leave the client running on a script error
        u.log_error(f'SHADOW_TRAIL error {error!r}')
        u.unregister_slate_post_tick_callback(tick['handle'])
        u.SystemLibrary.execute_console_command(tick['world'], 'quit')


tick["world"] = None
tick['handle'] = u.register_slate_post_tick_callback(on_tick)
