"""GFX-01 WP7 / CTL-01: player character and camera controls at the WP8 forest bookmark, in a -game session.

Launch (no editor open):
    UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -RenderOffscreen
        -ResX=2560 -ResY=1440 "-ExecCmds=py <this file> <evidence folder>"
Puts the pawn on bookmark 01-forest-path-town facing its look point, then drives IMC_Player actions through
EnhancedInput injection (the same polled values a mouse produces): RMB mouselook must turn pawn and camera, LMB
orbit must turn only the camera, wheel in must reach first person with the body hidden, wheel out must reach the far
zoom. Captures default, walking, near (minimum third-person) and far zoom, wraps the walk in an FPS chart, and writes
player-review.json into the evidence folder.
"""
import json, math, sys
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT.parents[1] / 'docs/evidence/graphics/wp7-player'
OUT.mkdir(parents=True, exist_ok=True)
view = json.loads((ROOT / 'Greenlands/silverbrook-reference-bookmarks.json').read_text(encoding='utf-8'))['views'][0]
IA = {n: u.load_asset(f'/Game/Frontier/Input/IA_{n}') for n in ('MouseLook', 'Orbit', 'Look', 'Zoom', 'MoveForward', 'StrafeRight', 'Interact')}
x, y = view['xy_m']; tx, ty = view['look_xy_m']
yaw = math.degrees(math.atan2(ty - y, tx - x))
tick = dict(frame=0, mark={})
world = pc = pawn = mesh = boom = inputs = state = None
build = network = False


def setup():
    """Standalone or network client: wait for the map and the possessed pawn (a client connects after ExecCmds)."""
    global world, pc, pawn, mesh, boom, inputs, state, build, network
    world = next((w for w in u.ObjectIterator(u.World) if w.get_name() == 'Greenlands_Silverbrook'
                  and u.GameplayStatics.get_player_controller(w, 0)), None)
    pc = u.GameplayStatics.get_player_controller(world, 0) if world else None
    pawn = pc.get_controlled_pawn() if pc else None
    if not pawn:
        return False
    mesh = pawn.mesh
    boom = pawn.get_component_by_class(u.SpringArmComponent)
    # The subsystem getter is BlueprintInternalUseOnly (not in Python), so find the live instance.
    inputs = next(s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith("Default__"))
    # build mode (IMC_Build owns LMB/E/F): sandbox controller, or a replicated construction state on the network controller
    build = pc.get_class().get_name() == 'SFConstructionSandboxController' or bool(u.GameplayStatics.get_all_actors_of_class(world, u.load_class(None, '/Script/ShatteredFrontier.SFConstructionNetworkState')))
    network = str(pawn.get_editor_property('role')).endswith('AUTONOMOUS_PROXY: 2>')
    state = dict(pawn=pawn.get_class().get_name(), controller=pc.get_class().get_name(), build_mode=build, network_client=network,
                 mesh=mesh.skeletal_mesh_asset.get_path_name() if mesh.skeletal_mesh_asset else None,
                 anim_class=mesh.get_anim_instance().get_class().get_name() if mesh.get_anim_instance() else None,
                 shadow_cache_invalidation=str(mesh.get_editor_property('shadow_cache_invalidation_behavior')),
                 bookmark=view['name'], checks={}, shots=[], frames_ms=[])
    return True


def inject(name, value):
    v = value if isinstance(value, tuple) else (value, 0.0)
    inputs.inject_input_vector_for_action(IA[name], u.Vector(float(v[0]), float(v[1]), 0.0), [], [])


def yaws():
    return pawn.get_actor_rotation().yaw, pc.player_camera_manager.get_camera_rotation().yaw


def turned(a, b):
    return abs((b - a + 180) % 360 - 180)


def shot(name):
    u.SystemLibrary.execute_console_command(world, 'HighResShot 2560x1440 filename=' + str(OUT / name).replace('\\', '/'))
    state['shots'].append(dict(name=name, arm_cm=round(boom.target_arm_length), speed_cms=round(pawn.get_velocity().length())))
    u.log('WP7_CAPTURE ' + name)


# Frame windows: (first, last, what). Held buttons stay down for 30+ frames, past ClickMaxSeconds, so they drag.
def on_tick(delta):
    try:
        step(delta)
    except Exception as error:  # never leave a -game session running on a script error
        u.log_error(f"WP7_REVIEW error {error!r}")
        u.unregister_slate_post_tick_callback(tick["handle"])
        u.SystemLibrary.execute_console_command(world, "quit")


def step(delta):
    if state is None:
        setup()
        return
    tick["frame"] += 1
    f = tick['frame']; m = tick['mark']
    if f == 90:
        hit = u.SystemLibrary.line_trace_single(world, u.Vector(x * 100, y * 100, 100000), u.Vector(x * 100, y * 100, -100000),
                                                 u.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn], u.DrawDebugTrace.NONE, True)
        z = hit.to_tuple()[4].z if hit else 0
        if not network:  # a client teleport would be corrected by the server; it walks from its spawn instead
            pawn.set_actor_location_and_rotation(u.Vector(x * 100, y * 100, z + 100), u.Rotator(pitch=0, yaw=yaw, roll=0), False, True)
        pc.set_control_rotation(u.Rotator(pitch=0, yaw=yaw, roll=0))
        for c in ('r.MotionBlurQuality 0', 'r.ScreenPercentage 100', 'ShowFlag.TextRender 0'):
            u.SystemLibrary.execute_console_command(world, c)
    if f == 160: shot('01-default-zoom.png')
    # Walk forward (locomotion) under an FPS chart.
    if 180 <= f < 300:
        inject('MoveForward', 1.0); state['frames_ms'].append(round(delta * 1000, 2))
    if f == 180: u.SystemLibrary.execute_console_command(world, 'StartFPSChart')
    if f == 260: shot('02-walking.png')
    if f == 299: u.SystemLibrary.execute_console_command(world, 'StopFPSChart')
    # Shadow-trail check: orbit 180 degrees and look back along the walked path (stale VSM pages would show there).
    if 300 <= f < 360:
        inject("MouseLook" if build else "Orbit", 1.0)
        if f >= 330: inject("Look", (24.0, -1.0))
    if f == 364: shot("02b-path-behind.png")
    # RMB mouselook: pawn and camera turn together.
    if f == 390: m['rmb'] = yaws()
    if 390 <= f < 440:
        inject('MouseLook', 1.0)
        if f >= 410: inject('Look', (15.0, 0.0))
    if f == 450:
        (p0, c0), (p1, c1) = m['rmb'], yaws()
        state['checks']['rmb_mouselook'] = dict(pawn_turn=round(turned(p0, p1), 1), camera_turn=round(turned(c0, c1), 1),
                                                camera_vs_pawn=round(turned(p1, c1), 1),
                                                passed=turned(p0, p1) > 30 and turned(p1, c1) < 5)
    # LMB orbit: only the camera turns.
    if f == 460: m['lmb'] = yaws()
    if 460 <= f < 510:
        inject('Orbit', 1.0)
        if f >= 480: inject('Look', (15.0, 0.0))
    if f == 455 and build:  # injection bypasses key mappings, so ask which keys still reach the player actions
        keys = {n: sorted(k.export_text() for k in inputs.query_keys_mapped_to_action(IA[n])) for n in ('Orbit', 'StrafeRight', 'Interact')}
        state['checks']['build_keys_reserved'] = dict(keys=keys, passed=not any('LeftMouseButton' in k for k in keys['Orbit'])
                                                      and 'E' not in keys['StrafeRight'] and 'F' not in keys['Interact'])
    if f == 520:
        (p0, c0), (p1, c1) = m['lmb'], yaws()
        state['checks']['lmb_orbit'] = dict(pawn_turn=round(turned(p0, p1), 1), camera_turn=round(turned(c0, c1), 1),
                                            # build mode: injection skips key mappings, so the LMB reservation is checked by build_keys_reserved
                                            passed=turned(p0, p1) < 1 and turned(c0, c1) > 30)
    # Wheel in to first person, one notch out to the nearest third-person zoom, then all the way out.
    if 530 <= f < 560: inject('Zoom', 1.0)
    if f == 620:
        state['checks']['first_person'] = dict(arm_cm=round(boom.target_arm_length, 1), body_hidden=bool(mesh.get_editor_property('owner_no_see')),
                                               passed=boom.target_arm_length < 10 and bool(mesh.get_editor_property('owner_no_see')))
        shot('03-first-person.png')
    if f == 640: inject('Zoom', -1.0)
    if f == 720: shot('04-near-zoom.png')
    if 740 <= f < 780: inject('Zoom', -1.0)
    if f == 880:
        state['checks']['far_zoom'] = dict(arm_cm=round(boom.target_arm_length, 1), passed=boom.target_arm_length > 1000)
        shot('05-far-zoom.png')
    if f == 920:
        ms = sorted(state['frames_ms'])
        state['frame_ms_mean'] = round(sum(ms) / len(ms), 2) if ms else None
        state['frame_ms_p95'] = ms[int(len(ms) * 0.95)] if ms else None
        state['walk_speed_ok'] = state['shots'][1]['speed_cms'] > 100
        state['passed'] = bool(state['mesh'] and state['anim_class'] and state['walk_speed_ok']
                               and all(c['passed'] for c in state['checks'].values()) and len(state["checks"]) == (5 if build else 4))
        (OUT / 'player-review.json').write_text(json.dumps(state, indent=2))
        u.log('WP7_REVIEW ' + json.dumps(dict(passed=state['passed'], checks={k: v['passed'] for k, v in state['checks'].items()},
                                              frame_ms_mean=state['frame_ms_mean'], frame_ms_p95=state['frame_ms_p95'])))
        u.unregister_slate_post_tick_callback(tick['handle'])
        u.SystemLibrary.execute_console_command(world, 'quit')


tick['handle'] = u.register_slate_post_tick_callback(on_tick)
