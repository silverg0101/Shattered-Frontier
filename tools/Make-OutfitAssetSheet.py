"""GEAR-01 sheet: one row per face, face_<id>.png then outfit_<id>_{front,three_quarter,side}.png (Capture-PlayerFace/Outfit.py).

py tools/Make-OutfitAssetSheet.py <out.jpg> <id,id,...> [shots dir]   (missing shots are left grey)
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw

out, ids = sys.argv[1], [i for i in sys.argv[2].split(',') if i]
src = Path(sys.argv[3] if len(sys.argv) > 3 else 'GameClient/Saved/Screenshots')
S = 480
COLS = ['face_{}', 'outfit_{}_front', 'outfit_{}_three_quarter', 'outfit_{}_side']
sheet = Image.new('RGB', (S * len(COLS), S * len(ids)), (60, 60, 60))
draw = ImageDraw.Draw(sheet)
for r, i in enumerate(ids):
    for c, pat in enumerate(COLS):
        f = next(iter(sorted(src.rglob(pat.format(i) + '*.png'))[-1:]), None)
        if f:
            im = Image.open(f).convert('RGB'); w, h = im.size; m = min(w, h)
            # outfit shots: a square around the centre, tall enough for the full figure
            sheet.paste(im.crop(((w - m) // 2, (h - m) // 2, (w + m) // 2, (h + m) // 2)).resize((S, S), Image.LANCZOS), (c * S, r * S))
        draw.text((c * S + 8, r * S + 8), f"{i} {pat.split('_', 1)[0] if c == 0 else pat[10:]}", fill=(255, 255, 0))
Path(out).parent.mkdir(parents=True, exist_ok=True)
sheet.save(out, quality=90)
print(out)
