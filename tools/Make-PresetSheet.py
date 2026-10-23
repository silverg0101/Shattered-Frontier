"""Contact sheet from raw renders: python Make-PresetSheet.py <out.png> <cols> <name...>  (reads presets/raw/<name>.png)"""
import sys
from PIL import Image, ImageDraw, ImageFont
RAW = r"C:\Projects\SF-wt-ui\GameClient\ArtSource\Characters\Player\presets\raw"
out, cols, names = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
W, H = 400, 480
font = ImageFont.truetype("arialbd.ttf", 28)
rows = -(-len(names) // cols)
sheet = Image.new("RGB", (cols * W, rows * H), (20, 20, 22))
for i, n in enumerate(names):
    im = Image.open(f"{RAW}/{n}.png").convert("RGB")
    w, h = im.size  # crop a head-and-shoulders window then fit the cell
    im = im.crop((60, 200, w - 60, h - 40)).resize((W, H - 0), Image.LANCZOS)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, 8 + 14 * len(n.split("_front")[0]), 40), fill=(0, 0, 0))
    d.text((6, 4), n.replace("_front", ""), fill=(255, 255, 255), font=font)
    sheet.paste(im, ((i % cols) * W, (i // cols) * H))
sheet.save(out)
