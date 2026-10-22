"""GM-01 follow-up: every mapped action in a -game session on the build-mode dev map.

Launch (no editor open); tools/Test-BuildModeInput.ps1 does this and checks the log:
    UnrealEditor.exe <uproject> /Game/Frontier/Maps/Dev/BuildMode/BuildMode_Review -game -RenderOffscreen -ResX=1920 -ResY=1080
        "-ExecCmds=py <this file> <evidence folder>"
Injects each IMC_Player / IMC_Build action through EnhancedInput (the controller logs INPUT_ACTION <name> when a bound action
reaches its handler). Pawn-polled actions are checked by their effect (position, yaw, jump, zoom). A build action injected before
B must NOT fire (negative control). Key assignments are read back from the live subsystem. Writes build-mode-review.json.
"""
import json, sys
from pathlib import Path
import unreal as u

OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('docs/evidence/gm01-build-mode')
OUT.mkdir(parents=True, exist_ok=True)
EXPECT_KEYS = {  # action -> keys that must still be mapped (IMC_Player / IMC_Build)
    'MoveForward': ['W'], 'MoveBack': ['S'], 'StrafeLeft': ['Q'], 'StrafeRight': ['E'], 'TurnLeft': ['A'], 'TurnRight': ['D'],
    'Look': ['Mouse2D'], 'Zoom': ['MouseWheelAxis'], 'MouseLook': ['RightMouseButton'], 'Orbit': ['LeftMouseButton'], 'Jump': ['SpaceBar'],
    'Interact': ['F'], 'TargetNext': ['Tab'], 'BuildMode': ['B'], 'Inventory': ['I'], 'Skills': ['M'], 'EquipAxe': ['Five'],
    'EquipPickaxe': ['Six'], 'CycleRecipe': ['C'], 'ClaimPlot': ['K'], 'Escape': ['Escape'], 'Trade': ['F6'], 'Market': ['F7'],
    'BuildPlace': ['LeftMouseButton'], 'BuildDemolish': ['X', 'Delete'], 'BuildShape1': ['One'], 'BuildShape2': ['Two'],
    'BuildShape3': ['Three'], 'BuildShape4': ['Four'], 'BuildRotate': ['R'], 'BuildLine': ['L'], 'BuildPlane': ['P'],
    'BuildLayerUp': ['PageUp'], 'BuildLayerDown': ['PageDown'], 'BuildDoor': ['E', 'F']}
BUILD_NAMES = [n for n in ['BuildPlace', 'BuildDemolish', 'BuildShape1', 'BuildShape2', 'BuildShape3', 'BuildShape4', 'BuildRotate', 'BuildLine', 'BuildPlane', 'BuildLayerUp', 'BuildLayerDown', 'BuildDoor']]
IA = {n: u.load_asset(f'/Game/Frontier/Input/IA_{n}') for n in EXPECT_KEYS}
# (frame, action, frames held). Always-on controller actions first, then B, then the build actions.
ALWAYS = ['Inventory', 'Inventory', 'Skills', 'Skills', 'EquipAxe', 'EquipPickaxe', 'CycleRecipe', 'ClaimPlot', 'Trade', 'Trade',
          'Market', 'Market', 'TargetNext']
BUILD = ['BuildShape1', 'BuildShape2', 'BuildShape3', 'BuildShape4', 'BuildRotate', 'BuildLine', 'BuildLine', 'BuildPlane', 'BuildPlane',
         'BuildLayerUp', 'BuildLayerDown', 'BuildDemolish', 'BuildDemolish', 'BuildDoor', 'BuildPlace']
done = set()
tick = dict(frame=0, mark={}, handle=None)
world = pc = pawn = boom = inputs = None
state = dict(checks={}, shots=[], keys={})


def setup():
    global world, pc, pawn, boom, inputs
    world = next((w for w in u.ObjectIterator(u.World) if w.get_name() == 'BuildMode_Review' and u.GameplayStatics.get_player_controller(w, 0)), None)
    pc = u.GameplayStatics.get_player_controller(world, 0) if world else None
    pawn = pc.get_controlled_pawn() if pc else None
    if not pawn:
        return False
    boom = pawn.get_component_by_class(u.SpringArmComponent)
    inputs = next(s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith('Default__'))
    state['controller'] = pc.get_class().get_name(); state['pawn'] = pawn.get_class().get_name()
    return True


def inject(name, value=1.0):
    v = value if isinstance(value, tuple) else (value, 0.0)
    inputs.inject_input_vector_for_action(IA[name], u.Vector(float(v[0]), float(v[1]), 0.0), [], [])


def shot(name):
    u.SystemLibrary.execute_console_command(world, 'HighResShot 1920x1080 filename=' + str(OUT / name).replace('\\', '/'))
    state['shots'].append(name); u.log('BM_CAPTURE ' + name)


def pulse(frame, start, action, held=6):
    if start <= frame < start + held:
        inject(action)


def yaw(): return pc.player_camera_manager.get_camera_rotation().yaw


def step(delta):
    if pc is None:
        setup(); return
    tick['t'] = tick.get('t', 0.0) + delta; tick['frame'] = int(tick['t'] * 30)
    f = tick['frame']; m = tick['mark']
    at = lambda n: n <= f and not (n in done or done.add(n))
    if at(60):
        state['checks']['build_keys_inactive_before_B'] = all(not [k for k in inputs.query_keys_mapped_to_action(IA[n])] for n in BUILD_NAMES)
        for name, keys in ((n, k) for n, k in EXPECT_KEYS.items() if n not in BUILD_NAMES):
            have = sorted(k.export_text() for k in inputs.query_keys_mapped_to_action(IA[name]))
            state['keys'][name] = dict(have=have, ok=all(k in have for k in keys))
        state['checks']['key_assignments'] = all(v['ok'] for v in state['keys'].values())
        for c in ('r.MotionBlurQuality 0', 'ShowFlag.TextRender 0'):
            u.SystemLibrary.execute_console_command(world, c)
    # Pawn-polled actions, judged by effect.
    if at(100): m['pos'] = pawn.get_actor_location()
    if 100 <= f < 160: inject('MoveForward')
    if at(165):
        d = (pawn.get_actor_location() - m['pos']).length(); state['checks']['move_forward'] = d > 100; state['move_cm'] = round(d, 1); state['pawn_at'] = str(pawn.get_actor_location()); m['pos'] = pawn.get_actor_location()
    if 170 <= f < 200: inject('StrafeRight')
    if at(205): state['strafe_cm'] = round((pawn.get_actor_location() - m['pos']).length(), 1); state['checks']['strafe_right'] = state['strafe_cm'] > 50
    if at(210): m['z'] = pawn.get_actor_location().z
    if at(212): inject('Jump')
    if 213 <= f < 240: m['zmax'] = max(m.get('zmax', 0), pawn.get_actor_location().z)
    if at(240): state['checks']['jump'] = m['zmax'] - m['z'] > 20
    if at(250): m['yaw'] = yaw()
    if 250 <= f < 290:
        inject('MouseLook')
        if f >= 260: inject('Look', (15.0, 0.0))
    if at(295): state['yaw_turn'] = round(abs((yaw() - m['yaw'] + 180) % 360 - 180), 1); state['checks']['mouse_look_turns'] = state['yaw_turn'] > 20
    if at(300): m['arm'] = boom.target_arm_length
    if 300 <= f < 320: inject('Zoom', 1.0)
    if at(340): state['checks']['zoom_in'] = boom.target_arm_length < m['arm'] - 20; m['arm'] = boom.target_arm_length
    if 345 <= f < 365: inject('Zoom', -1.0)
    if at(400): state['checks']['zoom_out'] = boom.target_arm_length > m['arm'] + 20
    # Always-on controller actions: each reaches its handler once per pulse.
    for i, a in enumerate(ALWAYS): pulse(f, 420 + i * 8, a)
    pulse(f, 560, 'BuildMode')
    if at(600):
        keys_of = lambda n: sorted(k.export_text() for k in inputs.query_keys_mapped_to_action(IA[n]))
        for name in BUILD_NAMES:
            have = keys_of(name); state['keys'][name] = dict(have=have, ok=all(k in have for k in EXPECT_KEYS[name]))
        # IMC_Build consumes LMB from IMC_Player, so orbit must not see it in build mode.
        state['checks']['lmb_reserved_in_build'] = 'LeftMouseButton' not in keys_of('Orbit')
        state['checks']['build_keys_assigned'] = all(state['keys'][n]['ok'] for n in BUILD_NAMES)
        shot('01-build-mode.png')
    for i, a in enumerate(BUILD): pulse(f, 610 + i * 8, a)
    # Wheel zoom still works in build mode.
    if at(740): m['arm'] = boom.target_arm_length
    if 740 <= f < 770: inject('Zoom', 1.0)
    if at(800): state['arm_build_cm'] = [round(m['arm']), round(boom.target_arm_length)]; state['checks']['zoom_in_build_mode'] = boom.target_arm_length < m['arm'] - 20; shot('02-build-mode-zoomed-out.png')
    pulse(f, 820, 'Escape')
    pulse(f, 840, 'Escape')
    pulse(f, 860, 'BuildMode')
    pulse(f, 880, 'BuildMode')
    if at(920):
        state['passed'] = all(state['checks'].values())
        (OUT / 'build-mode-review.json').write_text(json.dumps(state, indent=2))
        u.log('BM_REVIEW ' + json.dumps(state['checks']))
        u.unregister_slate_post_tick_callback(tick['handle'])
        u.SystemLibrary.execute_console_command(world, 'quit')


def on_tick(delta):
    try:
        step(delta)
    except Exception as error:  # never leave a -game session running on a script error
        u.log_error(f'BM_REVIEW error {error!r}')
        u.unregister_slate_post_tick_callback(tick['handle'])
        u.SystemLibrary.execute_console_command(world or u.EditorLevelLibrary.get_editor_world(), 'quit')


tick['handle'] = u.register_slate_post_tick_callback(on_tick)
