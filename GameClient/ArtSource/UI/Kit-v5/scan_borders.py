"""Fail on chroma-key red/green speckles in every Kit-v5 PNG outer 6 px."""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).parent
failures = []
checked = 0
for path in sorted(ROOT.glob("*.png")):
    im = Image.open(path).convert("RGBA")
    checked += 1
    width, height = im.size
    count = 0
    for y in range(height):
        for x in range(width):
            if 6 <= x < width - 6 and 6 <= y < height - 6:
                continue
            red, green, blue, alpha = im.getpixel((x, y))
            if alpha and ((red >= 80 and green * 10 <= red and blue * 10 <= red)
                          or (green >= 80 and red * 10 <= green and blue * 10 <= green)):
                count += 1
    if count:
        failures.append(f"{path.name}: {count}")

if failures:
    raise SystemExit(f"FAIL {checked} PNGs: " + ", ".join(failures))
print(f"PASS {checked} PNGs: no opaque chroma red/green pixels in outer 6 px")
