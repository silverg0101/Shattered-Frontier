"""Assemble immutable review pairs from new captures and frozen round-2 concepts."""
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Artifacts/LootItems/in-engine/round3'
PREV = ROOT / 'Artifacts/LootItems/in-engine/round2'
SOURCE = Path('C:/Projects/Shattered-Frontier/Artifacts/LootItems')
stems = ('archers_gloves', 'fangtooth_dagger', 'wolf_fang', 'brigand_blade',
         'cudgel', 'captains_longsword', 'wolf_pelt', 'wolfhide_bracers')


def paired(left, right, destination, left_label, right_label):
    canvas = Image.new('RGB', (1920, 1080), '#555555')
    draw = ImageDraw.Draw(canvas)
    draw.text((14, 12), left_label, fill='white')
    draw.text((975, 12), right_label, fill='white')
    canvas.paste(left.resize((960, 960), Image.Resampling.LANCZOS), (0, 80))
    canvas.paste(right.resize((960, 540), Image.Resampling.LANCZOS), (960, 290))
    canvas.save(destination)


for stem in stems:
    prior = Image.open(PREV / (stem + '-compare.png')).convert('RGB')
    # The left half is the approved reference from frozen round 2. Its pixels
    # and labeling are reused so the reviewer sees exactly the same concept.
    approved = prior.crop((0, 80, 960, 1040))
    live = Image.open(OUT / (stem + '.png')).convert('RGB')
    paired(approved, live, OUT / (stem + '-compare.png'),
           stem + ' | approved reference (round 2 frozen)', 'Unreal -game | 10 cm cube | round 3')

source_views = {
    'captains_longsword': SOURCE / 'captains_longsword/round3/review/s222_tex_q34.png',
    'wolf_pelt': SOURCE / 'wolf_pelt/round3/review/pelt_tex_q34.png',
}
for stem, path in source_views.items():
    paired(Image.open(path).convert('RGB'),
           Image.open(OUT / (stem + '_unlit.png')).convert('RGB'),
           OUT / (stem + '-source-unlit.png'),
           stem + ' | source GLB textured render',
           'Unreal imported mesh | unlit diagnostic; lighting differs')

for stem in ('archers_gloves', 'cudgel'):
    prior = Image.open(PREV / (stem + '-compare.png')).convert('RGB')
    paired(prior.crop((0, 80, 960, 1040)),
           Image.open(OUT / (stem + '_unlit.png')).convert('RGB'),
           OUT / (stem + '-concept-unlit.png'),
           stem + ' | approved concept',
           'Unreal imported mesh | unlit diagnostic')

report = []
for path in sorted(OUT.glob('*.png')):
    with Image.open(path) as im:
        dims = list(im.size)
    report.append({'file': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                   'size_px': dims})
(OUT / 'manifest.json').write_text(json.dumps({
    'round': 3, 'capture_mode': 'Unreal -game RenderOffscreen',
    'approved_reference': 'frozen round2 left halves; not recaptured',
    'files': report}, indent=2) + '\n')
print('ROUND3_PAIRS', len(report), OUT)
