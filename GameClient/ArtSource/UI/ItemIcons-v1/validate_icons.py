"""Validate icon exports and rebuild the slate review sheet (Pillow only)."""

from collections import deque
import json
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent
ITEMS = json.loads((ROOT / "manifest.json").read_text(encoding="utf-8"))["items"]
SLATE = (23, 35, 42, 255)


def enclosed_alpha_pixels(alpha: Image.Image) -> int:
    """Count transparent pixels unreachable from the canvas border."""
    width, height = alpha.size
    pixels = alpha.load()
    outside = set()
    queue = deque()
    for x in range(width):
        for y in (0, height - 1):
            if pixels[x, y] < 128 and (x, y) not in outside:
                outside.add((x, y))
                queue.append((x, y))
    for y in range(height):
        for x in (0, width - 1):
            if pixels[x, y] < 128 and (x, y) not in outside:
                outside.add((x, y))
                queue.append((x, y))
    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < width and 0 <= ny < height:
                if pixels[nx, ny] < 128 and (nx, ny) not in outside:
                    outside.add((nx, ny))
                    queue.append((nx, ny))
    return sum(
        pixels[x, y] < 128 and (x, y) not in outside
        for y in range(height)
        for x in range(width)
    )


def main() -> None:
    names = sorted(ITEMS)
    sheet = Image.new("RGBA", (5 * 256, 3 * 286), SLATE)
    labels = ImageDraw.Draw(sheet)
    assert len(names) == 15
    for index, item_id in enumerate(names):
        pair = ITEMS[item_id]
        for size in (256, 64):
            image = Image.open(ROOT / pair[str(size)])
            assert image.mode == "RGBA" and image.size == (size, size), pair[str(size)]
            alpha = image.getchannel("A")
            assert alpha.getextrema() == (0, 255), pair[str(size)]
            assert enclosed_alpha_pixels(alpha) == 0, f"interior alpha hole: {pair[str(size)]}"
            if size == 256:
                x, y = index % 5 * 256, index // 5 * 286
                sheet.alpha_composite(image, (x, y))
                labels.text((x + 5, y + 260), item_id.removeprefix("item/"), fill="white")
    sheet.convert("RGB").save(ROOT / "icons-on-slate.png")
    print(f"PASS: {len(names)} icons, 30 RGBA exports, no interior alpha holes")


if __name__ == "__main__":
    main()
