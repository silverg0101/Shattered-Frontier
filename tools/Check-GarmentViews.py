"""GEAR-02 gate before an Asset Forge 3d job: no background pixels enclosed by the garment in a view (a face opening that shows white
reads as a tunnel through the hood to Pixal3D). A background pixel not reachable from the image border over background is enclosed.
  blender --factory-startup -b --python tools/Check-GarmentViews.py -- front.png [left.png ...] [--max-px 200]
Exit 1 and prints the enclosed pixel count (at 256 px) per view when over --max-px (default 40: specks only).
"""
import sys
from collections import deque

import bpy
import numpy as np

args = sys.argv[sys.argv.index('--') + 1:]
limit = int(args[args.index('--max-px') + 1]) if '--max-px' in args else 40
paths = [a for i, a in enumerate(args) if a.endswith('.png')]
bad = False
for p in paths:
    im = bpy.data.images.load(__import__('os').path.abspath(p))
    w, h = im.size
    px = np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4)[:, :, :3]
    px = px[::w // 256, ::w // 256][:256, :256]  # 256 px is plenty for a hole of a face's size
    bg = (px.min(axis=2) > 0.93)
    n = bg.shape[0]
    seen = np.zeros_like(bg)
    q = deque((y, x) for y in range(n) for x in (0, n - 1) if bg[y, x]); q.extend((y, x) for x in range(n) for y in (0, n - 1) if bg[y, x])
    for y, x in q:
        seen[y, x] = True
    while q:
        y, x = q.popleft()
        for yy, xx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
            if 0 <= yy < n and 0 <= xx < n and bg[yy, xx] and not seen[yy, xx]:
                seen[yy, xx] = True; q.append((yy, xx))
    enclosed = int((bg & ~seen).sum())
    print(f'VIEWCHECK {p} enclosed_background_px={enclosed} {"FAIL" if enclosed > limit else "ok"}')
    bad |= enclosed > limit
sys.exit(1 if bad else 0)
