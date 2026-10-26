"""Build a run_pixal_mv.py views folder from four ready 1024x1024 RGBA views (real alpha, no matting).

usage: assemble_rgba_views.py --views FRONT LEFT BACK RIGHT --transforms <transforms.json> --output <new folder>
Rejects images that are not RGBA squares or whose four corners are not fully transparent.
"""
import argparse, hashlib, json, shutil
from pathlib import Path
from PIL import Image

NAMES = ['view00_azim000', 'view01_azim090', 'view02_azim180', 'view03_azim270']
p = argparse.ArgumentParser()
p.add_argument('--views', nargs=4, required=True)
p.add_argument('--transforms', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
out = Path(a.output)
if out.exists(): raise FileExistsError(out)
imgs = [Image.open(v) for v in a.views]
for v, im in zip(a.views, imgs):
    w, h = im.size
    if im.mode != 'RGBA' or w != h: raise ValueError(f'{v}: need a square RGBA PNG, got {im.mode} {im.size}')
    if any(im.getpixel(c)[3] for c in [(0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)]): raise ValueError(f'{v}: corners are not transparent')
out.mkdir(parents=True)
for n, im in zip(NAMES, imgs): im.save(out / f'{n}.png')
shutil.copyfile(a.transforms, out / 'transforms.json')
(out / 'inputs.json').write_text(json.dumps({'inputs': [{'path': v, 'sha256': hashlib.sha256(Path(v).read_bytes()).hexdigest()} for v in a.views]}, indent=2))
