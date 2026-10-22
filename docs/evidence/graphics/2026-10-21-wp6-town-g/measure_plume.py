"""Measure the WP6f review fix-1 acceptance numbers on chimney-smoke.png and main-street.png (numbers only, no verdict)."""
import json
from pathlib import Path
import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent

def load(name):
    a = np.asarray(Image.open(HERE / name).convert('RGB'), dtype=float)
    return a, a[..., 0] - a[..., 2], .2126*a[..., 0] + .7152*a[..., 1] + .0722*a[..., 2]

def rb_row(rb, y, x0, x1, k=3):  # 7-row-averaged R-B, excess over the background interpolated linearly between x0 and x1
    row = rb[y-k:y+k+1].mean(0)
    xs = np.arange(x0, x1+1)
    base = np.interp(xs, [x0, x1], [row[x0-2:x0+3].mean(), row[x1-2:x1+3].mean()])
    return xs, row[x0:x1+1] - base

def span(xs, ex, t):
    hit = xs[ex > t]
    return (int(hit.min()), int(hit.max()), int(hit.max()-hit.min()+1), int(len(hit))) if len(hit) else None

def field(L, y, x0=700, x1=990):  # x > ~990 at y 440-520 is the next roof and its stack, not field  # L excess over the same-row field at x 560-700 fitted linearly
    f = np.arange(560, 701); c = np.polyfit(f, L[y, 560:701], 1)
    xs = np.arange(x0, x1+1); return xs, L[y, x0:x1+1] - np.polyval(c, xs)

out = {}
_, rb, L = load('chimney-smoke.png')
c = {}
for y in (10, 30, 60, 80, 100, 150, 260, 400, 430, 460, 490, 520):
    xs, ex = rb_row(rb, y, 820, 1070 if y < 400 else 980)  # below y 400 the x 1000-1070 edge is the next roof/stack
    c[f'y{y}'] = {'rb_span_gt4': span(xs, ex, 4), 'rb_peak_excess': round(float(ex.max()), 1), 'rb_peak_x': int(xs[ex.argmax()])}
    if y >= 440:
        fx, fe = field(L, y); c[f'y{y}']['field_L_gt3'] = span(fx, fe, 3)
c['peak_rb_400_520'] = round(max(float(rb_row(rb, y, 820, 980)[1].max()) for y in range(400, 521)), 1)
c['max_field_cols_460_520'] = max(span(*field(L, y), 3)[3] if span(*field(L, y), 3) else 0 for y in range(460, 521))
for y in (30, 80, 260):  # plume L / sky L: plume columns (R-B excess > 4) against the two background windows
    xs, ex = rb_row(rb, y, 820, 1070); row = L[y-3:y+4].mean(0)
    sky = np.r_[row[818:823], row[1068:1073]].mean(); pl = row[xs[ex > 4]].mean() if (ex > 4).any() else float('nan')
    c[f'L_ratio_y{y}'] = round(float(pl/sky), 3)
out['chimney-smoke'] = c
_, rb, L = load('main-street.png')
m = {}
for x0, x1 in ((790, 830), (1085, 1125)):
    best = (0, None)
    for y in range(300, 401):
        xs, ex = rb_row(rb, y, x0, x1)
        n = int((ex >= 5).sum()); best = max(best, (n, y), key=lambda t: t[0])
    m[f'x{x0}-{x1}'] = {'max_cols_rb_ge5': best[0], 'row': best[1]}
out['main-street'] = m
print(json.dumps(out, indent=1))
(HERE / 'plume-measure.json').write_text(json.dumps(out, indent=1) + '\n')
