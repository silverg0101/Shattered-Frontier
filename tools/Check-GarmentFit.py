"""Measure dress-form containment and outward silhouette clearance for depth-guided garment views.

  py -3 tools/Check-GarmentFit.py --slot head --name GEAR02-hood [--views DIR --depth-dir DIR --out DIR]

Uses the renderer's proxy and bare-body masks. Distances refer to the external left/right garment outline
in rows occupied by the body, so the deliberately open face and hem do not count as a poor fit.
"""
import argparse
import json
import os

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as ndi

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ap = argparse.ArgumentParser()
ap.add_argument('--slot', default='head'); ap.add_argument('--name', required=True)
ap.add_argument('--views', default=os.path.join(ROOT, 'Artifacts/GEAR02/views'))
ap.add_argument('--depth-dir', default=os.path.join(ROOT, 'Artifacts/GEAR02/depth'))
ap.add_argument('--out', default=os.path.join(ROOT, 'Artifacts/GEAR02/fit'))
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
meta = json.load(open(os.path.join(a.depth_dir, f'{a.slot}_meta.json')))
clearance_cm = float(meta['clearance_cm'])
views = ('front', 'left', 'right')
res = {}
sheet = Image.new('RGB', (1024 * 3, 1050), 'white')
draw = ImageDraw.Draw(sheet)
for col, v in enumerate(views):
    picture = Image.open(os.path.join(a.views, f'{a.name}-{v}.png')).convert('RGB')
    img = np.asarray(picture).astype(np.int16)
    g = ndi.binary_fill_holes(ndi.binary_opening(img.min(2) < 225, iterations=1))
    proxy = np.asarray(Image.open(os.path.join(a.depth_dir, f'mask_{a.slot}_{v}.png')).convert('L')) > 127
    body = np.asarray(Image.open(os.path.join(a.depth_dir, f'body_mask_{a.slot}_{v}.png')).convert('L')) > 127
    if g.shape != proxy.shape or body.shape != proxy.shape or not g.any() or not proxy.any() or not body.any():
        raise ValueError(f'{v}: missing subject or mismatched masks')
    gap_px = []
    for row in range(g.shape[0]):
        bx = np.flatnonzero(body[row]); gx = np.flatnonzero(g[row])
        if len(bx) < 5 or len(gx) < 5 or bx[0] == 0 or bx[-1] == g.shape[1] - 1:
            continue
        gap_px.extend((bx[0] - gx[0], gx[-1] - bx[-1]))
    if not gap_px:
        raise ValueError(f'{v}: no comparable outline rows')
    gap = np.array(gap_px) * meta['size_m'] * 100 / g.shape[1]
    uncovered = int((proxy & ~g).sum())
    rows = np.flatnonzero(g.any(1))
    tracing = bool(gap.mean() <= clearance_cm or (np.abs(gap) < clearance_cm / 2).mean() > .1)
    res[v] = dict(proxy_contained=uncovered == 0,
                  proxy_coverage=round(1 - uncovered / int(proxy.sum()), 4),
                  uncovered_proxy_px=uncovered,
                  mean_outline_gap_cm=round(float(gap.mean()), 2),
                  near_zero_trace_fraction=round(float((np.abs(gap) < clearance_cm / 2).mean()), 3),
                  tracing_flag=tracing,
                  top=int(rows.min()), bottom=int(rows.max()), height=int(rows.max() - rows.min() + 1))
    overlay = np.full(img.shape, 255, np.uint8)
    overlay[g & proxy] = (120, 170, 120)
    overlay[g & ~proxy] = (230, 190, 60)
    overlay[proxy & ~g] = (220, 50, 50)
    Image.fromarray(overlay).save(os.path.join(a.out, f'overlay_{a.slot}_{v}.png'))
    sheet.paste(picture, (1024 * col, 26))
    draw.text((1024 * col + 12, 6), f'{v}: {res[v]["mean_outline_gap_cm"]:.2f} cm gap, {res[v]["proxy_coverage"]:.1%} proxy', fill='black')
heights = [res[v]['height'] for v in views]
res['cross_view_height_spread'] = round((max(heights) - min(heights)) / float(np.mean(heights)), 3)
res['clearance_cm'] = clearance_cm
res['passes'] = all(res[v]['proxy_contained'] and not res[v]['tracing_flag'] for v in views) and res['cross_view_height_spread'] <= .05
sheet.save(os.path.join(a.out, 'sheet.png'))
with open(os.path.join(a.out, 'fit.json'), 'w') as f:
    json.dump(res, f, indent=2)
print(json.dumps(res, indent=2))
