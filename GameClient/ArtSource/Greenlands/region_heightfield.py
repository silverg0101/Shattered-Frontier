"""Shape the coarse region heightfield from a region layout (step 0 of docs/settlement-world-pipeline.md).

Standard library only. Usage:
    python region_heightfield.py [region.json]   # writes <region>-heightfield-preview.png, prints REGION_TERRAIN_OK
Land: settlement datums and zone markers blended with rivers' banks, pulled down to the coast; mountain ranges
on top; rivers carved. Sea: shelves away from the coast. Heights in metres above sea level, north is -Y.
zone_edges() gives a tile's edge heights and the rivers and roads that cross its edge; zone builds read it.
"""
import json, math, random, struct, sys, zlib
from array import array
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from validate_region import in_polygon, tile_of, validate  # noqa: E402
from validate_layout import intersection, line_dist  # noqa: E402

DATUM_WEIGHT = 4  # settlement datums and zone markers count this many times a river or coast point


def _smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def _samples(pts, step):
    out = []
    for a, b in zip(pts, pts[1:]):
        n = max(1, math.ceil(math.dist(a, b) / step))
        out += [(a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n, k / n) for k in range(n)]
    return out + [(pts[-1][0], pts[-1][1], 1.0)]


def _river_samples(r, step):
    """(x, y, level) every `step` m along a river."""
    out = []
    for i, (a, b) in enumerate(zip(r['points'], r['points'][1:])):
        n = max(1, math.ceil(math.dist(a, b) / step))
        for k in range(n):
            t = k / n
            out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, r['levels'][i] + (r['levels'][i + 1] - r['levels'][i]) * t))
    return out


class ValueNoise:
    """Smoothly interpolated random values on a square lattice of `cell` metres, in 0..1."""
    def __init__(self, rng, bounds, cell):
        self.x0, self.y0, self.cell = bounds[0], bounds[1], cell
        self.n = int(max(bounds[2] - bounds[0], bounds[3] - bounds[1]) / cell) + 3
        self.v = [rng.random() for _ in range(self.n * self.n)]

    def at(self, x, y):
        fx, fy = (x - self.x0) / self.cell, (y - self.y0) / self.cell
        i, j = int(fx), int(fy); u, v = _smoothstep(fx - i), _smoothstep(fy - j)
        n, g = self.n, self.v
        return (g[j * n + i] * (1 - u) + g[j * n + i + 1] * u) * (1 - v) + (g[(j + 1) * n + i] * (1 - u) + g[(j + 1) * n + i + 1] * u) * v


class RegionHeightfield:
    def __init__(self, R, spacing=50.0):
        self.R, self.step = R, spacing
        self.x0, self.y0, x1, y1 = R['bounds']
        self.nx, self.ny = int((x1 - self.x0) / spacing) + 1, int((y1 - self.y0) / spacing) + 1
        coast = R['coastline']; ring = coast + coast[:1]
        ctrl = [(tuple(s['at']), s['datum'], DATUM_WEIGHT) for s in R['settlements'].values()]
        for s in R['settlements'].values():
            if 'zone_layout' in s:
                Z = json.loads((ROOT / s['zone_layout']).read_text(encoding='utf-8'))
                ctrl += [((s['at'][0] + p[0], s['at'][1] + p[1]), s['datum'] + h, DATUM_WEIGHT) for p, h in Z['elevation']['markers'].values()]
        for r in R['rivers'].values():
            ctrl += [((x, y), lv + 3, 1) for x, y, lv in _river_samples(r, 250)]
        ctrl += [((x, y), 2, 1) for x, y, _ in _samples(ring, 250)]

        def idw(x, y):
            num = den = 0.0
            for (px, py), h, w in ctrl:
                d2 = (x - px) ** 2 + (y - py) ** 2 + 1
                num += w * h / d2; den += w / d2
            return num / den

        cs = 250.0
        cnx, cny = int((x1 - self.x0) / cs) + 1, int((y1 - self.y0) / cs) + 1
        coarse = [[idw(self.x0 + i * cs, self.y0 + j * cs) for i in range(cnx)] for j in range(cny)]
        rng = random.Random(7)
        hills = [(ValueNoise(rng, R['bounds'], cell), amp) for cell, amp in ((2200, 36.0), (900, 16.0), (350, 5.0))]
        ridges = [(ValueNoise(rng, R['bounds'], cell), wt) for cell, wt in ((1500, 0.55), (650, 0.3), (280, 0.15))]
        self.h = array('d', bytes(8 * self.nx * self.ny))
        self.land = bytearray(self.nx * self.ny)
        for j in range(self.ny):
            y = self.y0 + j * spacing
            fy = min((y - self.y0) / cs, cny - 1.001); cj = int(fy); v = fy - cj
            for i in range(self.nx):
                x = self.x0 + i * spacing
                k = j * self.nx + i
                dc = line_dist((x, y), ring)
                if not in_polygon((x, y), coast):
                    self.h[k] = -min(40.0, 2 + dc * 0.02)
                    continue
                self.land[k] = 1
                fx = min((x - self.x0) / cs, cnx - 1.001); ci = int(fx); u = fx - ci
                base = ((coarse[cj][ci] * (1 - u) + coarse[cj][ci + 1] * u) * (1 - v)
                        + (coarse[cj + 1][ci] * (1 - u) + coarse[cj + 1][ci + 1] * u) * v)
                base += sum(amp * (2 * nz.at(x, y) - 1) for nz, amp in hills) * _smoothstep(dc / 1200)
                base = 1 + (base - 1) * _smoothstep(dc / 600)  # beaches and low coast
                peak = 0.0
                for mt in R['mountains'].values():
                    f = _smoothstep(1 - line_dist((x, y), mt['points']) / mt['width'])
                    if f > 0:
                        ridged = sum(wt * (1 - abs(2 * nz.at(x, y) - 1)) ** 2 for nz, wt in ridges)
                        peak = max(peak, mt['height'] * f * (0.35 + 0.65 * ridged))
                self.h[k] = max(base, peak)
        for s in R['settlements'].values():  # each settlement stands on ground at its datum
            for k, cx, cy in self._cells(*s['at'], 600):
                if self.land[k]:
                    self.h[k] += (s['datum'] - self.h[k]) * (1 - _smoothstep((math.dist((cx, cy), s['at']) - 300) / 300))
        for r in R['rivers'].values():  # carve: water level in the channel, banks rising away from it
            hw = max(r['width'] / 2, spacing)  # the coarse grid can't hold a narrower channel
            for x, y, lv in _river_samples(r, spacing / 2):
                for k, cx, cy in self._cells(x, y, hw + 300):
                    if self.land[k]:
                        d = math.hypot(cx - x, cy - y)
                        self.h[k] = min(self.h[k], lv - 2 if d <= hw else lv + (d - hw) * 0.12)

    def _cells(self, x, y, r):
        s = self.step
        for j in range(max(0, int((y - r - self.y0) / s)), min(self.ny, int((y + r - self.y0) / s) + 2)):
            for i in range(max(0, int((x - r - self.x0) / s)), min(self.nx, int((x + r - self.x0) / s) + 2)):
                yield j * self.nx + i, self.x0 + i * s, self.y0 + j * s

    def at(self, x, y):
        fx = min(max((x - self.x0) / self.step, 0), self.nx - 1.001)
        fy = min(max((y - self.y0) / self.step, 0), self.ny - 1.001)
        i, j = int(fx), int(fy); u, v = fx - i, fy - j
        h, n = self.h, self.nx
        return (h[j * n + i] * (1 - u) + h[j * n + i + 1] * u) * (1 - v) + (h[(j + 1) * n + i] * (1 - u) + h[(j + 1) * n + i + 1] * u) * v


def zone_edges(R, hf, settlement, step=50):
    """What a zone inherits from the region along its tile edge: heights every `step` m (clockwise from the
    north-west corner) and the points where rivers and roads cross the edge."""
    ts = R['tile_size']
    c, r = tile_of(R, R['settlements'][settlement]['at'])
    x0, y0 = R['bounds'][0] + c * ts, R['bounds'][1] + r * ts
    corners = [(x0, y0), (x0 + ts, y0), (x0 + ts, y0 + ts), (x0, y0 + ts), (x0, y0)]
    edge = [(round(x), round(y), round(hf.at(x, y), 1)) for x, y, _ in _samples(corners, step)][:-1]
    crossings = []
    S = R['settlements']
    lines = [(name, rv['points']) for name, rv in R['rivers'].items()]
    lines += [(f'road {rd["between"][0]}-{rd["between"][1]}', rd['points']) for rd in R['roads']]
    for name, pts in lines:
        for a, b in zip(pts, pts[1:]):
            for e0, e1 in zip(corners, corners[1:]):
                hit = intersection(a, b, e0, e1)
                if hit:
                    crossings.append((name, round(hit[0]), round(hit[1])))
    return edge, list(dict.fromkeys(crossings))


def write_png(path, hf, R, scale=3):
    """Shaded relief: sea blues, land greens to rock and snow, tile grid, settlements in red."""
    n, ny = hf.nx, hf.ny
    def colour(h):
        if h < 0:
            t = min(1, -h / 40); return (40 - 20 * t, 90 - 40 * t, 160 - 50 * t)
        for top, c0, c1 in ((60, (70, 120, 55), (95, 140, 60)), (250, (95, 140, 60), (150, 140, 90)),
                            (700, (150, 140, 90), (140, 135, 130)), (1600, (140, 135, 130), (245, 245, 250))):
            if h <= top:
                lo = {60: 0, 250: 60, 700: 250, 1600: 700}[top]; t = (h - lo) / (top - lo)
                return tuple(a + (b - a) * t for a, b in zip(c0, c1))
        return (245, 245, 250)
    px = [[None] * n for _ in range(ny)]
    for j in range(ny):
        for i in range(n):
            k = j * n + i; h = hf.h[k]
            dx = hf.h[k + 1] - hf.h[k - 1] if 0 < i < n - 1 else 0
            dy = hf.h[k + n] - hf.h[k - n] if 0 < j < ny - 1 else 0
            shade = max(0.45, min(1.25, 1 + (-dx - dy) / (4 * hf.step) * 3)) if h > 0 else 1
            px[j][i] = tuple(max(0, min(255, int(c * shade))) for c in colour(h))
    ts = R['tile_size']
    for j in range(ny):
        for i in range(n):
            x, y = hf.x0 + i * hf.step, hf.y0 + j * hf.step
            if (x - hf.x0) % ts == 0 or (y - hf.y0) % ts == 0:
                px[j][i] = (20, 20, 20)
    for s in R['settlements'].values():
        ci, cj = int((s['at'][0] - hf.x0) / hf.step), int((s['at'][1] - hf.y0) / hf.step)
        for dj in range(-2, 3):
            for di in range(-2, 3):
                if 0 <= cj + dj < ny and 0 <= ci + di < n:
                    px[cj + dj][ci + di] = (220, 30, 30)
    raw = b''.join(b'\x00' + b''.join(bytes(c) for c in row for _ in range(scale)) for row in px for _ in range(scale))
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    Path(path).write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', n * scale, ny * scale, 8, 2, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


if __name__ == '__main__':
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'greenlands-region-layout.json'
    R = json.loads(path.read_text(encoding='utf-8'))
    problems = validate(R)
    assert not problems, f'Region layout is invalid; run validate_region.py: {problems}'
    hf = RegionHeightfield(R)
    for name, s in R['settlements'].items():  # settlements stand on land near their datum
        h = hf.at(*s['at'])
        assert h > 0 and abs(h - s['datum']) < 25, f'{name} ground {h:.0f} m vs datum {s["datum"]} m'
    for name, rv in R['rivers'].items():
        for x, y, lv in _river_samples(rv, 200):
            assert hf.at(x, y) <= lv + 1, f'{name} bed above water at {x:.0f},{y:.0f}'
    edge, crossings = zone_edges(R, hf, 'Silverbrook')
    assert any(c[0] == 'Silverbrook River' for c in crossings), 'Silverbrook River must cross its tile edge'
    out = path.with_name(path.stem.replace('-layout', '') + '-heightfield-preview.png')
    write_png(out, hf, R)
    hs = [h for h, l in zip(hf.h, hf.land) if l]
    print(f'REGION_TERRAIN_OK {path.name}: {hf.nx}x{hf.ny} at {hf.step:.0f} m, land {min(hs):.0f}..{max(hs):.0f} m; '
          f'Silverbrook edge {min(e[2] for e in edge):.0f}..{max(e[2] for e in edge):.0f} m, crossings {crossings}; preview {out.name}')
