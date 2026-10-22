"""Concept-left / live-right pairs for the tree-felling review.

py -3 make_pairs.py <capture root with level/ and slope/ from tools/Test-HarvestFellingReview.ps1>
Writes <ground>-{sequence,stump,log}-compare.png and <ground>-frames.png (every captured frame) beside this script.
"""
import sys
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent
CONCEPT = HERE.parents[3] / 'GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/Concept'
W, H = 1920, 1080


def fit(path):
    return Image.open(path).convert('RGB').resize((W, H), Image.LANCZOS)


def grid(paths, cols, w, h):
    sheet = Image.new('RGB', (w * cols, h * ((len(paths) + cols - 1) // cols)), (40, 40, 40))
    for i, p in enumerate(paths):
        sheet.paste(Image.open(p).convert('RGB').resize((w, h), Image.LANCZOS), ((i % cols) * w, (i // cols) * h))
    return sheet


def pair(left, right, out):
    img = Image.new('RGB', (W * 2, H))
    img.paste(left, (0, 0)); img.paste(right, (W, 0))
    img.save(HERE / out, optimize=True)


root = Path(sys.argv[1])
for ground in ('level', 'slope'):
    run = root / ground
    frames = sorted(run.glob('*.png'))
    pick = lambda prefix: next(f for f in frames if f.name.startswith(prefix))
    # Concept keyframes: notched, ~20 deg, ~55 deg, rest.
    keys = [pick('00-'), pick('03-'), pick('05-'), pick('08-')]
    pair(fit(CONCEPT / 'sequence/felling-sequence-concept.png'), grid(keys, 2, W // 2, H // 2), f'{ground}-sequence-compare.png')
    pair(fit(CONCEPT / 'stump/stump-close-concept.png'), fit(pick('09-')), f'{ground}-stump-compare.png')
    pair(fit(CONCEPT / 'log/log-close-concept.png'), fit(pick('10-')), f'{ground}-log-compare.png')
    grid(frames, 4, 960, 540).save(HERE / f'{ground}-frames.png', optimize=True)
    print('PAIRS', ground, len(frames))
