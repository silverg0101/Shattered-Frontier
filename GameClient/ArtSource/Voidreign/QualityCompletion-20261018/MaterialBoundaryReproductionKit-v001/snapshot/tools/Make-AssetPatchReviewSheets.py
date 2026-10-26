"""Compose indexed inspection plates from an explicit finite image queue."""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
profile, out = Path(a.profile).resolve(), Path(a.output).resolve()
cfg = json.loads(profile.read_text(encoding='utf-8-sig'))
assert cfg['schema_version'] == 1 and not out.exists()
items, labels = cfg['items'], cfg['column_labels']
assert 1 <= len(items) <= 128 and 1 <= len(labels) <= 8
assert len({r['id'] for r in items}) == len(items)
tile, batch = cfg['tile_pixels'], cfg['rows_per_sheet']
assert 128 <= tile <= 512 and 1 <= batch <= 8
for path, expected in cfg.get('input_sha256', {}).items():
    assert sha(path) == expected, path
for row in items:
    assert len(row['images']) == len(labels)
    assert all(Path(f).is_file() for f in row['images'])
out.mkdir(parents=True)
(out / 'profile.json').write_bytes(profile.read_bytes())
(out / 'tool-source.py').write_bytes(Path(__file__).read_bytes())
font = ImageFont.load_default(size=15)
records = []
for start in range(0, len(items), batch):
    chunk = items[start:start + batch]
    image = Image.new('RGB', (tile * len(labels), 30 + (tile + 24) * len(chunk)), '#202329')
    draw = ImageDraw.Draw(image)
    for column, label in enumerate(labels):
        draw.text((column * tile + 5, 7), label, font=font, fill='white')
    name = 'sheet-' + str(start // batch + 1).zfill(2) + '.png'
    for index, row in enumerate(chunk):
        top = 30 + index * (tile + 24)
        draw.text((5, top + 3), row['id'], font=font, fill='white')
        inputs = []
        for column, path in enumerate(row['images']):
            with Image.open(path) as source:
                thumb = source.convert('RGB')
                thumb.thumbnail((tile, tile), Image.Resampling.LANCZOS)
            image.paste(thumb, (column * tile + (tile-thumb.width)//2, top + 24))
            inputs.append(dict(path=path, sha256=sha(path)))
        records.append(dict(id=row['id'], sheet=name, row=index, images=inputs,
                            review_status='awaiting-visual-review'))
    image.save(out / name)
report = dict(schema_version=1, status='inspection-plates-only-not-semantic-approval',
    profile_sha256=sha(profile), tool_sha256=sha(__file__), items=records,
    sheets=[dict(path=f.name, sha256=sha(f)) for f in sorted(out.glob('sheet-*.png'))])
(out / 'sheets-report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(dict(status=report['status'], items=len(items), sheets=len(report['sheets']))))
