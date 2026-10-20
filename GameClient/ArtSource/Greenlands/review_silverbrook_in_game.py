"""Walk Silverbrook's main route with the project pawn in a real -game session.

Launch: UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook
        -game -RenderOffscreen -ExecutePythonScript=<this file>
Follows review_route in silverbrook-town-layout.json from the arrival in the Market Square: north to the North Gate,
back through the square, south over the Silverbrook Bridge to the South Gate, then across the Valemere Road bridge. Writes
silverbrook-game-review.json and captures (arrival plus one per ReviewCamera_*) beside this script.
"""
import json, math, time
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parent
LAYOUT = json.loads((ROOT / 'silverbrook-town-layout.json').read_text(encoding='utf-8'))
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
                       and state['game_mode'] == 'SFE1GameMode' and state['pawn'] == 'SFE1Character')
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


handle = u.register_slate_post_tick_callback(tick)
