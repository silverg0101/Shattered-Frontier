"""Write T_ChimneySmoke.png: a soft, V-tileable smoke column for the chimney plume cards (MI_SB_ChimneySmoke).

py make_chimney_smoke.py   (numpy + Pillow; deterministic). The lamp wisp T_LampSmoke is a thin, faint strand
(alpha <= 0.35 over a third of the width), which reads as nothing on a 1.4 x 4.2 m chimney card.
Columns: U across the card (dense core, soft edges); rows: V along the plume, periodic so a V pan loops seamlessly.
"""
from pathlib import Path
import numpy as np
from PIL import Image

W, H = 256, 512
rng = np.random.default_rng(7)


def periodic_noise(octaves=5):
    """Value noise, periodic in both axes, normalised to 0..1."""
    out = np.zeros((H, W))
    for o in range(octaves):
        cells_x, cells_y = 2 ** (o + 1), 2 ** (o + 2)
        grid = rng.random((cells_y, cells_x))
        ys = np.arange(H) * cells_y / H; xs = np.arange(W) * cells_x / W
        y0 = ys.astype(int); x0 = xs.astype(int); fy = ys - y0; fx = xs - x0
        fy = fy * fy * (3 - 2 * fy); fx = fx * fx * (3 - 2 * fx)
        a = grid[y0 % cells_y][:, x0 % cells_x]; b = grid[y0 % cells_y][:, (x0 + 1) % cells_x]
        c = grid[(y0 + 1) % cells_y][:, x0 % cells_x]; d = grid[(y0 + 1) % cells_y][:, (x0 + 1) % cells_x]
        top = a + (b - a) * fx; bot = c + (d - c) * fx
        out += (top + (bot - top) * fy[:, None]) / 2 ** o
    out -= out.min()
    return out / out.max()


n = periodic_noise()
u = (np.arange(W) + .5) / W - .5
sway = (periodic_noise(3)[:, :1] - .5) * .18          # the column's centre drifts a little along V
profile = np.exp(-((u[None, :] - sway) / .30) ** 2)    # soft-edged column, about 70 % of the card wide
alpha = np.clip(profile * (.35 + .75 * n), 0, 1) * 1.0
shade = .82 + .18 * n                                  # lighter puffs, slightly darker gaps
rgba = np.dstack([shade, shade, shade * 1.02, alpha])
out = Path(__file__).with_name('T_ChimneySmoke.png')
Image.fromarray((np.clip(rgba, 0, 1) * 255).astype(np.uint8), 'RGBA').save(out)
assert alpha.max() > .7 and (profile[0] > .5).sum() > W * .3, 'plume too faint or too narrow'
print('CHIMNEY_SMOKE_TEXTURE', out.name, W, H, 'alpha max', round(float(alpha.max()), 2), 'mean', round(float(alpha.mean()), 2))
