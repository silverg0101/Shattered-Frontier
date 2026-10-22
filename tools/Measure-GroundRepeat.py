"""Ground repeat gate (GFX-02 section 2a): autocorrelation peak of the luminance in the fixed 256 px crops of each A-view capture.

py -3.12 tools/Measure-GroundRepeat.py <capture_dir> [out.json] [--bookmarks <json>] [--limit 0.25]
Crops come from `repeat_crops` ({name: [x, y, w, h]}, 1920x1080 pixels) of each view in silverbrook-reference-bookmarks.json; <capture_dir>/<view>.png is read.
Per crop: luminance 0.2126 R + 0.7152 G + 0.0722 B (sRGB values), 6 px Gaussian high-pass, mean removed, Hann window, autocorrelation ifft2(|fft2|^2) normalised to 1 at the origin;
peak = max over r_min px (view `repeat_min_radius_px`, default 6) < radius <= 120 px, taken over local maxima only (a smooth image's zero-lag shoulder is not a repeat). Gate: peak <= limit for every crop. Exit 0 when all pass, 1 otherwise.
"""
import json, sys
from pathlib import Path
import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter

ROOT = Path(__file__).resolve().parents[1]
args = sys.argv[1:]


def opt(k, d):
    if k not in args:
        return d
    i = args.index(k); v = args[i + 1]; del args[i:i + 2]; return v


bookmarks = Path(opt('--bookmarks', ROOT / 'GameClient/ArtSource/Greenlands/silverbrook-reference-bookmarks.json'))
limit = float(opt('--limit', 0.25))
shots = Path(args[0]); out = Path(args[1]) if len(args) > 1 else None


def peak(rgb, r_min=6):
    lum = rgb @ np.array([0.2126, 0.7152, 0.0722])
    lum = lum - gaussian_filter(lum, 6)  # high-pass (6 px): shadows and gradients must not swallow the texture
    lum = lum - lum.mean()
    h, w = lum.shape
    lum = lum * np.outer(np.hanning(h), np.hanning(w))
    ac = np.fft.ifft2(np.abs(np.fft.fft2(lum)) ** 2).real
    ac = np.fft.fftshift(ac / ac[0, 0])
    yy, xx = np.indices(ac.shape)
    r = np.hypot(yy - h // 2, xx - w // 2)
    # a repeat is a secondary maximum; the shoulder of the zero-lag lobe (any smooth image reads ~1 at 6 px) is not one
    near = np.stack([np.roll(ac, (dy, dx), (0, 1)) for dy in (-1, 0, 1) for dx in (-1, 0, 1) if dy or dx])
    band = (r > r_min) & (r <= 120) & (ac >= near.max(axis=0))
    if not band.any():
        return float('nan'), [0, 0]  # no measurement: fails the crop, never reads as 0
    i = np.argmax(np.where(band, ac, -np.inf))
    return float(ac.flat[i]), [int(xx.flat[i] - w // 2), int(yy.flat[i] - h // 2)]


result, ok = {}, True
for v in json.loads(bookmarks.read_text(encoding='utf-8'))['views']:
    for crop, (x, y, w, h) in v.get('repeat_crops', {}).items():
        img = np.asarray(Image.open(shots / f"{v['name']}.png").convert('RGB'), dtype=np.float64)
        p, lag = peak(img[y:y + h, x:x + w], v.get('repeat_min_radius_px', 6))
        result[f"{v['name']}/{crop}"] = dict(peak=round(p, 4), lag_px=lag, rect=[x, y, w, h], passed=bool(p <= limit))
        ok &= bool(p <= limit)
report = dict(limit=limit, passed=ok, crops=result)
for k, r in result.items():
    print(f"{'PASS' if r['passed'] else 'FAIL'} {k} peak {r['peak']:.3f} lag {r['lag_px']}")
print(f"GROUND_REPEAT {'PASS' if ok else 'FAIL'} crops {len(result)} max_peak {np.nanmax([r['peak'] for r in result.values()]) if result else 'n/a'} limit {limit}")
if out:
    out.write_text(json.dumps(report, indent=2), encoding='utf-8')
sys.exit(0 if ok and result else 1)
