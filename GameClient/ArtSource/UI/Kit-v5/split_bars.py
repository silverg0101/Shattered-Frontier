"""Derive independently compositable bar layers from the baked Kit-v5 PNGs."""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).parent
NAMES = ("bar_hp", "bar_stamina", "bar_xp", "bar_channel")
SIZE = (760, 64)
# Full-size frame aperture; its narrow dark rim remains part of the casing.
APERTURE = ((18, 32), (42, 9), (718, 9), (742, 32), (718, 55), (42, 55))
FILL_RECT = (42, 13, 718, 51)
TILE_LEFT, TILE_WIDTH = 316, 128


def aperture_mask():
    scale = 4
    mask = Image.new("L", (SIZE[0] * scale, SIZE[1] * scale))
    ImageDraw.Draw(mask).polygon([(x * scale, y * scale) for x, y in APERTURE], fill=255)
    return mask.resize(SIZE, Image.Resampling.LANCZOS)


def seamless_fill(source):
    left, top, right, bottom = FILL_RECT
    tile = source.crop((TILE_LEFT, top, TILE_LEFT + TILE_WIDTH, bottom)).convert("RGBA")
    # Blend both ends toward their mean so repetition has no visible edge.
    pixels = tile.load()
    for y in range(tile.height):
        edge = tuple((pixels[0, y][c] + pixels[tile.width - 1, y][c]) // 2 for c in range(3))
        for x in range(12):
            for pos in (x, tile.width - 1 - x):
                weight = (12 - x) / 13
                original = pixels[pos, y]
                pixels[pos, y] = tuple(round(original[c] * (1 - weight) + edge[c] * weight) for c in range(3)) + (255,)
        pixels[0, y] = pixels[tile.width - 1, y] = edge + (255,)
    return tile


def main():
    mask = aperture_mask()
    for name in NAMES:
        source = Image.open(ROOT / f"{name}.png").convert("RGBA")
        assert source.size == SIZE
        frame = source.copy()
        alpha = frame.getchannel("A")
        alpha.putdata([round(a * (255 - m) / 255) for a, m in zip(alpha.getchannel(0).get_flattened_data(), mask.get_flattened_data())])
        frame.putalpha(alpha)
        frame.putdata([(r, g, b, a) if a else (0, 0, 0, 0) for r, g, b, a in frame.get_flattened_data()])
        frame.save(ROOT / f"{name}_frame.png")
        seamless_fill(source).save(ROOT / f"{name}_fill.png")

        # The target plate's slate texture supplies the empty trough treatment.
        slate = Image.open(ROOT / "target_frame.png").convert("RGBA").crop((80, 42, 740, 98))
        slate = slate.resize(SIZE, Image.Resampling.BILINEAR)
        slate.putalpha(mask)
        slate.save(ROOT / f"{name}_back.png")


if __name__ == "__main__":
    main()
