"""Contact sheet: python tools/Montage.py out.png h tile.png [tile.png ...]  (tiles scaled to height h, RGBA over magenta)"""
import sys
from PIL import Image
out, h, *files = sys.argv[1:]; h = int(h)
ims = []
for f in files:
    i = Image.open(f).convert('RGBA'); ims.append(i.resize((round(i.width * h / i.height), h)))
s = Image.new('RGB', (sum(i.width for i in ims), h), (255, 0, 255)); x = 0
for i in ims: s.paste(i, (x, 0), i); x += i.width
s.save(out)
