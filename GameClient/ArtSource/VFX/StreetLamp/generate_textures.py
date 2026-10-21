"""Deterministic source textures. Run with Pillow 10+; no editor required."""
from pathlib import Path
from PIL import Image
import math
import random

ROOT = Path(__file__).resolve().parent
N = 128
rng = random.Random(1701)


def save(name, pixel):
    im = Image.new('RGBA', (N, N))
    im.putdata([pixel(x, y) for y in range(N) for x in range(N)])
    im.save(ROOT / name, optimize=True)


def flame(x, y):
    u = (x + .5 - N / 2) / (N / 2)
    h = 1 - (y + .5) / N
    width = .37 * (1 - h) ** 1.5 + .018
    bend = .08 * math.sin(h * 13) * h
    body = max(0, 1 - abs(u - bend) / width)
    a = int(255 * min(1, body ** 1.8 * min(1, h * 8) * min(1, (1 - h) * 15)))
    return (255, int(70 + 165 * body), int(8 + 75 * body), a)


def smoke(x, y):
    u = (x + .5) / N - .5
    v = (y + .5) / N
    width = .11 + .16 * (1 - v)
    flow = .09 * math.sin(v * 11) + .04 * math.sin(v * 23)
    core = max(0, 1 - abs(u - flow) / width)
    grain = .8 + .2 * math.sin(x * .17 + y * .23) * math.sin(y * .13)
    a = int(100 * core ** 2 * grain * min(1, v * 8) * min(1, (1 - v) * 8))
    return (185, 174, 154, a)


def glow(x, y):
    d = math.hypot((x + .5 - N / 2) / (N / 2), (y + .5 - N / 2) / (N / 2))
    return (255, 181, 71, int(115 * math.exp(-7 * d * d) * max(0, 1 - d)))


if __name__ == '__main__':
    save('T_LampFlame.png', flame)
    save('T_LampSmoke.png', smoke)
    save('T_LampGlow.png', glow)
