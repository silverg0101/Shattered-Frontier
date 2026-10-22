"""Concept (left) next to a live capture (right), both scaled to 1080 high: py -3 tools/Compare-Side.py concept.png live.png out.png"""
import sys
from PIL import Image
concept, live, out = (Image.open(p).convert('RGB') for p in sys.argv[1:3]), None, sys.argv[3]
concept, live = concept
def fit(image): return image.resize((round(image.width * 1080 / image.height), 1080), Image.LANCZOS)
concept, live = fit(concept), fit(live)
sheet = Image.new('RGB', (concept.width + live.width + 8, 1080), (20, 20, 20))
sheet.paste(concept, (0, 0)); sheet.paste(live, (concept.width + 8, 0))
sheet.save(out)
