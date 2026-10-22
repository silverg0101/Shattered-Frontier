"""Rib-angle coherence R (r7 review metric): 3 px Gaussian high-pass of luminance, structure tensor per block, R = |mean(exp(2i*theta))| over blocks.
1 = every block's texture runs at one angle (a lattice), ~0 = random. Run: py -3.12 rib_angle.py <image> x y w h <block px>"""
import sys
import numpy as np
from PIL import Image, ImageFilter
x, y, w, h, b = map(int, sys.argv[2:7])
im = Image.open(sys.argv[1]).convert('L').crop((x, y, x + w, y + h))
a = np.asarray(im, dtype=np.float64) - np.asarray(im.filter(ImageFilter.GaussianBlur(3)), dtype=np.float64)
gy, gx = np.gradient(a)
z = []
for by in range(0, h - b + 1, b):
    for bx in range(0, w - b + 1, b):
        sx, sy = gx[by:by + b, bx:bx + b], gy[by:by + b, bx:bx + b]
        jxx, jyy, jxy = (sx * sx).sum(), (sy * sy).sum(), (sx * sy).sum()
        z.append(np.exp(1j * np.arctan2(2 * jxy, jxx - jyy)))
print(f'RIB_R {sys.argv[1]} [{x},{y},{w},{h}] block {b} blocks {len(z)} R {abs(np.mean(z)):.2f}')
