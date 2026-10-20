"""Build a run_pixal_mv.py views folder from four separate images or one turnaround sheet.

Views are FRONT, LEFT (faces image-left), BACK, RIGHT. Each is padded to a square with its own
corner colour, masked with prepare_model_input.py, and paired with upstream's four-view orbit transforms.json.
"""
import argparse, hashlib, json, shutil, subprocess, sys
from pathlib import Path
from PIL import Image

NAMES = ['view00_azim000', 'view01_azim090', 'view02_azim180', 'view03_azim270']
parser = argparse.ArgumentParser()
source = parser.add_mutually_exclusive_group(required=True)
source.add_argument('--views', nargs=4, metavar=('FRONT', 'LEFT', 'BACK', 'RIGHT'), help='Four opaque images, plain background')
source.add_argument('--sheet', help='One opaque sheet with the four views in equal-width cells, left to right')
parser.add_argument('--output', required=True, help='New views folder')
parser.add_argument('--tolerance', type=int, default=12, help='Passed to prepare_model_input.py')
parser.add_argument('--transforms', default='/home/grego/sf-asset-lab/sources/Pixal3D/assets/mv_images/example/transforms.json')
args = parser.parse_args()
out = Path(args.output).resolve()
if out.exists():
    raise FileExistsError(f'Choose a new output folder: {out}')
inputs = [Path(p).resolve() for p in (args.views or [args.sheet])]
if args.sheet:
    sheet = Image.open(inputs[0]).convert('RGB')
    cell = sheet.width // len(NAMES)
    images = [sheet.crop((i * cell, 0, (i + 1) * cell, sheet.height)) for i in range(len(NAMES))]
else:
    images = [Image.open(p).convert('RGB') for p in inputs]
if len({im.size for im in images}) != 1:
    raise ValueError(f'Views differ in size: {[im.size for im in images]}; frame them identically')
(out / 'cells').mkdir(parents=True)
for name, image in zip(NAMES, images):
    side = max(image.size)
    square = Image.new('RGB', (side, side), image.getpixel((0, 0)))
    square.paste(image, ((side - image.width) // 2, (side - image.height) // 2))
    square.save(out / 'cells' / f'{name}.png')
    subprocess.run([sys.executable, str(Path(__file__).with_name('prepare_model_input.py')), '--image', str(out / 'cells' / f'{name}.png'),
                    '--output', str(out / f'{name}.png'), '--tolerance', str(args.tolerance)], check=True, stdout=subprocess.DEVNULL)
shutil.copyfile(args.transforms, out / 'transforms.json')
(out / 'inputs.json').write_text(json.dumps({'inputs': [{'path': str(p), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in inputs],
    'view_px': max(images[0].size), 'transforms_source': args.transforms, 'review_status': 'needs-review'}, indent=2) + '\n')
print(out)
