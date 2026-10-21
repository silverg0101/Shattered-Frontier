"""Build labelled concept-left / live-right evidence pairs from final HighResShot frames."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageOps

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[3]
hud = ROOT / 'docs/Game-Design/concept-art/UI/v5/gameplay-hud.png'
board = ROOT / 'docs/Game-Design/concept-art/Silverbrook-Design-Board-v1.webp'

def panel(path, crop=None):
    im = Image.open(path).convert('RGB')
    if crop: im = im.crop(crop)
    return ImageOps.fit(im, (960, 540), method=Image.Resampling.LANCZOS)

def pair(name, reference, reference_label, suffix, crop=None):
    canvas = Image.new('RGB', (1920, 580), '#171713')
    canvas.paste(panel(reference, crop), (0, 40))
    canvas.paste(panel(OUT / (name + '.png')), (960, 40))
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 23)
    draw.text((14, 8), reference_label, font=font, fill='white')
    draw.text((974, 8), f'LIVE — {name}', font=font, fill='white')
    canvas.save(OUT / f'{name}-vs-{suffix}.jpg', quality=86, subsampling=0)

for view in ('main-street', 'market', 'house-frontage'):
    pair(view, hud, 'TARGET — gameplay HUD v5, town background', 'hud')
    pair(view, board, 'GFX-01 — Silverbrook design board', 'gfx01', (845, 48, 1773, 570))
