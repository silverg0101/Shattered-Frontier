"""Per-pass GPU times from a ProfileGPU dump in a -game log (perf_gpu.py). Run: py -3.12 gpu_passes.py <tag>/perf-01.log

Reads the graphics-pipeline table, prints and writes <tag>/perf-01-gpu.json: frame total and the inclusive ms of every direct child of `Scene`.
"""
import json, re, sys
from pathlib import Path

log = Path(sys.argv[1]); lines = log.read_text(encoding='utf-8', errors='replace').splitlines()
start = next(i for i, l in enumerate(lines) if 'GPU Profile for Frame' in l and 'Graphics pipeline' in l)
rows = []
for l in lines[start:]:
    if 'LogRHI' not in l: break
    c = l.split('┃')
    if len(c) < 5: continue
    m = re.search(r'([\d.]+) ms', c[2])  # inclusive column
    if not m: continue
    name = c[3].rstrip()
    if not name.strip(): continue  # wrapped continuation rows
    rows.append((len(name) - len(name.lstrip()), name.strip(), float(m.group(1))))
scene = next(i for i, r in enumerate(rows) if r[1] == 'Scene')
d0 = rows[scene][0]; sub = []
for r in rows[scene + 1:]:
    if r[0] <= d0: break
    sub.append(r)
kids = [r for r in sub if r[0] == min(x[0] for x in sub)]  # direct children of Scene
frame = max(r[2] for r in rows if r[1].startswith('Frame '))
out = dict(log=log.name, frame_ms=frame, scene_ms=rows[scene][2],
           passes={})
for _, n, ms in kids: n = re.sub(r'\d+ card captures [\d.]+M texels', 'N card captures', n); out['passes'][n] = round(out['passes'].get(n, 0) + ms, 3)  # repeated passes summed
out['passes'] = dict(sorted(((k, v) for k, v in out['passes'].items() if v >= 0.02), key=lambda kv: -kv[1]))
(log.parent / 'perf-01-gpu.json').write_text(json.dumps(out, indent=2)); print(json.dumps(out, indent=2))
