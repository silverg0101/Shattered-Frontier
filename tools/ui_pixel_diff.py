"""Concept-vs-live pixel diff for the v5 UI screens (acceptance numbers for the Fable reviewer; it does not replace the reviewer).

    py -3 tools/ui_pixel_diff.py <screen> [--live <live.png>] [--out <dir>]
    py -3 tools/ui_pixel_diff.py --all [--live-dir docs/evidence/ui-v5/final3]

Per screen it reads docs/evidence/ui-v5/elements/<screen>.json (concept rects annotated once, in 1920x1080 px):
    {"concept": "build-mode.png", "elements": [{"name": "tab Voxel Objects", "rect": [x, y, w, h], "samples": [[x, y, "label"], ...]}]}
and writes to <out>/<screen>/: diff-heat.png, overlay-50.png, flip.gif (concept/live alternating), report.md and report.json.
Rect delta: the concept's top-left and bottom-right corner patches are template-matched (ZNCC on grayscale, +-SEARCH px) in the live
frame, giving the live left/top/right/bottom edge; Colour: CIEDE2000 between concept and live at each sample point (3x3 mean) after
shifting the sample by the element's measured offset. Tolerances (docs/blueprints/ui-hud.md "Acceptance"): geometry +-2 px, size +-2 px, dE2000 <= 3.
"""
import argparse, json, sys
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
CONCEPTS = ROOT / 'docs/Game-Design/concept-art/UI/v5'
ELEMENTS = ROOT / 'docs/evidence/ui-v5/elements'
SEARCH, PATCH, TOL_PX, TOL_DE = 24, 24, 2, 3.0


def load(path):
    return Image.open(path).convert('RGB').resize((1920, 1080), Image.LANCZOS) if Image.open(path).size != (1920, 1080) else Image.open(path).convert('RGB')


def srgb_to_lab(rgb):
    c = np.asarray(rgb, dtype=np.float64) / 255.0
    c = np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
    m = np.array([[0.4124564, 0.3575761, 0.1804375], [0.2126729, 0.7151522, 0.0721750], [0.0193339, 0.1191920, 0.9503041]])
    xyz = c @ m.T / np.array([0.95047, 1.0, 1.08883])
    f = np.where(xyz > 216 / 24389, np.cbrt(xyz), (24389 / 27 * xyz + 16) / 116)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], axis=-1)


def de2000(l1, l2):
    L1, a1, b1 = l1; L2, a2, b2 = l2
    C1, C2 = np.hypot(a1, b1), np.hypot(a2, b2); Cb = (C1 + C2) / 2
    G = 0.5 * (1 - np.sqrt(Cb ** 7 / (Cb ** 7 + 25 ** 7)))
    a1p, a2p = (1 + G) * a1, (1 + G) * a2
    C1p, C2p = np.hypot(a1p, b1), np.hypot(a2p, b2)
    h1p = np.degrees(np.arctan2(b1, a1p)) % 360; h2p = np.degrees(np.arctan2(b2, a2p)) % 360
    dLp, dCp = L2 - L1, C2p - C1p
    dh = h2p - h1p
    dh = dh - 360 if dh > 180 else dh + 360 if dh < -180 else dh
    if C1p * C2p == 0: dh = 0
    dHp = 2 * np.sqrt(C1p * C2p) * np.sin(np.radians(dh) / 2)
    Lbp, Cbp = (L1 + L2) / 2, (C1p + C2p) / 2
    hs = h1p + h2p
    if C1p * C2p == 0: hbp = hs
    elif abs(h1p - h2p) <= 180: hbp = hs / 2
    else: hbp = (hs + 360) / 2 if hs < 360 else (hs - 360) / 2
    T = 1 - 0.17 * np.cos(np.radians(hbp - 30)) + 0.24 * np.cos(np.radians(2 * hbp)) + 0.32 * np.cos(np.radians(3 * hbp + 6)) - 0.20 * np.cos(np.radians(4 * hbp - 63))
    dth = 30 * np.exp(-(((hbp - 275) / 25) ** 2))
    Rc = 2 * np.sqrt(Cbp ** 7 / (Cbp ** 7 + 25 ** 7)); Sl = 1 + 0.015 * (Lbp - 50) ** 2 / np.sqrt(20 + (Lbp - 50) ** 2)
    Sc, Sh = 1 + 0.045 * Cbp, 1 + 0.015 * Cbp * T; Rt = -np.sin(np.radians(2 * dth)) * Rc
    return float(np.sqrt((dLp / Sl) ** 2 + (dCp / Sc) ** 2 + (dHp / Sh) ** 2 + Rt * (dCp / Sc) * (dHp / Sh)))


def gray(img):
    return np.asarray(img.convert('L'), dtype=np.float64)


def match_patch(concept_g, live_g, cx, cy):
    """Top-left of the PATCH x PATCH concept patch at (cx, cy) found in the live frame within +-SEARCH; returns (dx, dy, score)."""
    h, w = concept_g.shape
    cx, cy = int(np.clip(cx, 0, w - PATCH)), int(np.clip(cy, 0, h - PATCH))
    tpl = concept_g[cy:cy + PATCH, cx:cx + PATCH]; tpl = tpl - tpl.mean(); tn = np.sqrt((tpl ** 2).sum()) or 1.0
    best = (0, 0, -2.0)
    for dy in range(-SEARCH, SEARCH + 1):
        for dx in range(-SEARCH, SEARCH + 1):
            x, y = cx + dx, cy + dy
            if x < 0 or y < 0 or x + PATCH > w or y + PATCH > h: continue
            win = live_g[y:y + PATCH, x:x + PATCH]; win = win - win.mean(); wn = np.sqrt((win ** 2).sum()) or 1.0
            s = float((tpl * win).sum() / (tn * wn))
            if s > best[2]: best = (dx, dy, s)
    return best


def sample(img, x, y):
    a = np.asarray(img)[max(0, y - 1):y + 2, max(0, x - 1):x + 2].reshape(-1, 3).mean(axis=0)
    return a


def run(screen, live_path, out_root):
    spec = json.loads((ELEMENTS / (screen + '.json')).read_text())
    concept = load(CONCEPTS / spec['concept']); live = load(live_path)
    out = Path(out_root) / screen; out.mkdir(parents=True, exist_ok=True)
    cg, lg = gray(concept), gray(live)
    diff = np.abs(np.asarray(concept, dtype=np.int16) - np.asarray(live, dtype=np.int16)).sum(axis=2) / 3
    heat = np.zeros((1080, 1920, 3), dtype=np.uint8); heat[..., 0] = np.clip(diff * 3, 0, 255); heat[..., 1] = np.clip(255 - diff * 3, 0, 255) // 4
    Image.fromarray(heat).save(out / 'diff-heat.png')
    Image.blend(concept, live, 0.5).save(out / 'overlay-50.png')
    concept.save(out / 'flip.gif', save_all=True, append_images=[live], duration=700, loop=0)
    rows, ok_all = [], True
    for el in spec['elements']:
        x, y, w, h = el['rect']
        tl = match_patch(cg, lg, x, y); br = match_patch(cg, lg, x + w - PATCH, y + h - PATCH)
        found = min(tl[2], br[2]) >= 0.5  # below this the element is not where the template search could see it
        dl, dt, dr, db = (tl[0], tl[1], br[0], br[1]) if found else (None,) * 4
        dw = (dr - dl) if found else None; dh_ = (db - dt) if found else None
        geo = found and max(abs(dl), abs(dt), abs(dw), abs(dh_)) <= TOL_PX
        des = []
        for sx, sy, label in el.get('samples', []):
            ox, oy = (dl, dt) if found else (0, 0)
            c = srgb_to_lab(sample(concept, sx, sy)); l = srgb_to_lab(sample(live, sx + ox, sy + oy))
            des.append((label, round(de2000(c, l), 1)))
        col = all(d <= TOL_DE for _, d in des)
        ok = bool(geo and col); ok_all &= ok
        rows.append(dict(name=el['name'], rect=el['rect'], found=found, match=round(min(tl[2], br[2]), 2), d_left=dl, d_top=dt, d_width=dw, d_height=dh_, de=des, geometry='PASS' if geo else 'FAIL', colour='PASS' if col else 'FAIL', verdict='PASS' if ok else 'FAIL'))
    (out / 'report.json').write_text(json.dumps(rows, indent=1))
    lines = ['# %s: pixel diff, %s' % (screen, 'PASS' if ok_all else 'FAIL'), '', 'Tolerance: geometry +-%d px (left/top/width/height), colour dE2000 <= %.0f. dx/dy = live minus concept; "-" = the corner patch was not found (match < 0.5).' % (TOL_PX, TOL_DE), '',
             '| element | concept rect | dx | dy | dw | dh | match | colour dE | verdict |', '| --- | --- | --- | --- | --- | --- | --- | --- | --- |']
    for r in rows:
        f = lambda v: '-' if v is None else '%+d' % v
        lines.append('| %s | %s | %s | %s | %s | %s | %.2f | %s | %s |' % (r['name'], r['rect'], f(r['d_left']), f(r['d_top']), f(r['d_width']), f(r['d_height']), r['match'], ', '.join('%s %.1f' % d for d in r['de']) or '-', r['verdict']))
    lines += ['', 'Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).']
    (out / 'report.md').write_text('\n'.join(lines) + '\n')
    print(screen, 'PASS' if ok_all else 'FAIL', sum(r['verdict'] == 'PASS' for r in rows), '/', len(rows))
    return ok_all


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('screen', nargs='?'); ap.add_argument('--all', action='store_true')
    ap.add_argument('--live'); ap.add_argument('--live-dir', default=str(ROOT / 'docs/evidence/ui-v5/final3')); ap.add_argument('--out', default=str(ROOT / 'docs/evidence/ui-v5/final3/pixel'))
    a = ap.parse_args()
    screens = [p.stem for p in sorted(ELEMENTS.glob('*.json'))] if a.all else [a.screen]
    ok = True
    for s in screens:
        live = a.live if a.live and not a.all else Path(a.live_dir) / (s + '-live.png')
        if not Path(live).exists(): print(s, 'no live capture', live); ok = False; continue
        ok &= run(s, live, a.out)
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
