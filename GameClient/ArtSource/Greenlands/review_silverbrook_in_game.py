"""Walk Silverbrook's main route with the project pawn in a real -game session.

Launch: UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook
        -game -RenderOffscreen -ExecutePythonScript=<this file>
Follows review_route in silverbrook-town-layout.json from the arrival in the Market Square: north to the North Gate,
back through the square, south over the Silverbrook Bridge to the South Gate, then across the Valemere Road bridge. Writes
silverbrook-game-review.json and captures (arrival plus one per ReviewCamera_*) beside this script.
"""
import json, math, os, time, sys
from pathlib import Path
from validate_layout import SILVERBROOK_LAYOUT
import unreal as u

ROOT = Path(__file__).resolve().parent
EVIDENCE = ROOT.parents[2] / 'docs/evidence/graphics' / os.environ.get('GFX_EVIDENCE_DIR', '2026-10-21-before')
LAYOUT = json.loads(SILVERBROOK_LAYOUT.read_text(encoding='utf-8'))
ROUTE = LAYOUT['review_route']
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pc = u.GameplayStatics.get_player_controller(world, 0)
pawn = u.GameplayStatics.get_player_character(world, 0)
mode = u.GameplayStatics.get_game_mode(world)
fixture_class = u.load_class(None, '/Script/ShatteredFrontier.SFE1InteractionTarget')
state = dict(scope='Project pawn walks the review route from the arrival through the graybox, then swims in the river and climbs out; no AI, combat, gathering or performance acceptance',
             game_mode=mode.get_class().get_name() if mode else None, pawn=pawn.get_class().get_name() if pawn else None,
             e1_fixtures=len(u.GameplayStatics.get_all_actors_of_class(world, fixture_class)),
             start_cm=None, target=1, reached=[], stuck_events=0)
started = time.monotonic(); walk_started = None; handle = None; frame = 0
last = dict(pos=None, at=0.0)
frames = []  # frame times in ms during the walk (offscreen, 1280x720; for comparisons, not a budget)
shots = dict(aerial=None, taken=0)  # aerial: when the walk ended; taken: review cameras captured so far
swim = dict(started=None)  # after the walk: dropped under the river, the pawn must swim, then climb out on a bank


def swim_spot():
    """The first river segment midpoint within 700 m of the centre and 40 m from every road and from the wall: (x, y, water level, bank direction)."""
    river, levels = LAYOUT['river']['points'], LAYOUT['elevation']['river_levels']
    roads = [pts for tier in ('primary', 'secondary', 'trail') for pts in LAYOUT['roads'][tier].values()]
    W = LAYOUT['wall']
    def seg(p, a, b):
        dx, dy = b[0] - a[0], b[1] - a[1]
        t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / ((dx * dx + dy * dy) or 1)))
        return math.dist(p, (a[0] + dx * t, a[1] + dy * t))
    for i, (a, b) in enumerate(zip(river, river[1:])):
        m = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
        wall = abs(math.hypot(m[0] / W['semi_axis_x'], m[1] / W['semi_axis_y']) - 1) * W['semi_axis_x']
        if wall > 40 and math.hypot(*m) < 700 and all(seg(m, q, r) > 40 for pts in roads for q, r in zip(pts, pts[1:])):
            n = math.dist(a, b)
            return m[0], m[1], (levels[i] + levels[i + 1]) / 2, (-(b[1] - a[1]) / n, (b[0] - a[0]) / n)


def shot(name):
    u.SystemLibrary.execute_console_command(world, 'HighResShot 1280x720 filename=' + str(ROOT / name).replace('\\', '/'))


def finish():
    state['elapsed_seconds'] = round(time.monotonic() - started, 1)
    state['passed'] = (state.get('walk_completed') is True and state['e1_fixtures'] == 0
                       and state.get('swim', {}).get('swimming_after_4s') is True and state['swim'].get('climbed_out') is True
                       and state['game_mode'] == 'SFGameMode' and state['pawn'] == 'SFE1Character')
    (ROOT / 'silverbrook-game-review.json').write_text(json.dumps(state, indent=2))
    u.log(f"SILVERBROOK_REVIEW passed={state['passed']} reached={len(state['reached'])}/{len(ROUTE) - 1}")
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.execute_console_command(world, 'quit')


def stuck_report(p, ahead):
    """What the pawn is doing and what its own capsule meets 2 m ahead, for diagnosing a stall."""
    move, capsule = pawn.character_movement, pawn.capsule_component
    r, hh = capsule.get_scaled_capsule_radius(), capsule.get_scaled_capsule_half_height()
    types = [getattr(u.ObjectTypeQuery, f'OBJECT_TYPE_QUERY{i}') for i in range(1, 7)]
    hits = u.SystemLibrary.capsule_trace_multi_for_objects(world, p, p + ahead * 200, r, hh, types, True, [pawn],
                                                           u.DrawDebugTrace.NONE, True) or []
    floor = move.current_floor.hit_result.to_tuple() if move.current_floor else None
    return dict(velocity_cm_s=round(pawn.get_velocity().length()), mode=str(move.movement_mode),
                capsule=[round(r), round(hh)], floor=floor[9].get_actor_label() if floor and floor[9] else None,
                sweep=[dict(actor=h.to_tuple()[9].get_actor_label() if h.to_tuple()[9] else None,
                            impact_cm=[round(h.to_tuple()[5].x), round(h.to_tuple()[5].y), round(h.to_tuple()[5].z)],
                            blocking=h.to_tuple()[0]) for h in hits])


def tick(delta):
    global walk_started, frame
    frame += 1
    if frame < 90:
        return
    try:
        now = time.monotonic()
        p = pawn.get_actor_location()
        if walk_started is None:
            walk_started = now; state['start_cm'] = [round(p.x), round(p.y), round(p.z)]
            shot('Silverbrook-Arrival-InGame.png'); last.update(pos=(p.x, p.y), at=now)
            return
        if swim['started'] is not None and shots['aerial'] is None:
            t = now - swim['started']; mode_ = str(pawn.character_movement.movement_mode)
            if t > 4 and 'swimming_after_4s' not in state['swim']:
                state['swim'].update(swimming_after_4s='SWIMMING' in mode_, rise_cm=round(p.z - swim['z0']),
                                     centre_below_surface_cm=round(state['swim']['surface_cm'] - p.z))
            if t > 4:
                bx, by = swim['bank']; pawn.add_movement_input(u.Vector(bx, by, 0), 1, False)
                if 'WALKING' in mode_:
                    state['swim'].update(climbed_out=True, out_after_s=round(t - 4, 1)); shots['aerial'] = now
                elif t > 20:
                    state['swim'].update(climbed_out=False, mode=mode_); shots['aerial'] = now
            return
        if shots['aerial'] is not None:  # after the walk: each ReviewCamera_* in turn, 3 s to settle, then a capture
            cams = sorted(u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor), key=lambda c: c.get_actor_label())
            phase = int((now - shots['aerial']) // 3); i = phase // 2  # even phase: switch to camera i; odd: capture it
            if i < len(cams) and i == shots['taken']:  # HighResShot writes a frame later, so never switch in the same tick
                if phase % 2 == 0:
                    pc.set_view_target_with_blend(cams[i], 0)
                else:
                    shot(f"Silverbrook-{cams[i].get_actor_label().replace('ReviewCamera_', '')}-InGame.png")
                    shots['taken'] += 1
            elif shots['taken'] == len(cams) and phase > 2 * len(cams) + 1:
                finish()
            return
        frames.append(delta * 1000)
        x, y = ROUTE[state['target']]
        dx, dy = x * 100 - p.x, y * 100 - p.y; d = math.hypot(dx, dy)
        if d < 300:
            state['reached'].append(dict(xy_metres=[x, y], seconds=round(now - walk_started, 1), z_cm=round(p.z)))
            state['target'] += 1
        else:
            pawn.add_movement_input(u.Vector(dx / d, dy / d, 0), 1, False)
        if now - last['at'] > 3:
            if math.dist(last['pos'], (p.x, p.y)) < 200:
                state['stuck_events'] += 1; state.setdefault('stuck_at_cm', []).append([round(p.x), round(p.y)])
                if 'stuck_report' not in state:
                    state['stuck_report'] = stuck_report(p, u.Vector(dx / d, dy / d, 0))
            last.update(pos=(p.x, p.y), at=now)
        if state['target'] == len(ROUTE) or now - walk_started > 420 or state['stuck_events'] >= 3:
            state['walk_completed'] = state['target'] == len(ROUTE)
            state['walk_seconds'] = round(now - walk_started, 1)
            ordered = sorted(frames)
            state['frame_ms'] = dict(median=round(ordered[len(ordered) // 2], 1), p95=round(ordered[int(len(ordered) * 0.95)], 1), frames=len(ordered))
            state['final_pawn_cm'] = [round(p.x), round(p.y), round(p.z)]
            sx, sy, level, bank = swim_spot()  # then the swim check, then each ReviewCamera_* placed by build_silverbrook.py
            z0 = (level - 1.0) * 100  # the capsule (about 0.9 m half height) clears the 2 m deep bed
            pawn.set_actor_location(u.Vector(sx * 100, sy * 100, z0), False, True)
            swim.update(started=now, z0=z0, bank=bank)
            state['swim'] = dict(at_metres=[round(sx), round(sy)], surface_cm=round(level * 100), dropped_cm=round(z0))
    except Exception as error:
        state['error'] = str(error); finish()


def reference_capture():
    """GFX-01 fixed views in the built editor target's -game world."""
    bookmarks = json.loads((ROOT / 'silverbrook-reference-bookmarks.json').read_text(encoding='utf-8'))
    if os.environ.get('GFX_EVIDENCE_DIR'):
        bookmarks['views'] += [
            dict(name='05-market-plaza', xy_m=[55, -82], look_xy_m=[0, -30], eye_height_m=5,
                 look_height_m=0, fov_deg=70),
            dict(name='06-cemetery-trail', xy_m=[-75, 594], look_xy_m=[-245, 531], eye_height_m=8,
                 look_height_m=0, fov_deg=70),
            dict(name='07-farmlands', xy_m=[-430, 275], look_xy_m=[-550, 260], eye_height_m=18,
                 look_height_m=0, fov_deg=70),
        ]
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    camera = next(c for c in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor)
                  if c.get_actor_label() == 'ReviewCamera_Aerial')
    sun = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.DirectionalLight) if a.get_actor_label() != 'Moon')
    sun.set_actor_rotation(u.Rotator(pitch=-40, yaw=-60, roll=0), False)
    for command in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0',
                    'r.MotionBlurQuality 0', 'r.ScreenPercentage 100', 'r.ExposureOffset 0',
                    'ShowFlag.TextRender 0'):
        u.SystemLibrary.execute_console_command(world, command)
    pawn.character_movement.disable_movement()
    capture = dict(frame=0, index=0)

    def ground_z(x, y):
        start, end = u.Vector(x * 100, y * 100, 100000), u.Vector(x * 100, y * 100, -100000)
        hit = u.SystemLibrary.line_trace_single(world, start, end, u.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                 False, [pawn], u.DrawDebugTrace.NONE, True)
        return hit.to_tuple()[4].z if hit else 0

    def reference_tick(_delta):
        capture['frame'] += 1
        if capture['frame'] < 90:
            return
        local = (capture['frame'] - 90) % 150
        view = bookmarks['views'][capture['index']]
        if local == 0:
            x, y = view['xy_m']; tx, ty = view['look_xy_m']
            z = ground_z(x, y) + view['eye_height_m'] * 100
            target_z = ground_z(tx, ty) + view['look_height_m'] * 100
            yaw = math.degrees(math.atan2(ty - y, tx - x))
            pitch = math.degrees(math.atan2(target_z - z, math.hypot(tx - x, ty - y) * 100))
            camera.set_actor_location(u.Vector(x * 100, y * 100, z), False, False)
            camera.set_actor_rotation(u.Rotator(pitch=pitch, yaw=yaw, roll=0), False)
            camera.camera_component.set_field_of_view(view['fov_deg'])
            pc.set_view_target_with_blend(camera, 0)
            pawn.set_actor_hidden_in_game(not view.get('over_shoulder', False))
            if view.get('over_shoulder', False):
                forward = u.Vector(math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0)
                right = u.Vector(-forward.y, forward.x, 0)
                pawn.set_actor_location(u.Vector(x * 100, y * 100, ground_z(x, y) + 100) + forward * 500 + right * 150,
                                        False, True)
        if local == 125:
            name = view['name'] + '.png'
            resolution = '1920x1080' if os.environ.get('GFX_EVIDENCE_DIR') else '2560x1440'
            u.SystemLibrary.execute_console_command(world, 'HighResShot ' + resolution + ' filename=' +
                                                     str(EVIDENCE / name).replace('\\', '/'))
            u.log('GFX01_CAPTURE ' + name)
        if local == 149:
            capture['index'] += 1
            if capture['index'] == len(bookmarks['views']):
                u.unregister_slate_post_tick_callback(capture['handle'])
                u.SystemLibrary.execute_console_command(world, 'quit')

    capture['handle'] = u.register_slate_post_tick_callback(reference_tick)


if '-GfxReferenceCapture' in u.SystemLibrary.get_command_line() or '-GfxReferenceCapture' in sys.argv:
    reference_capture()
else:
    handle = u.register_slate_post_tick_callback(tick)
