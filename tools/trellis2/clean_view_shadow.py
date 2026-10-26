"""Remove contact-shadow slivers from a matted RGBA view: grey (low-saturation) pixels in the bottom band lose alpha,
then connected alpha components smaller than --min-area are dropped. usage: clean_view_shadow.py in.png out.png"""
import argparse, cv2, numpy as np
from PIL import Image
p = argparse.ArgumentParser(); p.add_argument('src'); p.add_argument('dst')
p.add_argument('--band', type=float, default=0.84, help='rows below this fraction of image height are cleaned')
p.add_argument('--chroma', type=int, default=16, help='max(rgb)-min(rgb) below this counts as grey')
p.add_argument('--min-area', type=int, default=400)
p.add_argument("--bright", type=int, default=95, help="only pixels brighter than this (all channels) count as shadow; dark leather stays")
a = p.parse_args()
im = np.array(Image.open(a.src).convert('RGBA')); h = im.shape[0]
rgb = im[..., :3].astype(int); grey = ((rgb.max(-1) - rgb.min(-1)) < a.chroma) & (rgb.min(-1) > a.bright)
band = np.zeros(grey.shape, bool); band[int(h * a.band):] = True
im[grey & band, 3] = 0
n, lab, st, _ = cv2.connectedComponentsWithStats((im[..., 3] > 20).astype(np.uint8), connectivity=8)
for i in range(1, n):
    if st[i, cv2.CC_STAT_AREA] < a.min_area: im[lab == i, 3] = 0
im[im[..., 3] == 0, :3] = 0
Image.fromarray(im).save(a.dst)
