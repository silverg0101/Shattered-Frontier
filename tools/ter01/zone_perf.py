"""TER-01b zone GPU check, run inside a -game client by tools/ter01/Run-ZonePerf.ps1 (RTX 3060, 1920x1080).

For each view (GFX-02 bookmarks 01, 02, 05 on the voxel zone, plus A3 at 250 m and a 600 m overview for the worst case):
places the spike camera (eye height over the zone ground), waits for the LOD meshing to settle, checks every shown leaf for
seams (SeamCheckAll: LOD transition faces included), then measures 240 frames of GPU time and runs ProfileGPU three times
with the voxel ground visible and again hidden. Markers TER01B_PROFILE <view> <on|off> <i> precede each dump in the log;
tools/ter01/zone_gpu_passes.py turns them into per-pass deltas. Writes <out>/zone-perf.json.
"""
import json, sys, time
from pathlib import Path
import unreal as u

OUT = Path(sys.argv[1]); OUT.mkdir(parents=True, exist_ok=True)
ROOT = Path(__file__).resolve().parents[2]
BM = {v['name']: v for v in json.loads((ROOT / 'GameClient/ArtSource/Greenlands/silverbrook-reference-bookmarks.json').read_text(encoding='utf-8'))['views']}
VIEWS = [('01-forest-path-town', BM['01-forest-path-town']), ('02-main-street', BM['02-main-street']), ('05-market-square', BM['05-market-square']),
         ('A3-south-approach-250m', BM['A3-south-approach-250m']),
         ('overview-600m', dict(xy_m=[-900, -900], eye_height_m=600, look_xy_m=[200, 200], look_height_m=0))]
st = dict(world=None, spike=None, t=0.0, i=0, phase='find', wait=0.0, frames=[], res={}, prof=0, settled_at=None)


def cmd(c):
    u.SystemLibrary.execute_console_command(st['world'], c)


def place(v):
    (x, y), (lx, ly) = [[m * 100 for m in v[k]] for k in ('xy_m', 'look_xy_m')]
    sp = st['spike']
    eye = u.Vector(x, y, sp.ground_z(x, y) + v['eye_height_m'] * 100)
    at = u.Vector(lx, ly, sp.ground_z(lx, ly) + v['look_height_m'] * 100)
    sp.set_spike_camera(eye, at)
    pawn = u.GameplayStatics.get_player_controller(st['world'], 0).get_controlled_pawn()
    if pawn:  # the player stands under the view (LOD focus = camera + pawn, as in play)
        pawn.set_actor_location(u.Vector(x, y, sp.ground_z(x, y) + 120), False, True)


def tick(delta):
    st['t'] += delta
    if st['phase'] == 'find':
        for w in u.ObjectIterator(u.World):
            sp = u.GameplayStatics.get_all_actors_of_class(w, u.SFTerrainSpike) if w.get_name() == 'TerrainSpike_Voxel' else []
            if sp and u.GameplayStatics.get_player_controller(w, 0):
                st['world'], st['spike'] = w, sp[0]
        if st['world'] is None:
            return
        for c in ('r.MotionBlurQuality 0', 't.MaxFPS 0', 'r.VSync 0', 'r.ProfileGPU.ShowUI 0', 'ShowFlag.Particles 0'):
            cmd(c)
        pawn = u.GameplayStatics.get_player_controller(st['world'], 0).get_controlled_pawn()
        if pawn:
            pawn.set_actor_hidden_in_game(True)
        st['phase'] = 'place'; return
    name, v = VIEWS[st['i']] if st['i'] < len(VIEWS) else (None, None)
    sp = st['spike']
    if st['phase'] == 'place':
        if name is None:
            (OUT / 'zone-perf.json').write_text(json.dumps(st['res'], indent=2))
            u.log('TER01B_ZONE_PERF ' + json.dumps(st['res']))
            u.log('TER01B_ZONE_DONE')
            u.unregister_slate_post_tick_callback(handle)
            cmd('quit'); return
        place(v); st['res'][name] = dict(view=v); st['phase'] = 'settle'; st['wait'] = st['t'] + 2.0; st['settled_at'] = None; return
    if st['phase'] == 'settle':
        if not sp.is_settled():
            st['settled_at'] = None; return
        st['settled_at'] = st['settled_at'] or st['t']
        if st['t'] < st['wait'] or st['t'] - st['settled_at'] < 1.5:
            return
        st['res'][name]['seams'] = json.loads(sp.seam_check_all())
        st['res'][name]['stats'] = json.loads(sp.stats_json())
        st['phase'] = 'on'; st['frames'] = []; st['wait'] = st['t'] + 1.0; st['prof'] = 0; return
    if st['phase'] in ('on', 'off', 'cmp'):
        if st['t'] < st['wait']:
            return
        if len(st['frames']) < 240:
            st['frames'].append((delta * 1000.0, sp.gpu_frame_ms())); return
        if st['prof'] < 3:
            u.log(f"TER01B_PROFILE {name} {st['phase']} {st['prof']}")
            cmd('ProfileGPU'); st['prof'] += 1; st['wait'] = st['t'] + 0.5; return
        f = sorted(x[0] for x in st['frames']); g = sorted(x[1] for x in st['frames'])
        st['res'][name][st['phase']] = dict(frame_ms_p50=round(f[120], 2), frame_ms_p95=round(f[228], 2), gpu_ms_p50=round(g[120], 2), gpu_ms_p95=round(g[228], 2))
        if st['phase'] == 'on':
            sp.set_voxel_visible(False); st['phase'] = 'off'; st['frames'] = []; st['prof'] = 0; st['wait'] = st['t'] + 1.0
        elif st['phase'] == 'off' and sp.set_compare_visible(True):  # today's Silverbrook ground mesh, same view
            st['phase'] = 'cmp'; st['frames'] = []; st['prof'] = 0; st['wait'] = st['t'] + 2.0
        else:
            sp.set_compare_visible(False); sp.set_voxel_visible(True); st['i'] += 1; st['phase'] = 'place'


handle = u.register_slate_post_tick_callback(tick)
