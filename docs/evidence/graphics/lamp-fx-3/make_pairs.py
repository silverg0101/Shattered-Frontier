"""Place the unchanged concept beside each unaltered game capture."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent
CONCEPT = Path(__file__).resolve().parents[3] / 'Game-Design/concept-art/Silverbrook/Street Lamp-1.png'
concept = Image.open(CONCEPT).convert('RGB')
font_path = 'C:/Windows/Fonts/arial.ttf'
font = ImageFont.truetype(font_path, 24)
height = 900
header = 48
left = concept.resize((round(concept.width * height / concept.height), height), Image.Resampling.LANCZOS)
for name in ('day-close', 'dusk-close', 'night-close', 'dusk-street', 'night-street'):
    live = Image.open(OUT / (name + '.png')).convert('RGB')
    live = live.resize((round(live.width * height / live.height), height), Image.Resampling.LANCZOS)
    pair = Image.new('RGB', (left.width + live.width, height + header), (16, 16, 16))
    pair.paste(left, (0, header))
    pair.paste(live, (left.width, header))
    draw = ImageDraw.Draw(pair)
    draw.text((12, 11), 'CONCEPT: Street Lamp-1', font=font, fill='white')
    draw.text((left.width + 12, 11), 'LIVE: ' + name, font=font, fill='white')
    pair.save(OUT / (name + '-compare.jpg'), quality=90)
