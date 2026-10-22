"""Frame time + per-pass GPU timing at bookmark 01 in a real -game run (GFX-02 J2-pre; perf_bookmark_game.py plus a ProfileGPU dump).

$env:GFX_TAG='before'|'after'; UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -ResX=2560 -ResY=1440 -windowed
    -log -abslog=<abs>/<tag>/perf-01.log "-ExecCmds=py <this file>"
Warms up 6 s, records 500 frames (median / p95 / worst ms), then runs `ProfileGPU` (the per-pass tree goes to the log; `stat gpu` is an overlay
HighResShot does not draw), waits 150 frames, quits. Writes <tag>/perf-01.json beside this script; gpu_passes.py reads the log.
"""
import json, math, os, time
from pathlib import Path
import unreal as u

HERE = Path(__file__).resolve().parent; OUT = HERE / os.environ.get('GFX_TAG', 'after'); OUT.mkdir(exist_ok=True)
view = next(v for v in json.loads((HERE.parents[3] / 'GameClient/ArtSource/Greenlands/silverbrook-reference-bookmarks.json').read_text(encoding='utf-8'))['views'] if v['name'] == '01-forest-path-town')
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
st = dict(start=time.monotonic(), placed=False, frames=[], stat_at=None, shot=False)
cmd = lambda c: u.SystemLibrary.execute_console_command(world, c)


def ground_z(x, y):
    hit = u.SystemLibrary.line_trace_single(world, u.Vector(x, y, 3e5), u.Vector(x, y, -3e5), u.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], u.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 0.0


def tick(delta):
    pawn, pc = u.GameplayStatics.get_player_character(world, 0), u.GameplayStatics.get_player_controller(world, 0)
    if not pawn or not pc: return
    t = time.monotonic() - st['start']
    if not st['placed']:
        (x, y), (lx, ly) = [[m * 100 for m in view[k]] for k in ('xy_m', 'look_xy_m')]
        z = ground_z(x, y)
        pawn.set_actor_location(u.Vector(x, y, z + 120), False, False)
        pc.set_control_rotation(u.MathLibrary.find_look_at_rotation(u.Vector(x, y, z + 170), u.Vector(lx, ly, ground_z(lx, ly) + view['look_height_m'] * 100)))
        st['placed'] = True
    if st['stat_at'] is None:
        if t > 6: st['frames'].append(delta * 1000)
        if len(st['frames']) >= 500:
            f = sorted(st['frames'])
            out = dict(view=view['name'], frames=len(f), median_ms=round(f[len(f) // 2], 2), p95_ms=round(f[int(len(f) * .95)], 2), worst_ms=round(f[-1], 2),
                       fps_median=round(1000 / f[len(f) // 2], 1), fps_p95=round(1000 / f[int(len(f) * .95)], 1), resolution='2560x1440 windowed, 100% screen percentage, project defaults')
            (OUT / 'perf-01.json').write_text(json.dumps(out, indent=2)); u.log('PERF ' + json.dumps(out))
            cmd('r.ProfileGPU.ShowUI 0'); cmd('ProfileGPU'); st['stat_at'] = 0
    else:
        st['stat_at'] += 1
        if st['stat_at'] == 150:
            u.unregister_slate_post_tick_callback(st['h']); cmd('quit')


st['h'] = u.register_slate_post_tick_callback(tick)
