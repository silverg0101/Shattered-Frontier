"""Dusk capture gate (WP6i reviews r7-r9): no bright blob outside a lantern or window pane, a lit pool, a dark lantern cap.

py -3 check_dusk_blobs.py   (beside the captures; reads dusk-panes.json)
For each dusk view: label connected pixels with L > 200 (L = 0.2126 R + 0.7152 G + 0.0722 B); a component of >= 20 px
whose bbox is not inside a pane box grown by 10 px is "outside". Orange test: R > B + 80 and L > 200, >= 20 px, outside
the panes. DUSK_WASH: within 150 px of the subject lantern (first lantern box), outside every pane and window box grown by
10 px, warm = R >= B + 10 and L >= 30 (>= WARM[view]) and hot = L > 235 (0). DUSK_CAP: p90 L of each cap box <= 120.
Exits 1 on any failure.
"""
import json, sys
from pathlib import Path
import numpy as np
from PIL import Image
from scipy import ndimage as nd

HERE = Path(__file__).resolve().parent
PANES = json.loads((HERE / 'dusk-panes.json').read_text(encoding='utf-8'))
# REVIEW-r9 fix 2: side-b's lantern lights a face the eye cannot see; 50 is the soffit glint and the stoop riser.
WARM = {'house-frontage-dusk': 3000, 'context-lamp-wall-dusk': 3000, 'context-house-side-b-dusk': 50}


def inside(x0, x1, y0, y1, boxes):
    return any(x0 >= b[0] - 10 and x1 <= b[1] + 10 and y0 >= b[2] - 10 and y1 <= b[3] + 10 for b in boxes)


def outside(mask, L, boxes):
    lab, k = nd.label(mask)
    out = []
    for i, sl in enumerate(nd.find_objects(lab), 1):
        n = int((lab[sl] == i).sum())
        if n < 20 or inside(sl[1].start, sl[1].stop, sl[0].start, sl[0].stop, boxes):
            continue
        out.append((sl[1].start, sl[1].stop, sl[0].start, sl[0].stop, n, round(float(L[sl][lab[sl] == i].mean()))))
    return sorted(out, key=lambda o: -o[4])


def check(view, a, p):
    L = .2126 * a[..., 0] + .7152 * a[..., 1] + .0722 * a[..., 2]
    boxes = p['lantern'] + p['windows']
    o = outside(L > 200, L, boxes); orange = outside((a[..., 0] > a[..., 2] + 80) & (L > 200), L, boxes)
    print('DUSK_BLOBS', view, 'outside', len(o), 'largest', o[0] if o else None)
    print('DUSK_ORANGE', view, 'outside', len(orange), 'largest', orange[0] if orange else None)
    x0, x1, y0, y1 = p['lantern'][0]; cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    yy, xx = np.mgrid[0:L.shape[0], 0:L.shape[1]]
    panes = np.zeros(L.shape, bool)
    for b in boxes:
        panes[max(b[2] - 10, 0):b[3] + 11, max(b[0] - 10, 0):b[1] + 11] = True
    ring = ((xx - cx) ** 2 + (yy - cy) ** 2 <= 150 ** 2) & ~panes
    warm = ring & (a[..., 0] >= a[..., 2] + 10) & (L >= 30); hot = ring & (L > 235)
    print('DUSK_WASH', view, 'warm_px', int(warm.sum()), 'need', WARM.get(view, 0), 'hot_px', int(hot.sum()))
    bad = len(o) + len(orange) + int(warm.sum() < WARM.get(view, 0)) + int(hot.sum() > 0)
    for c in p.get('cap', []):
        p90 = float(np.percentile(L[c[2]:c[3], c[0]:c[1]], 90))
        print('DUSK_CAP', view, c, 'p90', round(p90, 1)); bad += int(p90 > 120)
    return bad


if __name__ == '__main__':
    bad = 0
    for view, p in PANES.items():
        if not view.startswith('_'):
            bad += check(view, np.asarray(Image.open(HERE / f'{view}.png').convert('RGB'), dtype=float), p)
    if '--selftest' in sys.argv:   # an L 230 orange patch on a wall must fail the orange test
        p = PANES['house-frontage-dusk']; a = np.asarray(Image.open(HERE / 'house-frontage-dusk.png').convert('RGB'), dtype=float).copy()
        a[600:620, 100:140] = (255, 225, 120)
        L = .2126 * a[..., 0] + .7152 * a[..., 1] + .0722 * a[..., 2]
        n = len(outside((a[..., 0] > a[..., 2] + 80) & (L > 200), L, p['lantern'] + p['windows']))
        print('DUSK_SELFTEST orange patch caught', n >= 1); assert n >= 1
    sys.exit(1 if bad else 0)
