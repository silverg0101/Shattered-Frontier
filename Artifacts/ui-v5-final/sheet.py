import sys, glob, os
from PIL import Image
d = sys.argv[1]; n = int(sys.argv[2]) if len(sys.argv) > 2 else 9
fs = sorted(glob.glob(os.path.join(d, 'frames', 'shot-*.png')))
step = max(1, len(fs) // n); pick = fs[::step][:n]
cols = 3; w, h = 640, 360
s = Image.new('RGB', (cols * w, ((len(pick) + cols - 1) // cols) * h))
for i, f in enumerate(pick):
    s.paste(Image.open(f).convert('RGB').resize((w, h), Image.LANCZOS), ((i % cols) * w, (i // cols) * h))
s.save(os.path.join(d, 'sheet.png')); print(len(fs), [os.path.basename(p) for p in pick])
