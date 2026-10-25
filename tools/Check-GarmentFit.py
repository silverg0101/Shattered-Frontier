"""Fit of generated garment views against the body silhouette (masks from Render-BodyDepthViews.py): overlay images and numbers.
  py -3 tools/Check-GarmentFit.py --slot head --name GEAR02-hood [--views DIR --depth-dir DIR --out DIR]
coverage = share of the body mask the garment covers; gap = share left bare; overshoot = share of the garment outside the body
mask (a loose garment is fine; the body mask is already grown by the slot's cloth thickness); top/bottom = rows of the garment
per view, which must agree across views (same scale and framing).
"""
import argparse, json, os
import numpy as np
from PIL import Image
from scipy import ndimage as ndi  # noqa

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ap = argparse.ArgumentParser()
ap.add_argument('--slot', default='head'); ap.add_argument('--name', required=True)
ap.add_argument('--views', default=os.path.join(ROOT, 'Artifacts/GEAR02/views'))
ap.add_argument('--depth-dir', default=os.path.join(ROOT, 'Artifacts/GEAR02/depth'))
ap.add_argument('--out', default=os.path.join(ROOT, 'Artifacts/GEAR02/fit'))
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
res = {}
for v in ('front', 'left', 'right'):
    img = np.asarray(Image.open(os.path.join(a.views, f'{a.name}-{v}.png')).convert('RGB')).astype(int)
    g = ndi.binary_fill_holes(ndi.binary_opening(img.min(2) < 225, iterations=1))
    b = np.asarray(Image.open(os.path.join(a.depth_dir, f'mask_{a.slot}_{v}.png')).convert('L')) > 127
    inter = g & b
    # the slot's lower edge is a hard cut and the frame clips the arms: score the body mask only above the garment's own lowest row
    rows = np.nonzero(g.any(1))[0]
    res[v] = dict(coverage=round(inter.sum() / b.sum(), 3), gap=round((b & ~g).sum() / b.sum(), 3),
                  overshoot=round((g & ~b).sum() / g.sum(), 3), top=int(rows.min()), bottom=int(rows.max()),
                  body_top=int(np.nonzero(b.any(1))[0].min()))
    o = np.zeros(img.shape, np.uint8)
    o[...] = 255
    o[g & b] = (120, 170, 120)   # green: garment over body
    o[g & ~b] = (230, 190, 60)   # amber: garment beyond the body mask
    o[b & ~g] = (220, 50, 50)    # red: body bare
    Image.fromarray(np.where((g | b)[..., None], o, (255, 255, 255)).astype(np.uint8)).save(os.path.join(a.out, f'overlay_{a.slot}_{v}.png'))
print(json.dumps(res, indent=1))
