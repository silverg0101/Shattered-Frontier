"""TER-01b: per-pass GPU deltas (voxel ground visible minus hidden) from the ProfileGPU dumps zone_perf.py leaves in a log.

    py -3.12 tools/ter01/zone_gpu_passes.py <dir>   (reads <dir>/client.log and <dir>/zone-perf.json, writes <dir>/zone-gpu.json)
Each TER01B_PROFILE <view> <on|off> <i> marker is followed by one 'GPU Profile for Frame' tree; per pass (direct children of
Scene, repeats summed) the median of the three dumps per state is taken. Ground GPU = the sum of the deltas of the passes that
draw the ground's own geometry (depth prepass, base pass, shadow depths, velocity); the frame delta is reported beside it.
"""
import json, re, statistics, sys
from pathlib import Path

GEOMETRY = ('PrePass', 'DepthPass', 'BasePass', 'ShadowDepths', 'Velocity', 'RenderVelocities')
d = Path(sys.argv[1]); lines = d.joinpath('client.log').read_text(encoding='utf-8', errors='replace').splitlines()


def tree(start):
    rows = []
    for l in lines[start:]:
        if 'LogRHI' not in l:
            if rows: break
            continue
        c = l.split('┃')
        if len(c) < 5: continue
        m = re.search(r'([\d.]+) ms', c[2])
        if not m: continue
        name = c[3].rstrip()
        if not name.strip(): continue
        rows.append((len(name) - len(name.lstrip()), name.strip(), float(m.group(1))))
    return rows


def passes(rows):
    scene = next(i for i, r in enumerate(rows) if r[1] == 'Scene')
    d0 = rows[scene][0]; sub = []
    for r in rows[scene + 1:]:
        if r[0] <= d0: break
        sub.append(r)
    top = min(x[0] for x in sub); out = {}
    for _, n, ms in (r for r in sub if r[0] == top):
        n = re.sub(r'\d+ card captures [\d.]+M texels', 'N card captures', n); n = re.sub(r'\(\d+.*?\)', '', n).strip()
        out[n] = out.get(n, 0) + ms
    frame = max(r[2] for r in rows if r[1].startswith('Frame '))
    return frame, out


samples = {}
for i, l in enumerate(lines):
    m = re.search(r'TER01B_PROFILE (\S+) (on|off|cmp) (\d)', l)
    if not m: continue
    j = next((k for k in range(i, len(lines)) if 'GPU Profile for Frame' in lines[k] and 'Graphics pipeline' in lines[k]), None)
    if j is None: continue
    samples.setdefault(m.group(1), {}).setdefault(m.group(2), []).append(passes(tree(j)))
perf = json.loads(d.joinpath('zone-perf.json').read_text())
res = {}
for view, st in samples.items():
    med = {}
    for state, lst in st.items():
        names = set().union(*(p for _, p in lst))
        med[state] = dict(frame=statistics.median(f for f, _ in lst), passes={n: statistics.median(p.get(n, 0.0) for _, p in lst) for n in names})
    on, off = med['on'], med['off']

    def deltas(a, b):
        names = set(a['passes']) | set(b['passes'])
        return {n: round(a['passes'].get(n, 0) - b['passes'].get(n, 0), 3) for n in names}

    delta = deltas(on, off)
    geo = {n: v for n, v in delta.items() if any(n.startswith(g) for g in GEOMETRY)}
    today = None
    if 'cmp' in med:  # today's Silverbrook ground mesh (same material, frame and view) instead of the voxel ground
        dc = deltas(med['cmp'], off)
        gc = {n: v for n, v in dc.items() if any(n.startswith(g) for g in GEOMETRY)}
        today = dict(ground_gpu_ms=round(sum(gc.values()), 3), geometry_pass_deltas=gc, frame_profile_delta_ms=round(med['cmp']['frame'] - off['frame'], 3),
                     gpu_ms_p50=perf.get(view, {}).get('cmp', {}).get('gpu_ms_p50'),
                     voxel_minus_today_ground_ms=round(sum(geo.values()) - sum(gc.values()), 3), voxel_minus_today_frame_ms=round(on['frame'] - med['cmp']['frame'], 3))
    s = perf.get(view, {}).get('stats', {})
    res[view] = dict(ground_gpu_ms=round(sum(geo.values()), 3), geometry_pass_deltas=geo, frame_profile_delta_ms=round(on['frame'] - off['frame'], 3),
                     frame_on_ms=round(on['frame'], 3), gpu_ms_p50_on=perf.get(view, {}).get('on', {}).get('gpu_ms_p50'), gpu_ms_p50_off=perf.get(view, {}).get('off', {}).get('gpu_ms_p50'),
                     other_deltas_ms={n: v for n, v in sorted(delta.items(), key=lambda kv: -abs(kv[1])) if n not in geo and abs(v) >= 0.05},
                     tris=s.get('tris'), tris_by_lod=s.get('tris_by_lod'), seams=perf.get(view, {}).get('seams', {}).get('tears'), today=today)
d.joinpath('zone-gpu.json').write_text(json.dumps(res, indent=2))
for v, r in res.items():
    t = r['today']
    extra = f" | today's ground {t['ground_gpu_ms']} ms, voxel - today {t['voxel_minus_today_ground_ms']} ms (frame {t['voxel_minus_today_frame_ms']})" if t else ''
    print(f"{v}: ground {r['ground_gpu_ms']} ms | frame delta {r['frame_profile_delta_ms']} (stat p50 {r['gpu_ms_p50_on']} vs {r['gpu_ms_p50_off']}) | tris {r['tris']} | seams {r['seams']}{extra}")
