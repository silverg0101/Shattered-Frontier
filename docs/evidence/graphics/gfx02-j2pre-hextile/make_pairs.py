"""Labelled before/after pairs beside gameplay-hud.png's far range (GFX-02 J2-pre). Run: py -3.12 make_pairs.py

A1-before-after-compare.png: concept far-range crop | A1 before | A1 after (three 640 x 360 panels).
<view>-before-after-compare.png for A2, A3, 01, 04-sun20: before | after (two 960 x 540 panels).
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageOps

OUT = Path(__file__).resolve().parent
hud = OUT.parents[3] / 'docs/Game-Design/concept-art/UI/v5/gameplay-hud.png'
font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 22)


def panel(img, size, box=None):
    img = Path(img); img = img if img.exists() else img.with_suffix('.jpg')
    im = Image.open(img).convert('RGB')
    if box: im = im.crop(box)
    return ImageOps.fit(im, size, method=Image.Resampling.LANCZOS)


def sheet(name, panels, size):
    canvas = Image.new('RGB', (size[0] * len(panels), size[1] + 36), '#171713'); d = ImageDraw.Draw(canvas)
    for i, (label, img, box) in enumerate(panels):
        canvas.paste(panel(img, size, box), (i * size[0], 36)); d.text((i * size[0] + 10, 6), label, font=font, fill='white')
    canvas.save(OUT / name)


sheet('A1-hud-before-after-compare.png', [('TARGET: gameplay-hud.png far range', hud, (830, 180, 1450, 530)),
                                          ('BEFORE: A1 south approach 30 m', OUT / 'before/A1-south-approach-30m.png', None),
                                          ('AFTER: A1 (hex-tiled ground)', OUT / 'after/A1-south-approach-30m.png', None)], (640, 360))
for v in ('A2-south-approach-100m', 'A3-south-approach-250m', '01-forest-path-town', '04-forest-floor-sun20'):
    sheet(f'{v}-before-after-compare.png', [(f'BEFORE {v}', OUT / f'before/{v}.png', None), (f'AFTER {v}', OUT / f'after/{v}.png', None)], (960, 540))
