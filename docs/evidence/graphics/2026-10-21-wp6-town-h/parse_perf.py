"""Turn the capture_perf.py log into perf.json: GPU graphics-root per phase and FX on/off deltas, Niagara game-thread
and worker-thread cost from the one-frame stat dumps, active-instance counts. Usage: py -3 parse_perf.py <wp6h-perf.log>"""
import json, re, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
lines = Path(sys.argv[1]).read_text(encoding='utf-8', errors='replace').splitlines()
counts = json.loads((HERE / 'perf-counts.json').read_text())
seg, out = None, {}
for i, l in enumerate(lines):
    m = re.search(r'WP6H_PERF_SAMPLE (\w+) (\S+)', l)
    if m:
        seg = f'{m.group(1)}/{m.group(2)}'; out[seg] = {'gt_ms': None, 'worker_ms': 0.0, 'rt_ms': 0.0, **counts.get(seg, {})}; gp = False; continue
    if not seg: continue
    r = out[seg]
    if 'Graphics pipeline 0' in l: gp = True
    if gp and 'Frame Time' in l and 'frame_ms' not in r: r['frame_ms'] = float(re.search(r'([\d.]+)ms', l).group(1))
    if gp and '<root>' in l and 'root_ms' not in r: r['root_ms'] = float(re.findall(r'([\d.]+) ms', l)[-1])
    m = re.search(r'([\d.]+)ms \(\s*(\d+)\)\s+-\s+(GT Total - STAT_NiagaraOverview_GT |.*NS_ChimneySmoke \[(GT_CNC|RT)\])', l)
    if m:
        v = float(m.group(1))
        if m.group(3).startswith('GT Total'): r['gt_ms'] = max(r['gt_ms'] or 0, v)
        elif m.group(4) == 'GT_CNC': r['worker_ms'] += v
        else: r['rt_ms'] += v
for r in out.values():
    for k in ('worker_ms', 'rt_ms'): r[k] = round(r[k], 3)
def delta(eye):
    on = [out[f'{eye}/on-{n}']['root_ms'] for n in (1, 2)]; off = [out[f'{eye}/off-{n}']['root_ms'] for n in (1, 2)]
    return round(sum(on) / 2 - sum(off) / 2, 3)
report = {'resolution': [2560, 1440], 'samples': out, 'gpu_root_delta_on_minus_off_ms': {e: delta(e) for e in ('chimney', 'street')},
          'note': 'gt_ms = STAT_NiagaraOverview_GT of one frame; worker_ms / rt_ms = sum of every NS_ChimneySmoke [GT_CNC] / [RT] entry in that frame dump (nested entries counted again, so an upper bound); off hides the 85 ChimneySmoke components.'}
(HERE / 'perf.json').write_text(json.dumps(report, indent=1) + '\n')
print('WP6H_PERF', json.dumps(report['gpu_root_delta_on_minus_off_ms']), {k: (v.get('root_ms'), v['gt_ms'], v['worker_ms'], v.get('active_within_120m')) for k, v in out.items()})
