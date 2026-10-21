"""Reproduce and bisect the player shadow trail in a -game session on a flat map, with timed captures.

Launch (no editor needed; on Greenlands_Silverbrook the pawn starts in the open meadow at WP8 bookmark 01):
    UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands_ConstructionSandbox -game -windowed -ResX=1920 -ResY=1080
        "-ExecCmds=py <this file> <evidence folder>"
Zooms the CTL-01 camera out to its far limit, pitches it steeply down (the owner's top-down view), then for each
variant walks about 20 m on a fresh lane under real movement input and captures where the pawn walked. Variants
change one shadow cvar each; the variant whose capture has no trail names the system that leaves it. Also lists
every primitive component on the pawn (hidden or not) with its shadow and mobility flags. Writes trail-repro.json.
"""
import json, math, sys
from pathlib import Path
import unreal as u

OUT = Path(sys.argv[1]); OUT.mkdir(parents=True, exist_ok=True)
VARIANTS = [
    ('0-baseline', []),
    ('1-vsm-cache-off', ['r.Shadow.Virtual.Cache 0']),
    ('2-vsm-off', ['r.Shadow.Virtual.Cache 1', 'r.Shadow.Virtual.Enable 0']),
    ('3-df-shadows-off', ['r.Shadow.Virtual.Enable 1', 'r.DistanceFieldShadowing 0']),
    ('4-contact-shadows-off', ['r.DistanceFieldShadowing 1', 'r.ContactShadows 0']),
    ('5-capsule-shadows-off', ['r.ContactShadows 1', 'r.CapsuleShadows 0']),
]
WALK_S, SETTLE_S = 2.8, 0.6  # about 20 m at the 7 m/s run speed, then let the shadow settle before the shot
state = dict(pawn_primitives=[], variants=[])
run = dict(world=None, t=0.0, phase='wait', index=0, phase_t=0.0, yaw=0.0)


def find_world():
    for w in u.ObjectIterator(u.World):
        if u.GameplayStatics.get_player_controller(w, 0) and u.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn():
            return w
    return None


def survey(pawn):
    actors = [pawn] + list(pawn.get_attached_actors())
    for actor in actors:
        for c in actor.get_components_by_class(u.PrimitiveComponent):
            row = dict(actor=actor.get_name(), component=c.get_name(), cls=c.get_class().get_name())
            for prop in ('visible', 'hidden_in_game', 'cast_shadow', 'cast_hidden_shadow', 'cast_dynamic_shadow', 'cast_static_shadow',
                         'owner_no_see', 'only_owner_see', 'mobility', 'shadow_cache_invalidation_behavior', 'cast_contact_shadow',
                         'cast_capsule_direct_shadow', 'cast_capsule_indirect_shadow'):
                try:
                    row[prop] = str(c.get_editor_property(prop))
                except Exception:
                    pass
            parent = c.get_attach_parent()
            row['attach_parent'] = parent.get_name() if parent else None
            state['pawn_primitives'].append(row)


def command(text):
    u.SystemLibrary.execute_console_command(run['world'], text)


def finish():
    (OUT / 'trail-repro.json').write_text(json.dumps(state, indent=2))
    u.log('TRAIL_REPRO ' + json.dumps(dict(primitives=len(state['pawn_primitives']), variants=[v['name'] for v in state['variants']])))
    u.unregister_slate_post_tick_callback(run['handle'])
    command('quit')


def step(delta):
    run['t'] += delta; run['phase_t'] += delta
    if run['world'] is None:
        run['world'] = find_world()
        return
    pc = u.GameplayStatics.get_player_controller(run['world'], 0)
    pawn = pc.get_controlled_pawn()
    inputs = next(s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith('Default__'))
    ia = lambda n: u.load_asset(f'/Game/Frontier/Input/IA_{n}')
    if run['phase'] == 'wait' and run['phase_t'] > 3.0:
        survey(pawn)
        run["yaw"] = pawn.get_actor_rotation().yaw
        if run["world"].get_name() == "Greenlands_Silverbrook":  # open meadow at WP8 bookmark 01, facing the town
            view = json.loads((Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / "ArtSource/Greenlands/silverbrook-reference-bookmarks.json").read_text(encoding="utf-8"))["views"][0]
            (x, y), (tx, ty) = view["xy_m"], view["look_xy_m"]
            hit = u.SystemLibrary.line_trace_single(run["world"], u.Vector(x * 100, y * 100, 100000), u.Vector(x * 100, y * 100, -100000),
                                                     u.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn], u.DrawDebugTrace.NONE, True)
            pawn.set_actor_location(u.Vector(x * 100, y * 100, (hit.to_tuple()[4].z if hit else 0) + 100), False, True)
            run["yaw"] = math.degrees(math.atan2(ty - y, tx - x))
        command('ShowFlag.TextRender 0')
        run['phase'], run['phase_t'] = 'zoom', 0.0
    elif run['phase'] == 'zoom':
        inputs.inject_input_vector_for_action(ia('Zoom'), u.Vector(-1.0, 0.0, 0.0), [], [])
        if run['phase_t'] > 1.5:
            run['phase'], run['phase_t'] = 'setup', 0.0
    elif run['phase'] == 'setup':
        name, cvars = VARIANTS[run['index']]
        for c in cvars:
            command(c)
        # Staircase path (alternating 0 and 90 degrees): each variant walks a fresh segment, so earlier trails stay out of it.
        lane_yaw = run["yaw"] + (90 if run["index"] % 2 else 0)
        pc.set_control_rotation(u.Rotator(pitch=-70, yaw=lane_yaw, roll=0))
        run['phase'], run['phase_t'], run['lane_yaw'] = 'walk', 0.0, lane_yaw
    elif run['phase'] == 'walk':
        inputs.inject_input_vector_for_action(ia('MoveForward'), u.Vector(1.0, 0.0, 0.0), [], [])
        if run['phase_t'] > WALK_S:
            run['phase'], run['phase_t'] = 'settle', 0.0
    elif run['phase'] == 'settle' and run['phase_t'] > SETTLE_S:
        name, cvars = VARIANTS[run['index']]
        shot = f'{name}.png'
        command('HighResShot 1920x1080 filename=' + str(OUT / shot).replace('\\', '/'))
        state['variants'].append(dict(name=name, cvars=cvars, shot=shot, pawn_cm=[round(v) for v in pawn.get_actor_location().to_tuple()]))
        run['phase'], run['phase_t'] = 'shot', 0.0
    elif run['phase'] == 'shot' and run['phase_t'] > 0.5:
        run['index'] += 1
        if run['index'] >= len(VARIANTS):
            finish(); return
        run['phase'], run['phase_t'] = 'setup', 0.0


def on_tick(delta):
    try:
        step(delta)
    except Exception as error:  # never leave a -game session running on a script error
        u.log_error(f'TRAIL_REPRO error {error!r}')
        (OUT / 'trail-repro.json').write_text(json.dumps(state, indent=2))
        u.unregister_slate_post_tick_callback(run['handle'])
        command('quit')


run['handle'] = u.register_slate_post_tick_callback(on_tick)
