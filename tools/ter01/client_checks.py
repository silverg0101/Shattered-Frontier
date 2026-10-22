"""TER-01 spike client checks, run in the -game client by Run-TerrainSpikeNet.ps1 (-ClientPy) or standalone:
    UnrealEditor.exe <uproject> /Game/Frontier/Maps/Dev/TerrainSpike/TerrainSpike_Voxel -game -ResX=1920 -ResY=1080 "-ExecCmds=py tools/ter01/client_checks.py <out>"
Waits for the replicated op log, captures the dig sequence at 1920x1080, then traces the replicated collision (dig floor, tunnel roof
= overhang, shaft bedrock), counts open edges (cracks), measures the voxel/heightfield seam and voxel GPU cost. Writes client-checks.json.
Standalone (no server) applies the same sequence locally because ASFTerrainSpike runs it with -SFTerrainSpikeScenario.
"""
import json, math, sys
from pathlib import Path
import unreal as u

OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('docs/evidence/terrain/ter01')
OUT.mkdir(parents=True, exist_ok=True)
TOTAL_EDITS = 51  # 3 + 15 tunnel + 7 shaft + 6 TER-02-legal + 20 random (2 rejections are not edits)
SHOTS = {0: 'dig-0-before', 1: 'dig-1-interior', 3: 'dig-2-chunk-corner', 18: 'dig-3-tunnel', 25: 'dig-4-shaft', 31: 'dig-5-ter02-legal'}
VIEWS = {  # eye cm, look-at cm
    'dig-0-before': ((1500, 1500, 900), (3200, 3200, 0)), 'dig-1-interior': ((1500, 1500, 900), (3200, 3200, 0)),
    'dig-2-chunk-corner': ((1500, 1500, 900), (3200, 3200, 0)), 'dig-3-tunnel': ((700, 5300, 500), (2300, 6000, 150)),
    'dig-4-shaft': ((5100, 1100, 1300), (6000, 2000, -500)), 'dig-5-ter02-legal': ((3300, 2300, 1100), (4700, 4300, 0)), 'dig-6-final-overview': ((-2500, -2500, 4500), (4800, 4800, 0)),
    'look-seam-heightfield-vs-voxel': ((-900, 3000, 700), (300, 4800, 100))}
st = dict(t=0.0, world=None, spike=None, cam=None, pc=None, shots=[], done=set(), phase='wait', perf={}, frames=[])


def base_h(x, y):
    mound = 7.0 * math.exp(-((x - 30) ** 2 + (y - 60) ** 2) / 150.0)
    return 3.0 * math.sin(x / 23.0) * math.cos(y / 31.0) + 1.5 * math.sin((x + 2 * y) / 11.0) + 0.04 * (x - 48) + mound


def find():
    for w in u.ObjectIterator(u.World):
        if w.get_name() != 'TerrainSpike_Voxel':
            continue
        sp = u.GameplayStatics.get_all_actors_of_class(w, u.SFTerrainSpike)
        pc = u.GameplayStatics.get_player_controller(w, 0)
        if sp and pc:
            return w, sp[0], pc
    return None, None, None


def view(name):
    eye, at = VIEWS[name]
    st['spike'].set_spike_camera(u.Vector(*eye), u.Vector(*at))


def shot(name):
    u.SystemLibrary.execute_console_command(st['world'], 'HighResShot 1920x1080 filename=' + str((OUT / name).resolve()).replace('\\', '/') + '.png')
    st['shots'].append(name); u.log('TER01_CAPTURE ' + name)
    st['hold'] = st['t'] + 0.5  # let HighResShot render before the camera moves


def trace(a, b):
    pawn = st['pc'].get_controlled_pawn()
    hit = u.SystemLibrary.line_trace_single(st['world'], u.Vector(*a), u.Vector(*b), u.TraceTypeQuery.TRACE_TYPE_QUERY1, True,
                                            [pawn] if pawn else [], u.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    return round(t[4].z, 1) if t[0] else None


def seam():
    comps = st['spike'].get_components_by_class(u.ProceduralMeshComponent)
    hf = next(c for c in comps if c.get_name() == 'HeightfieldStrip')
    vox = [c for c in comps if c is not hf]
    gaps = []
    for i in range(0, 161):
        y = 1000 + i * 50
        rh = hf.k2_line_trace_component(u.Vector(-1, y, 3000), u.Vector(-1, y, -3000), True, False, False)
        zv = None
        for c in vox:
            r = c.k2_line_trace_component(u.Vector(1, y, 3000), u.Vector(1, y, -3000), True, False, False)
            if r[0]:
                zv = r[1].z if zv is None else max(zv, r[1].z)
        if rh[0] and zv is not None:
            gaps.append(abs(zv - rh[1].z))
    gaps.sort()
    return dict(samples=len(gaps), max_cm=round(gaps[-1], 2), p95_cm=round(gaps[int(0.95 * (len(gaps) - 1))], 2),
                mean_cm=round(sum(gaps) / len(gaps), 2))


def edits():
    return json.loads(st['spike'].stats_json())['edits']


def tick(delta):
    st['t'] += delta
    if st['world'] is None:
        st['world'], st['spike'], st['pc'] = find()
        if st['world'] is None:
            return
        for c in ('r.MotionBlurQuality 0', 'ShowFlag.Particles 0', 't.MaxFPS 0', 'r.VSync 0'):
            u.SystemLibrary.execute_console_command(st['world'], c)
        pawn = st['pc'].get_controlled_pawn()
        if pawn:
            pawn.set_actor_hidden_in_game(True)
        st['t0'] = st['t']
        return
    if st['t'] - st['t0'] < 3.0 or st['t'] < st.get('hold', 0.0):
        return
    if st['phase'] == 'wait':
        n = edits()
        for k, name in SHOTS.items():
            if n >= k and name not in st['done'] and (k == 0 or n == k or n > k):
                st['done'].add(name); view(name); st['pending'] = (name, st['t'] + 0.6); st['phase'] = 'shoot'; return
        if n >= TOTAL_EDITS:
            st['phase'] = 'checks'
        return
    if st['phase'] == 'shoot':
        name, when = st['pending']
        if st['t'] >= when:
            shot(name); st['phase'] = 'wait'
        return
    if st['phase'] == 'checks':
        s = json.loads(st['spike'].stats_json())
        h_tunnel = base_h(26, 60) * 100
        res = dict(stats=s,
                   dig_interior_floor_z=trace((2400, 2400, 2000), (2400, 2400, -3000)),
                   dig_interior_base_z=round(base_h(24, 24) * 100, 1),
                   tunnel_roof_from_inside_z=trace((2600, 6000, 150), (2600, 6000, 3000)),
                   tunnel_floor_z=trace((2600, 6000, 150), (2600, 6000, -3000)),
                   tunnel_top_surface_from_above_z=trace((2600, 6000, 3000), (2600, 6000, 400)),
                   tunnel_top_base_z=round(h_tunnel, 1),
                   tunnel_axis_clear=trace((1700, 6000, 150), (3000, 6000, 150)),
                   shaft_bottom_z=trace((6000, 2000, 3000), (6000, 2000, -3100)), bedrock_z=-1400,
                   open_edges_interior=st['spike'].count_open_edges(u.Box(u.Vector(100, 100, -3150), u.Vector(9500, 9500, 1550))),
                   open_edges_corner_8_chunks=st['spike'].count_open_edges(u.Box(u.Vector(2700, 2700, -500), u.Vector(3700, 3700, 500))),
                   seam_voxel_vs_1m_heightfield=json.loads(st['spike'].seam_gap_json()), density_hash=st['spike'].density_hash())
        st['res'] = res; st['phase'] = 'final'; view('dig-6-final-overview'); st['pending'] = st['t'] + 0.8
        return
    if st['phase'] == 'final' and st['t'] >= st['pending']:
        shot('dig-6-final-overview'); st['phase'] = 'seamview'  # move the camera only after the shot has rendered
        return
    if st['phase'] == 'seamview':
        view('look-seam-heightfield-vs-voxel'); st['pending'] = st['t'] + 0.8; st['phase'] = 'seamshot'
        return
    if st['phase'] == 'seamshot' and st['t'] >= st['pending']:
        shot('look-seam-heightfield-vs-voxel'); st['phase'] = 'perfview'
        return
    if st['phase'] == 'perfview':
        view('dig-6-final-overview'); st['phase'] = 'perf_on'; st['frames'] = []; st['pending'] = st['t'] + 1.0
        return
    if st['phase'] in ('perf_on', 'perf_off'):
        if st['t'] < st['pending']:
            return
        st['frames'].append((delta * 1000.0, st['spike'].gpu_frame_ms()))
        if len(st['frames']) >= 300:
            f = sorted(x[0] for x in st['frames']); g = sorted(x[1] for x in st['frames'])
            st['perf'][st['phase']] = dict(frame_ms_p50=round(f[150], 2), frame_ms_p95=round(f[284], 2), gpu_ms_p50=round(g[150], 2), gpu_ms_p95=round(g[284], 2))
            if st['phase'] == 'perf_on':
                st['spike'].set_voxel_visible(False); st['phase'] = 'perf_off'; st['frames'] = []; st['pending'] = st['t'] + 1.0
            else:
                st['spike'].set_voxel_visible(True); st['phase'] = 'write'
        return
    if st['phase'] == 'write':
        st['res']['perf_overview_1920x1080'] = st['perf']; st['res']['shots'] = st['shots']
        st['res']['netmode'] = int(st['world'].get_net_mode()) if hasattr(st['world'], 'get_net_mode') else None
        (OUT / 'client-checks.json').write_text(json.dumps(st['res'], indent=2))
        u.log('TER01_CLIENT_RESULT ' + json.dumps(st['res']))
        u.log('TER01_CLIENT_DONE')
        st['phase'] = 'end'
        u.unregister_slate_post_tick_callback(handle)


handle = u.register_slate_post_tick_callback(tick)
