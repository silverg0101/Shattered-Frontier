"""Place the unchanged Gilded Tankard concept beside each unaltered game capture: py -3 make_pairs.py"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent
CONCEPT = Path(__file__).resolve().parents[4] / 'Game-Design/concept-art/Silverbrook/The Gilded Tankard.png'
concept = Image.open(CONCEPT).convert('RGB')
font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 24)
height, header = 900, 48
left = concept.resize((round(concept.width * height / concept.height), height), Image.Resampling.LANCZOS)
for shot in sorted(p for p in OUT.glob('*.png') if not p.stem.endswith('-compare')):
    live = Image.open(shot).convert('RGB')
    live = live.resize((round(live.width * height / live.height), height), Image.Resampling.LANCZOS)
    pair = Image.new('RGB', (left.width + live.width, height + header), (16, 16, 16))
    pair.paste(left, (0, header))
    pair.paste(live, (left.width, header))
    draw = ImageDraw.Draw(pair)
    draw.text((12, 11), 'CONCEPT: The Gilded Tankard', font=font, fill='white')
    draw.text((left.width + 12, 11), 'LIVE: ' + shot.stem, font=font, fill='white')
    pair.save(OUT / (shot.stem + '-compare.png'))
    print('PAIR', shot.stem + '-compare.png')
