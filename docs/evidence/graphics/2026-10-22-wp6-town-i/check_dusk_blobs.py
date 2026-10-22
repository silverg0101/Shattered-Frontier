"""Dusk capture gate (WP6i review r7 fix 2): no bright blob outside a lantern or window pane.

py -3 check_dusk_blobs.py   (beside the captures; reads dusk-panes.json)
For each dusk view: label connected pixels with L > 200 (L = 0.2126 R + 0.7152 G + 0.0722 B); a component of >= 20 px
whose bbox is not inside a pane box grown by 10 px is "outside". Also the orange test: R > B + 80 and L > 150, >= 20 px,
outside the panes. Prints DUSK_BLOBS <view> outside <n> largest <x0,x1,y0,y1,count,L> and DUSK_ORANGE <view> outside <n>;
exits 1 when any view has an outside blob.
"""
import json, sys
from pathlib import Path
import numpy as np
from PIL import Image
from scipy import ndimage as nd

HERE = Path(__file__).resolve().parent
PANES = json.loads((HERE / 'dusk-panes.json').read_text(encoding='utf-8'))
WARM = {'house-frontage-dusk': 3000, 'context-lamp-wall-dusk': 3000, 'context-house-side-b-dusk': 2000}   # REVIEW-r8 fix 1 bands


def outside(mask, L, boxes):
    lab, k = nd.label(mask)
    out = []
    for i, sl in enumerate(nd.find_objects(lab), 1):
        n = int((lab[sl] == i).sum())
        if n < 20:
            continue
        x0, x1, y0, y1 = sl[1].start, sl[1].stop, sl[0].start, sl[0].stop
        if any(x0 >= b[0] - 10 and x1 <= b[1] + 10 and y0 >= b[2] - 10 and y1 <= b[3] + 10 for b in boxes):
            continue
        out.append((x0, x1, y0, y1, n, round(float(L[sl][lab[sl] == i].mean()))))
    return sorted(out, key=lambda o: -o[4])


bad = 0
for view, p in PANES.items():
    if view.startswith('_'):
        continue
    a = np.asarray(Image.open(HERE / f'{view}.png').convert('RGB'), dtype=float)
    L = .2126 * a[..., 0] + .7152 * a[..., 1] + .0722 * a[..., 2]
    boxes = p['lantern'] + p['windows']
    o = outside(L > 200, L, boxes); orange = outside((a[..., 0] > a[..., 2] + 80) & (L > 150), L, boxes)
    print('DUSK_BLOBS', view, 'outside', len(o), 'largest', o[0] if o else None)
    print('DUSK_ORANGE', view, 'outside', len(orange), 'largest', orange[0] if orange else None)
    bad += len(o)
    # DUSK_WASH (r8 fix 3): the subject lantern (first lantern box) must light its surroundings without clipping: pixels within
    # 150 px of the pane centre and outside the pane bbox grown by 10 px, warm = R >= B + 10 and L >= 30, hot = L > 235.
    x0, x1, y0, y1 = p['lantern'][0]; cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    yy, xx = np.mgrid[0:L.shape[0], 0:L.shape[1]]
    ring = ((xx - cx) ** 2 + (yy - cy) ** 2 <= 150 ** 2) & ~((xx >= x0 - 10) & (xx <= x1 + 10) & (yy >= y0 - 10) & (yy <= y1 + 10))
    warm = ring & (a[..., 0] >= a[..., 2] + 10) & (L >= 30); hot = ring & (L > 235)
    need = WARM.get(view, 0)
    print('DUSK_WASH', view, 'warm_px', int(warm.sum()), 'need', need, 'hot_px', int(hot.sum()),
          'warm_x1250_1455', int(warm[:, 1250:1456].sum()) if view == 'context-house-side-b-dusk' else '-')
    bad += int(warm.sum() < need) + int(hot.sum() > 0)
sys.exit(1 if bad else 0)
