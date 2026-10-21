"""Frame-time probe at a WP8 bookmark in a real -game run (project pawn parked at the bookmark eye, looking at its target).

UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -ResX=2560 -ResY=1440 -windowed
    -ExecutePythonScript="<this file> <view_name>"
Warms up 6 s, then records 600 frames; writes perf-<view>.json beside this script (median / p95 / worst ms, fps) and quits.
"""
import json, math, sys, time
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parent
name = sys.argv[1] if len(sys.argv) > 1 else '01-forest-path-town'
view = next(v for v in json.loads((ROOT / 'silverbrook-reference-bookmarks.json').read_text(encoding='utf-8'))['views'] if v['name'] == name)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world() if hasattr(u, 'UnrealEditorSubsystem') and False else u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
st = dict(start=time.monotonic(), placed=False, frames=[])


def ground_z(x, y):
    hit = u.SystemLibrary.line_trace_single(world, u.Vector(x, y, 3e5), u.Vector(x, y, -3e5), u.TraceTypeQuery.ECC_VISIBILITY if hasattr(u.TraceTypeQuery, 'ECC_VISIBILITY') else u.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                            False, [], u.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 0.0


def tick(delta):
    pawn, pc = u.GameplayStatics.get_player_character(world, 0), u.GameplayStatics.get_player_controller(world, 0)
    if not pawn or not pc:
        return
    t = time.monotonic() - st['start']
    if not st['placed']:
        (x, y), (lx, ly) = [[m * 100 for m in view[k]] for k in ('xy_m', 'look_xy_m')]
        z = ground_z(x, y)
        pawn.set_actor_location(u.Vector(x, y, z + 120), False, False)
        pc.set_control_rotation(u.MathLibrary.find_look_at_rotation(u.Vector(x, y, z + 170), u.Vector(lx, ly, ground_z(lx, ly) + view['look_height_m'] * 100)))
        st['placed'] = True
    if t > 6:
        st['frames'].append(delta * 1000)
    if len(st['frames']) >= 600:
        f = sorted(st['frames'])
        out = dict(view=name, frames=len(f), median_ms=round(f[len(f) // 2], 2), p95_ms=round(f[int(len(f) * .95)], 2), worst_ms=round(f[-1], 2),
                   fps_median=round(1000 / f[len(f) // 2], 1), fps_p95=round(1000 / f[int(len(f) * .95)], 1))
        (ROOT / f'perf-{name}.json').write_text(json.dumps(out, indent=2))
        u.log('PERF ' + json.dumps(out))
        u.unregister_slate_post_tick_callback(st['h'])
        u.SystemLibrary.execute_console_command(world, 'quit')


st['h'] = u.register_slate_post_tick_callback(tick)
