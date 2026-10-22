"""WP6g review acceptance numbers 1-7 and the street thread, measured on chimney-smoke.png, motion-00..13.png and
main-street.png (numbers only, no verdict). Writes plume-measure.json.

Plume pixel (review definition): over sky, the 7-row-averaged R-B exceeds the sky interpolated between x 760 and x 1100
by more than 4; over field rows (y >= 440) the luminance exceeds the same-row field fit at x 560-700 by more than 3.
Item 8 (no rectangle / straight edge > 100 px) is left to the reviewer's eye.
"""
import json
from pathlib import Path
import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
X0, X1 = 760, 1100


def load(name):
    a = np.asarray(Image.open(HERE / name).convert('RGB'), dtype=float)
    return a[..., 0] - a[..., 2], .2126 * a[..., 0] + .7152 * a[..., 1] + .0722 * a[..., 2]


def rb_excess(rb, y, k=3):
    row = rb[max(0, y - k):y + k + 1].mean(0); xs = np.arange(X0, X1 + 1)
    base = np.interp(xs, [X0, X1], [row[X0 - 2:X0 + 3].mean(), row[X1 - 2:X1 + 3].mean()])
    return xs, row[X0:X1 + 1] - base


def field_excess(L, y, x0=700, x1=X1):
    f = np.arange(560, 701); c = np.polyfit(f, L[y, 560:701], 1); xs = np.arange(x0, x1 + 1)
    return xs, L[y, x0:x1 + 1] - np.polyval(c, xs)


def plume(rb, L, y):   # (xs of plume pixels, excess used)
    if y >= 440:
        xs, ex = field_excess(L, y); return xs[ex > 3], ex
    xs, ex = rb_excess(rb, y); return xs[ex > 4], ex


def span(px):
    return None if len(px) == 0 else dict(x0=int(px.min()), x1=int(px.max()), span=int(px.max() - px.min() + 1), centre=round(float((px.min() + px.max()) / 2), 1))


rb, L = load('chimney-smoke.png')
c = {}
s550, s150 = span(plume(rb, L, 550)[0]), span(plume(rb, L, 150)[0])
c['1_origin_y550'] = s550
c['2_growth_y150'] = s150; c['2_ratio_150_over_550'] = round(s150['span'] / s550['span'], 2) if s550 and s150 else None
veil = []
for y in range(460, 541):
    xs, ex = field_excess(L, y, 560, X1); hit = xs[ex > 3]
    veil.append((int(len(hit)), int(hit.min()) if len(hit) else None))
c['3_max_field_cols_460_540'] = max(v[0] for v in veil); c['3_leftmost_x_460_540'] = min((v[1] for v in veil if v[1] is not None), default=None)
c['4_peak_rb_380_540'] = round(max(float(rb_excess(rb, y)[1].max()) for y in range(380, 541)), 1)
c['4_peak_rb_y100'] = round(float(rb_excess(rb, 100)[1].max()), 1)
ratios = {}
for y in list(range(10, 431, 10)):
    xs, ex = rb_excess(rb, y); row = L[y - 3:y + 4].mean(0)
    sky = np.r_[row[X0 - 2:X0 + 3], row[X1 - 2:X1 + 3]].mean(); px = xs[ex > 4]
    if len(px): ratios[y] = round(float(row[px].mean() / sky), 3)
c['5_L_ratio_rows_30_80_260'] = {y: ratios.get(y) for y in (30, 80, 260)}
c['5_max_L_ratio_y10_430'] = max(ratios.values()) if ratios else None
edges = []
for y in range(100, 501):
    px = plume(rb, L, y)[0]; edges.append(int(px.min()) if len(px) else np.nan)
edges = np.array(edges, float)
c['6_left_edge_sd_per_100_rows'] = {f'{100 + 100 * i}-{199 + 100 * i}': round(float(np.nanstd(edges[100 * i:100 * i + 100])), 1) for i in range(4)}
dens = np.array([float(np.clip(plume(rb, L, y)[1], 0, None).sum()) for y in range(100, 501)])
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
