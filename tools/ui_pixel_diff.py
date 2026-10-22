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


def edge_pos(g, side, x, y, w, h):
    """Strongest intensity edge (gradient peak of the strip-averaged profile) near one side of the rect: returns (position, strength)."""
    if side in 'lr':
        c = x if side == 'l' else x + w; lo, hi = max(1, c - SEARCH), min(g.shape[1] - 2, c + SEARCH)
        y0, y1 = int(y + h * 0.25), max(int(y + h * 0.75), int(y + h * 0.25) + 1)
        prof = g[y0:y1, :].mean(axis=0)
    else:
        c = y if side == 't' else y + h; lo, hi = max(1, c - SEARCH), min(g.shape[0] - 2, c + SEARCH)
        x0, x1 = int(x + w * 0.25), max(int(x + w * 0.75), int(x + w * 0.25) + 1)
        prof = g[:, x0:x1].mean(axis=1)
    grad = np.abs(np.convolve(prof, [1, 0, -1], mode='same'))
    seg = grad[lo:hi + 1]
    # nearest of the strong edges (>= 60% of the strongest) to the annotated side: borders beat inner texture
    strong = np.where(seg >= 0.6 * seg.max())[0] if seg.max() > 0 else []
    if len(strong) == 0: return c, 0.0
    best = min(strong, key=lambda i: abs(lo + i - c))
    return lo + int(best), float(seg.max())


def iou(a, b):
    ax, ay, aw, ah = a; bx, by, bw, bh = b
    iw, ih = min(ax + aw, bx + bw) - max(ax, bx), min(ay + ah, by + bh) - max(ay, by)
    if iw <= 0 or ih <= 0: return 0.0
    return iw * ih / (aw * ah + bw * bh - iw * ih)


def live_rect(widgets, rect):
    """The live widget (exact Slate rect from <live>.json) that best overlaps the concept rect; prefers the smallest on ties. None if IoU < 0.25."""
    best, best_key = None, (0.25, 0)
    for w in widgets:
        r = w.get('rect')
        if not r: continue
        key = (round(iou(rect, r), 3), -r[2] * r[3])
        if key > best_key: best, best_key = w, key
    return best


def sample(img, x, y):
    a = np.asarray(img)[max(0, y - 1):y + 2, max(0, x - 1):x + 2].reshape(-1, 3).mean(axis=0)
    return a


def run(screen, live_path, out_root):
    spec = json.loads((ELEMENTS / (screen + '.json')).read_text())
    concept = load(CONCEPTS / spec['concept']); live = load(live_path)
    out = Path(out_root) / screen; out.mkdir(parents=True, exist_ok=True)
    cg, lg = gray(concept), gray(live)
    jpath = Path(live_path).with_suffix('.json')
    widgets = [w for w in json.loads(jpath.read_text()) if w] if jpath.exists() else None
    diff = np.abs(np.asarray(concept, dtype=np.int16) - np.asarray(live, dtype=np.int16)).sum(axis=2) / 3
    heat = np.zeros((1080, 1920, 3), dtype=np.uint8); heat[..., 0] = np.clip(diff * 3, 0, 255); heat[..., 1] = np.clip(255 - diff * 3, 0, 255) // 4
    Image.fromarray(heat).save(out / 'diff-heat.png')
    Image.blend(concept, live, 0.5).save(out / 'overlay-50.png')
    concept.save(out / 'flip.gif', save_all=True, append_images=[live], duration=700, loop=0)
    rows, ok_all = [], True
    for el in spec['elements']:
        x, y, w, h = el['rect']
        match_name = ''
        if widgets is not None:  # exact live geometry from the engine
            w = live_rect(widgets, el['rect']); found = w is not None
            if found:
                lx, ly, lw, lh = w['rect']; dl, dt = round(lx - x), round(ly - y); dw, dh_ = round(lw - el['rect'][2]), round(lh - el['rect'][3])
                match_name = '%s:%s' % (w['class'], w['name'])
            else: dl = dt = dw = dh_ = None
            tl = br = (0, 0, iou(el['rect'], w['rect']) if found else 0.0)
        else:  # no widget dump: strongest visible edges per side
            ce = [edge_pos(cg, k, x, y, w, h) for k in 'ltrb']; le = [edge_pos(lg, k, x, y, w, h) for k in 'ltrb']
            found = all(c[1] >= 4 and l[1] >= 4 for c, l in zip(ce, le))
            dl, dt, dr, db = [l[0] - c[0] for c, l in zip(ce, le)] if found else (None,) * 4
            dw = (dr - dl) if found else None; dh_ = (db - dt) if found else None
            tl = br = (0, 0, min(min(c[1], l[1]) for c, l in zip(ce, le)) / 100.0)
        geo = found and max(abs(dl), abs(dt), abs(dw), abs(dh_)) <= TOL_PX
        des = []
        for sx, sy, label in el.get('samples', []):
            ox, oy = (dl, dt) if found else (0, 0)
            c = srgb_to_lab(sample(concept, sx, sy)); l = srgb_to_lab(sample(live, sx + ox, sy + oy))
            des.append((label, round(de2000(c, l), 1)))
        col = all(d <= TOL_DE for _, d in des)
        ok = bool(col and (geo or not found)); ok_all &= ok and found  # an unmeasurable edge is REVIEW for the reviewer, never a silent PASS
        rows.append(dict(widget=match_name, name=el['name'], rect=el['rect'], found=found, match=round(min(tl[2], br[2]), 2), d_left=dl, d_top=dt, d_width=dw, d_height=dh_, de=des, geometry=('PASS' if geo else 'FAIL') if found else 'UNMEASURED', colour='PASS' if col else 'FAIL', verdict=('PASS' if ok else 'FAIL') if found else ('REVIEW' if col else 'FAIL')))
    (out / 'report.json').write_text(json.dumps(rows, indent=1))
    lines = ['# %s: pixel diff, %s' % (screen, 'PASS' if ok_all else 'FAIL'), '', 'REVIEW = the element could not be measured (no live widget overlaps it, or no visible edge); REVIEW never counts as PASS and the reviewer must judge it by eye. Tolerance: geometry +-%d px (left/top/width/height), colour dE2000 <= %.0f. dx/dy = live minus concept; "-" = the corner patch was not found (no visible edge).' % (TOL_PX, TOL_DE), '',
             '| element | concept rect | live widget | dx | dy | dw | dh | IoU/edge | colour dE | verdict |', '| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |']
    for r in rows:
        f = lambda v: '-' if v is None else '%+d' % v
        lines.append('| %s | %s | %s | %s | %s | %s | %s | %.2f | %s | %s |' % (r['name'], r['rect'], r.get('widget') or '-', f(r['d_left']), f(r['d_top']), f(r['d_width']), f(r['d_height']), r['match'], ', '.join('%s %.1f' % d for d in r['de']) or '-', r['verdict']))
    lines += ['', 'Images: diff-heat.png (red = large difference), overlay-50.png, flip.gif (concept / live).']
    (out / 'report.md').write_text('\n'.join(lines) + '\n')
    print(screen, 'PASS' if ok_all else 'FAIL', sum(r['verdict'] == 'PASS' for r in rows), 'PASS', sum(r['verdict'] == 'REVIEW' for r in rows), 'REVIEW', sum(r['verdict'] == 'FAIL' for r in rows), 'FAIL of', len(rows))
    return ok_all


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('screen', nargs='?'); ap.add_argument('--all', action='store_true')
    ap.add_argument('--live'); ap.add_argument('--live-dir', default=str(ROOT / 'docs/evidence/ui-v5/final3')); ap.add_argument('--out', default=str(ROOT / 'docs/evidence/ui-v5/final3/pixel'))
    a = ap.parse_args()
    screens = [p.stem for p in sorted(ELEMENTS.glob('*.json'))] if a.all else [a.screen]
    ok = True
    for s in screens:
        live = a.live if a.live and not a.all else Path(a.live_dir) / (s + '-live.png')
        if not Path(live).exists() and (Path(a.live_dir) / ('9-' + s + '-live.png')).exists(): live = Path(a.live_dir) / ('9-' + s + '-live.png')
        if not Path(live).exists(): print(s, 'no live capture', live); ok = False; continue
        ok &= run(s, live, a.out)
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
