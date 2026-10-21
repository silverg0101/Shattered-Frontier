"""Side-by-side sheets (concept | live) and kit-edge close-ups for docs/evidence/ui-v5 from raw window screenshots.
Usage: py compare_v5.py <raw-dir>   (expects b0/build/inv/market/trade .png from drive.ps1)"""
import os
import sys

from PIL import Image

S = sys.argv[1]
C = 'docs/Game-Design/concept-art/UI/v5/'
OUT = 'docs/evidence/ui-v5/'
pairs = {'build-mode': ('build.png', 'build-mode.png'), 'inventory': ('inv.png', 'inventory.png'), 'market-board': ('market.png', 'market-board.png'),
         'trade': ('trade.png', 'trade.png'), 'gameplay-hud': ('b0.png', 'gameplay-hud.png')}
for name, (live, concept) in pairs.items():
    im = Image.open(os.path.join(S, live)).convert('RGB'); w, h = im.size
    # the window screenshot includes the title bar and borders; crop to the client area and scale to the concept size
    cl = im.crop((int(w * 0.0093), int(h * 0.042), int(w * 0.9907), int(h * 0.99))).resize((1672, 941), Image.LANCZOS)
    cl.save(OUT + name + '-live.png')
    cc = Image.open(C + concept).convert('RGB').resize((1672, 941), Image.LANCZOS)
    sheet = Image.new('RGB', (1672 * 2 + 20, 941), (20, 20, 20)); sheet.paste(cc, (0, 0)); sheet.paste(cl, (1692, 0)); sheet.save(OUT + name + '-compare.png')
b, k, m = (Image.open(os.path.join(S, f)).convert('RGB') for f in ('b0.png', 'build.png', 'market.png'))
parts = [b.crop((1000, 1440, 1900, 1620)), k.crop((120, 600, 1300, 1000)), m.crop((1880, 1130, 2400, 1280))]
sheet = Image.new('RGB', (max(p.width for p in parts), sum(p.height for p in parts) + 20), (20, 20, 20)); y = 0
for p in parts:
    sheet.paste(p, (0, y)); y += p.height + 10
sheet.save(OUT + 'kit-edges-closeup.png')
print('ok')
