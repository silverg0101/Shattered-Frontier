"""WP6g review acceptance numbers 1-7 and the street thread, measured on chimney-smoke.png against chimney-smoke-ref.png
(same eye, every chimney NiagaraActor hidden; WP6h review fix 2), motion-00..13.png and main-street.png. Numbers only,
no verdict. Writes plume-measure.json.

Plume pixel: |excess| >= 4, excess = L(frame) - L(ref), both 7-row averaged (L = Rec.709 luma of the 8-bit sRGB values).
The absolute value is used because the plume may be darker than what is behind it (field, trees) as well as brighter
(dark slate); the sign is reported in item 5 as plume L / ref L. Item 8 (no rectangle / straight edge > 100 px) is left
to the reviewer's eye.
"""
import json
from pathlib import Path
import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
X0, X1 = 760, 1100   # motion box and street-thread code below are unchanged from WP6h
T = 4.0


def load(name):
    a = np.asarray(Image.open(HERE / name).convert('RGB'), dtype=float)
    return a[..., 0] - a[..., 2], .2126 * a[..., 0] + .7152 * a[..., 1] + .0722 * a[..., 2]


_, L = load('chimney-smoke.png'); _, LR = load('chimney-smoke-ref.png')
def rows(a, y, k=3): return a[max(0, y - k):y + k + 1].mean(0)
def excess(y): return rows(L, y) - rows(LR, y)
def plume(y): ex = excess(y); return np.nonzero(np.abs(ex) >= T)[0], ex


def span(px):
    return None if len(px) == 0 else dict(x0=int(px.min()), x1=int(px.max()), span=int(px.max() - px.min() + 1), centre=round(float((px.min() + px.max()) / 2), 1))


c = {'definition': 'plume = |L(frame) - L(ref)| >= 4, 7-row mean'}
s550, s150 = span(plume(550)[0]), span(plume(150)[0])
c['1_origin_y550'] = s550
c['2_growth_y150'] = s150; c['2_ratio_150_over_550'] = round(s150['span'] / s550['span'], 2) if s550 and s150 else None
veil = []
for y in range(460, 541):
    px = plume(y)[0]; veil.append((int(len(px)), int(px.min()) if len(px) else None, int(px.max()) if len(px) else None))
c['3_max_cols_460_540'] = max(v[0] for v in veil)
c['3_leftmost_x_460_540'] = min((v[1] for v in veil if v[1] is not None), default=None)
c['3_rightmost_x_460_540'] = max((v[2] for v in veil if v[2] is not None), default=None)
c['4_peak_abs_excess_380_540'] = round(max(float(np.abs(excess(y)).max()) for y in range(380, 541)), 1)
c['4_peak_abs_excess_y100'] = round(float(np.abs(excess(100)).max()), 1)
ratios = {}
for y in range(10, 561, 10):
    px, ex = plume(y)
    if len(px): ratios[y] = round(float(rows(L, y)[px].mean() / max(rows(LR, y)[px].mean(), 1e-6)), 3)
c['5_L_over_ref_rows_30_80_260'] = {y: ratios.get(y) for y in (30, 80, 260)}
# WP6i review fix 2: item 5 = core L / ref L in 0.65-0.88 over rows 380-540 within x 840-960, |L/ref - 1| >= 0.05 at y 150,
# never > 1.05 over sky rows (y < 60).
core = [float(rows(L, y)[840:961].mean() / rows(LR, y)[840:961].mean()) for y in range(380, 541, 10)]
c['5_core_L_over_ref_y380_540_x840_960'] = [round(min(core), 3), round(float(np.mean(core)), 3), round(max(core), 3)]
px150 = plume(150)[0]
c['5_L_over_ref_y150'] = round(float(rows(L, 150)[px150].mean() / rows(LR, 150)[px150].mean()), 3) if len(px150) else None
sky = [ratios[y] for y in ratios if y < 60]
c['5_max_L_over_ref_sky_rows'] = max(sky) if sky else None
c['5_L_over_ref_min_max_y10_560'] = [min(ratios.values()), max(ratios.values())] if ratios else None
c['5_rows_with_plume'] = sorted(ratios)
edges = np.array([float(plume(y)[0].min()) if len(plume(y)[0]) else np.nan for y in range(100, 501)])
c['6_left_edge_sd_per_100_rows'] = {f'{100 + 100 * i}-{199 + 100 * i}': round(float(np.nanstd(edges[100 * i:100 * i + 100])), 1) for i in range(4)}
dens = np.array([float(np.abs(excess(y))[np.abs(excess(y)) >= T].sum()) for y in range(100, 501)])
sm = np.convolve(dens, np.ones(15) / 15, mode='same'); peaks = []
for i in range(1, len(sm) - 1):
    if sm[i] > sm[i - 1] and sm[i] >= sm[i + 1] and (not peaks or i - peaks[-1] >= 40): peaks.append(i)
c['6_density_maxima_y'] = [100 + p for p in peaks]

# 7: motion over the 14 frames (0.3 s apart in world time, motion-times.json)
times = json.loads((HERE / 'motion-times.json').read_text())['world_time_s']
frames = [np.asarray(Image.open(HERE / f'motion-{k:02d}.png').convert('RGB'), dtype=float) for k in range(14)]
box = [f[0:561, X0:X1 + 1] for f in frames]
still = np.median(np.stack([b[..., 0] - b[..., 2] for b in box]), axis=0)   # the static scene; what moves is the smoke
def moving(f): return np.abs(f[..., 0] - f[..., 2] - still)
def rowprof(f): p = moving(f).sum(1); return p - p.mean()
def colprof(f): return moving(f).sum(0)
up, side, mad = [], [], []
for a, b, ta, tb in zip(box, box[1:], times, times[1:]):
    dt = tb - ta; ra, rbp = rowprof(a), rowprof(b)
    best = max(range(-60, 61), key=lambda s: float(np.dot(ra[max(0, s):len(ra) + min(0, s)], rbp[max(0, -s):len(rbp) + min(0, -s)])))
    up.append(best / dt)   # + = content moved up (to smaller y)
    ca, cb = colprof(a), colprof(b); xs = np.arange(len(ca))
    side.append(abs(float((cb * xs).sum() / max(cb.sum(), 1e-6) - (ca * xs).sum() / max(ca.sum(), 1e-6))) / dt)
    mad.append(float(np.abs(a - b).mean()))
c['7_upward_px_per_s_mean'] = round(float(np.mean(up)), 1); c['7_upward_px_per_s'] = [round(v, 1) for v in up]
c['7_sideways_drift_px_per_s_mean'] = round(float(np.mean(side)), 1)
c['7_consecutive_mean_abs_diff_min_mean'] = [round(min(mad), 2), round(float(np.mean(mad)), 2)]
c['7_frame_dt_s'] = [round(b - a, 3) for a, b in zip(times, times[1:])]

rb2, L2 = load('main-street.png')
m = {}
for x0, x1 in ((790, 830), (1085, 1125)):
    best = (0, None, None)
    for y in range(300, 401):
        row = rb2[y - 3:y + 4].mean(0); xs = np.arange(x0, x1 + 1)
        ex = row[x0:x1 + 1] - np.interp(xs, [x0, x1], [row[x0 - 2:x0 + 3].mean(), row[x1 - 2:x1 + 3].mean()])
        n = int((ex >= 5).sum())
        if n > best[0]:
            lrow = L2[y - 3:y + 4].mean(0); sky = np.r_[lrow[x0 - 2:x0 + 3], lrow[x1 - 2:x1 + 3]].mean()
            best = (n, y, round(float(lrow[xs[ex >= 5]].mean() / sky), 3))
    m[f'x{x0}-{x1}'] = {'max_cols_rb_ge5': best[0], 'row': best[1], 'thread_L_over_sky': best[2]}
out = {'chimney-smoke': c, 'main-street': m}
print(json.dumps(out, indent=1))
(HERE / 'plume-measure.json').write_text(json.dumps(out, indent=1) + '\n')
