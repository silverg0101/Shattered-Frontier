"""Walk Silverbrook's loop road with the project pawn in a real -game session.

Launch: UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook
        -game -RenderOffscreen -ExecutePythonScript=<this file>
Writes silverbrook-game-review.json beside this script and one arrival capture.
"""
import json, math, sys, time
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from region_layout import AREAS, ROUTE, height

world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pawn = u.GameplayStatics.get_player_character(world, 0)
mode = u.GameplayStatics.get_game_mode(world)
fixtures = u.GameplayStatics.get_all_actors_of_class(world, u.load_class(None, '/Script/ShatteredFrontier.SFE1InteractionTarget'))
state = dict(scope='Native project pawn walks the 956 m loop; no AI, combat, gathering or performance acceptance',
             map=world.get_name(), game_mode=mode.get_class().get_name() if mode else None,
             pawn=pawn.get_class().get_name() if pawn else None, e1_fixtures=len(fixtures),
             frame=0, target=1, reached=[], areas_visited=[])
started = time.monotonic(); walk_started = None; handle = None; captured = False

def finish():
    state['elapsed_seconds'] = round(time.monotonic() - started, 1)
    state['passed'] = (state.get('walk_completed') is True and state['e1_fixtures'] == 0
                       and state['game_mode'] == 'SFE1GameMode' and set(state['areas_visited']) == set(AREAS))
    (ROOT / 'silverbrook-game-review.json').write_text(json.dumps(state, indent=2))
    u.log(f"SILVERBROOK_REVIEW passed={state['passed']}")
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.execute_console_command(world, 'quit')

def tick(delta):
    global walk_started, captured
    state['frame'] += 1
    if state['frame'] < 60:
        return
    try:
        now = time.monotonic()
        if walk_started is None:
            walk_started = now
            u.SystemLibrary.execute_console_command(world, 'HighResShot 1280x720 filename=' + str(ROOT / 'Silverbrook-Arrival-InGame.png').replace('\\', '/'))
        p = pawn.get_actor_location()
        for name, area in AREAS.items():
            if name not in state['areas_visited'] and math.dist((p.x / 100, p.y / 100), area['center']) <= area['radius'] + 6:
                state['areas_visited'].append(name)
        x, y = ROUTE[state['target']]
        dx, dy = x * 100 - p.x, y * 100 - p.y; d = math.hypot(dx, dy)
        if d < 150:
            state['reached'].append(dict(xy_metres=[x, y], height_above_terrain_metres=round(p.z / 100 - height(p.x / 100, p.y / 100), 2),
                                         seconds=round(now - walk_started, 1)))
            state['target'] += 1
        else:
            pawn.add_movement_input(u.Vector(dx / d, dy / d, 0), 1, False)
        if state['target'] == len(ROUTE) or now - walk_started > 420:
            state['walk_completed'] = state['target'] == len(ROUTE)
            state['final_pawn_cm'] = [round(p.x), round(p.y), round(p.z)]
            finish()
    except Exception as error:
        state['error'] = str(error); finish()

handle = u.register_slate_post_tick_callback(tick)
