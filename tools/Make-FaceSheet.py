"""Label and tile Saved/Screenshots face_<id>.png (Capture-PlayerFace.py) into one jpg: py tools/Make-FaceSheet.py <out.jpg> [shots dir]."""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

IDS = 'Godric Aelfwyn Edwin Hilda Orlando Wulfric Eadgyth Osric Ebba'.split()
out, src = sys.argv[1], Path(sys.argv[2] if len(sys.argv) > 2 else 'GameClient/Saved/Screenshots')
files = {i: next(iter(sorted(src.rglob(f'face_{i}*.png'))[-1:]), None) for i in IDS}
S = 640  # tile edge: square crop around the frame centre
tiles = []
for i in IDS:
    im = Image.open(files[i]).convert('RGB')
    w, h = im.size
    m = min(w, h)
    im = im.crop(((w - m) // 2, (h - m) // 2, (w + m) // 2, (h + m) // 2)).resize((S, S), Image.LANCZOS)
    ImageDraw.Draw(im).text((12, 8), i, fill='white', stroke_width=3, stroke_fill='black', font=ImageFont.truetype('arial.ttf', 40))
    tiles.append(im)
sheet = Image.new('RGB', (S * 3, S * 3))
for k, im in enumerate(tiles):
    sheet.paste(im, (k % 3 * S, k // 3 * S))
sheet.save(out, quality=90)
