"""Shape a zone heightfield from a settlement layout's elevation data (stage 2 of docs/settlement-world-pipeline.md).

Standard library only, so the shape can be checked without Unreal. Usage:
    python heightfield.py [layout.json]     # checks landmarks, banks, basins and road grades; prints TERRAIN_OK
Order: control points (markers, ridges, water banks, cliffs) -> smooth base -> rolling hills ->
flat building pads -> graded road beds -> river and brook channels -> basins. Later steps win.
backdrop() adds the mountains beyond the zone edge. Heights in metres, north is -Y, east is +X.
The grid is a regenerable stand-in until TER-01 picks the surface type.
"""
import json, math, random, sys
from array import array
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from validate_layout import waters  # noqa: E402

WATER_BANK = 1.5     # m above water where water meets the land
CHANNEL_DEPTH = 2.0  # m below water at a channel centre
BASIN_DEPTH = 3.0    # m below water in the middle of a basin
BANK_SLOPE = 0.6     # m of rise per m away from the water's edge
SHOULDER = 6.0       # m blend outside a road bed or building pad
MARKER_WEIGHT = 4    # elevation markers count this many times a ridge or bank control point


def _samples(pts, step):
    """Points every `step` m along a polyline, with (segment index, t) for each."""
    out = []
    for i, (a, b) in enumerate(zip(pts, pts[1:])):
        n = max(1, math.ceil(math.dist(a, b) / step))
        out += [((a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n), i, k / n) for k in range(n)]
    out.append((tuple(pts[-1]), len(pts) - 2, 1.0))
    return out


def water_level(w, i, t):
    """Surface of water w at fraction t of segment i; declared waterfalls drop at the segment midpoint."""
    h0, h1 = w['levels'][i], w['levels'][i + 1]
    if [i, i + 1] in [f['between_points'] for f in w['waterfalls']]:
        return h0 if t < 0.5 else h1
    return h0 + (h1 - h0) * t


def in_polygon(p, poly):
    hit = False
    for (ax, ay), (bx, by) in zip(poly, poly[1:] + poly[:1]):
        if (ay > p[1]) != (by > p[1]) and p[0] < ax + (p[1] - ay) * (bx - ax) / (by - ay):
            hit = not hit
    return hit


def _dist(p, pts):
    best = 1e18
    for a, b in zip(pts, pts[1:]):
        dx, dy = b[0] - a[0], b[1] - a[1]
        t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / ((dx * dx + dy * dy) or 1)))
        best = min(best, math.dist(p, (a[0] + t * dx, a[1] + t * dy)))
    return best


def _smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


class Heightfield:
    def __init__(self, L, pads=(), spacing=5.0):
        """pads: (x, y, radius) footprints to flatten, e.g. the builder's placed buildings."""
        self.L, self.step = L, spacing
        self.x0, self.y0, x1, y1 = L['zone_bounds']
        self.nx = int(round((x1 - self.x0) / spacing)) + 1
        self.ny = int(round((y1 - self.y0) / spacing)) + 1
        elev = L['elevation']
        self.h = array('d', bytes(8 * self.nx * self.ny))
        self._base(elev)
        self._hills(L['seed'])
        for x, y, r in pads:
            self._pad(x, y, r + elev['flatten']['under_buildings_margin'])
        self.road_profiles = self._roads(elev['flatten'])
        self.basins = [b['polygon'] for b in L.get('basins', [])]
        for b in L.get('basins', []):
            self._basin(b)
        for w in waters(L):
            self._channel(w)

    # --- grid access -------------------------------------------------------
    def at(self, x, y):
        """Bilinear height at (x, y) metres."""
        fx = min(max((x - self.x0) / self.step, 0), self.nx - 1.001)
        fy = min(max((y - self.y0) / self.step, 0), self.ny - 1.001)
        i, j = int(fx), int(fy); u, v = fx - i, fy - j
        h, n = self.h, self.nx
        return (h[j * n + i] * (1 - u) + h[j * n + i + 1] * u) * (1 - v) + (h[(j + 1) * n + i] * (1 - u) + h[(j + 1) * n + i + 1] * u) * v

    def _cells(self, x, y, r):
        """Grid indices and cell positions within r of (x, y)."""
        s = self.step
        for j in range(max(0, int((y - r - self.y0) / s)), min(self.ny, int((y + r - self.y0) / s) + 2)):
            for i in range(max(0, int((x - r - self.x0) / s)), min(self.nx, int((x + r - self.x0) / s) + 2)):
                yield j * self.nx + i, self.x0 + i * s, self.y0 + j * s

    def _nearest(self, samples, radius):
        """Map cell -> (distance, payload) of the nearest sample within radius."""
        best = {}
        for (x, y), payload in samples:
            for k, cx, cy in self._cells(x, y, radius):
                d = math.hypot(cx - x, cy - y)
                if d <= radius and (k not in best or d < best[k][0]):
                    best[k] = (d, payload)
        return best

    # --- shaping steps -----------------------------------------------------
    def _base(self, elev):
        """Inverse-distance blend of control points on a 50 m grid, then bilinear onto the fine grid."""
        ctrl = [(tuple(p), h, MARKER_WEIGHT) for p, h in elev['markers'].values()]
        ctrl += [(p, r['height'], 1) for r in elev['ridges'].values() for p, _, _ in _samples(r['points'], 50)]
        for w in waters(self.L):
            ctrl += [(p, water_level(w, i, t) + WATER_BANK, 1) for p, i, t in _samples(w['points'], 50)]
        for b in self.L.get('basins', []):
            ctrl += [(tuple(p), b['level'] + WATER_BANK, 1) for p in b['polygon']]

        def idw(x, y, pts):
            num = den = 0.0
            for (px, py), h, wt in pts:
                d2 = (x - px) ** 2 + (y - py) ** 2
                if d2 < 1:
                    return h
                w = wt / d2
                num += w * h; den += w
            return num / den

        # Cliffs: a steep step across the line, high side facing away from the town centre.
        for c in elev['cliffs'].values():
            for (x, y), i, _ in _samples(c['points'], 20):
                a, b = c['points'][i], c['points'][i + 1]
                nx, ny = b[1] - a[1], a[0] - b[0]; n = math.hypot(nx, ny); nx, ny = nx / n, ny / n
                if nx * x + ny * y < 0:
                    nx, ny = -nx, -ny
                mid = idw(x, y, ctrl)
                ctrl += [((x + 6 * nx, y + 6 * ny), mid + c['drop'] / 2, 1), ((x - 6 * nx, y - 6 * ny), mid - c['drop'] / 2, 1)]

        cs = 50.0
        cn = int((self.nx - 1) * self.step / cs) + 1
        coarse = [[idw(self.x0 + i * cs, self.y0 + j * cs, ctrl) for i in range(cn)] for j in range(cn)]
        for j in range(self.ny):
            fy = min(j * self.step / cs, cn - 1.001); cj = int(fy); v = fy - cj
            for i in range(self.nx):
                fx = min(i * self.step / cs, cn - 1.001); ci = int(fx); u = fx - ci
                self.h[j * self.nx + i] = ((coarse[cj][ci] * (1 - u) + coarse[cj][ci + 1] * u) * (1 - v)
                                           + (coarse[cj + 1][ci] * (1 - u) + coarse[cj + 1][ci + 1] * u) * v)

    def _hills(self, seed):
        """Seeded rolling hills: gentle inside the wall, stronger in the countryside."""
        rng = random.Random(seed)
        waves = [(amp, wl, rng.uniform(0, math.tau), rng.uniform(0, math.tau), rng.uniform(0, math.pi))
                 for amp, wl in ((4.0, 420), (2.2, 190), (0.8, 70))]
        W = self.L['wall']
        for j in range(self.ny):
            y = self.y0 + j * self.step
            for i in range(self.nx):
                x = self.x0 + i * self.step
                e = math.hypot(x / W['semi_axis_x'], y / W['semi_axis_y'])  # 1 on the wall
                amp = 0.3 + 0.7 * _smoothstep(e - 0.8)
                n = 0.0
                for a, wl, p, q, ang in waves:
                    u = (x * math.cos(ang) + y * math.sin(ang)) / wl
                    v = (-x * math.sin(ang) + y * math.cos(ang)) / wl
                    n += a * math.sin(math.tau * u + p) * math.cos(math.tau * v + q)
                self.h[j * self.nx + i] += amp * n

    def _roads(self, flatten):
        """Grade-limited centreline heights, then a flat bed with blended shoulders. Roads go down in order;
        a finished bed is locked, and a later road is pinned to it where they cross. Returns profiles for checks."""
        profiles, locked = {}, bytearray(self.nx * self.ny)
        cell = lambda p: int(round((p[1] - self.y0) / self.step)) * self.nx + int(round((p[0] - self.x0) / self.step))
        for tier in ('primary', 'secondary', 'trail'):
            hw = max(self.L['roads'][f'{tier}_width'] / 2, self.step)  # the grid can't hold a narrower bed
            max_grade = flatten.get(f'{tier}_max_grade', flatten['road_max_grade'])
            for name, pts in self.L['roads'][tier].items():
                samples = []
                s = _samples(pts, 5)
                z = [self.at(*p) for p, _, _ in s]
                gaps = [math.dist(a[0], b[0]) for a, b in zip(s, s[1:])]
                # Cut-only and fill-only envelopes are both exactly within grade; their average balances cut and fill.
                cut, fill = z[:], z[:]
                for order in (range(1, len(z)), range(len(z) - 2, -1, -1)):
                    for k in order:
                        j = k - 1 if order.step == 1 else k + 1
                        lim = max_grade * gaps[min(k, j)]
                        cut[k] = min(cut[k], cut[j] + lim)
                        fill[k] = max(fill[k], fill[j] - lim)
                avg = [(a + b) / 2 for a, b in zip(cut, fill)]
                # Pins: both ends, and crossings with locked beds, keep the terrain height. Clamp into the band
                # every within-grade line through the pins allows, so pins hold and the grade stays exact.
                pins = {k for k, (p, _, _) in enumerate(s) if k in (0, len(s) - 1) or 0 <= cell(p) < len(locked) and locked[cell(p)]}
                lo, hi = [-1e9] * len(z), [1e9] * len(z)
                for order in (range(len(z)), range(len(z) - 1, -1, -1)):
                    prev = None
                    for k in order:
                        if prev is not None:
                            lim = max_grade * gaps[min(k, prev)]
                            lo[k], hi[k] = max(lo[k], lo[prev] - lim), min(hi[k], hi[prev] + lim)
                        if k in pins:
                            lo[k], hi[k] = max(lo[k], z[k]), min(hi[k], z[k])
                        prev = k
                z = [min(max(a, l), h) if l <= h else (l + h) / 2 for a, l, h in zip(avg, lo, hi)]
                profiles[name] = [(p, h) for (p, _, _), h in zip(s, z)]
                for ((p, _, _), a), ((q, _, _), b) in zip(zip(s, z), zip(s[1:], z[1:])):  # 1 m stamps, no steps
                    n = max(1, round(math.dist(p, q)))
                    samples += [((p[0] + (q[0] - p[0]) * k / n, p[1] + (q[1] - p[1]) * k / n), (a + (b - a) * k / n, hw))
                                for k in range(n)]
                bed = []
                for k, (d, (zz, hw_)) in self._nearest(samples, hw + SHOULDER + 1).items():
                    if not locked[k]:
                        self.h[k] += (zz - self.h[k]) * (1 - _smoothstep((d - hw_) / SHOULDER))
                        if d <= hw_:
                            bed.append(k)
                for k in bed:
                    locked[k] = 1
        return profiles

    def _pad(self, x, y, r):
        z = self.at(x, y)
        for k, cx, cy in self._cells(x, y, r + SHOULDER):
            d = math.hypot(cx - x, cy - y)
            self.h[k] += (z - self.h[k]) * (1 - _smoothstep((d - r) / SHOULDER))

    def half_width(self, w):
        return max(w['width'] / 2, self.step)  # the grid can't hold a narrower channel

    def _channel(self, w):
        """Carve a channel below water; banks rise away from the edge and never sit below the water."""
        hw = self.half_width(w)
        edge = min(4, hw / 2)
        samples = [(p, water_level(w, i, t)) for p, i, t in _samples(w['points'], 2.5)]
        for k, (d, level) in self._nearest(samples, hw + 40).items():
            if d <= hw - edge:
                bed = level - CHANNEL_DEPTH
            elif d <= hw:
                bed = level - CHANNEL_DEPTH + (CHANNEL_DEPTH + 0.3) * (d - hw + edge) / edge
            else:
                bed = level + 0.3 + (d - hw) * BANK_SLOPE
                x, y = self.x0 + k % self.nx * self.step, self.y0 + k // self.nx * self.step
                if not any(in_polygon((x, y), b) for b in self.basins):
                    self.h[k] = max(self.h[k], level + 0.3)  # banks always hold the water
            self.h[k] = min(self.h[k], bed)

    def _basin(self, b):
        """Lower the inside of a basin below its level; the shore rises like a river bank."""
        poly, level = b['polygon'], b['level']
        ring = poly + poly[:1]
        xs, ys = [p[0] for p in poly], [p[1] for p in poly]
        cx, cy, r = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, max(max(xs) - min(xs), max(ys) - min(ys)) / 2 + 40
        for k, x, y in self._cells(cx, cy, r):
            d = _dist((x, y), ring)
            if in_polygon((x, y), poly):
                self.h[k] = min(self.h[k], level - min(BASIN_DEPTH, 0.5 + d * 0.3))
            elif d < 40:
                if d < 15:
                    self.h[k] = max(self.h[k], level + 0.3)  # channels carved later cut through this shore
                self.h[k] = min(self.h[k], level + 0.3 + d * BANK_SLOPE)


def backdrop(L, hf):
    """Heights on a coarse grid out to backdrop.extent: zone-edge height plus the mountain ranges.
    Vertices inside the zone sit under its terrain. Returns (n, spacing, heights row-major, x fastest)."""
    B, H = L['backdrop'], L['zone_half_extent']
    ext, sp = B['extent'], B['spacing']
    n = int(round(2 * ext / sp)) + 1
    rng = random.Random(L['seed'] + 7)
    waves = [(wl, rng.uniform(0, math.tau), rng.uniform(0, math.tau), wt) for wl, wt in ((1700, 0.5), (900, 0.3), (420, 0.2))]
    z = []
    for j in range(n):
        y = -ext + j * sp
        for i in range(n):
            x = -ext + i * sp
            out = max(abs(x) - H, abs(y) - H)
            if out < -1:  # inside: tuck under the zone terrain, deep unless next to the edge
                if out < -sp - 1:
                    z.append(-100.0)
                else:
                    z.append(min(hf.at(x + dx, y + dy) for dx in range(-sp, sp + 1, 25) for dy in range(-sp, sp + 1, 25)) - 3)
                continue
            edge = hf.at(max(-H, min(H, x)), max(-H, min(H, y)))
            m = 0.0
            for r in B['ranges'].values():
                m = max(m, r['height'] * _smoothstep(1 - _dist((x, y), r['points']) / r['width']))
            ridged = sum(wt * (1 - abs(math.sin(math.tau * x / wl + p) + math.sin(math.tau * y / (wl * 0.8) + q)) / 2)
                         for wl, p, q, wt in waves)
            z.append(edge - (0.3 if out < 1 else 0) + m * (0.35 + 0.65 * ridged) * _smoothstep(out / B['ramp']))
    return n, sp, z


def check(L, hf):
    """Exit checks from the pipeline's workflow step 3. Returns (problems, stats)."""
    problems, elev = [], L['elevation']
    errs = {name: hf.at(*p) - h for name, (p, h) in elev['markers'].items()}
    for name, e in errs.items():
        if abs(e) > 6:
            problems.append(f'{name} is {e:+.1f} m off its marker')
    W = waters(L)
    basins = [b['polygon'] for b in L.get('basins', [])]
    wet = lambda q, pad: any(in_polygon(q, b) for b in basins) or any(_dist(q, w['points']) < w['width'] / 2 + pad for w in W)
    for w in W:
        hw, pts = hf.half_width(w) + hf.step, w['points']
        falls = [f['between_points'] for f in w['waterfalls']]
        for (x, y), i, t in _samples(pts, 25):
            level = water_level(w, i, t)
            a, b = pts[i], pts[i + 1]
            near_fall = [i, i + 1] in falls and abs(t - 0.5) * math.dist(a, b) < 15
            nx, ny = b[1] - a[1], a[0] - b[0]; n = math.hypot(nx, ny) or 1
            for side in (-1, 1):
                bx, by = x + side * hw * nx / n, y + side * hw * ny / n
                if abs(bx) < L['zone_half_extent'] and abs(by) < L['zone_half_extent'] and not near_fall \
                        and not wet((bx, by), 1) and hf.at(bx, by) < level:
                    problems.append(f'{w["name"]} bank below water at {bx:.0f},{by:.0f}')
            if hf.at(x, y) > level - 1 and not near_fall and not any(in_polygon((x, y), b) for b in basins):
                problems.append(f'{w["name"]} bed above water at {x:.0f},{y:.0f}')
    for b in L.get('basins', []):
        xs, ys = [p[0] for p in b['polygon']], [p[1] for p in b['polygon']]
        centre = (sum(xs) / len(xs), sum(ys) / len(ys))
        if hf.at(*centre) > b['level'] - 1:
            problems.append(f'{b["name"]} floor above water')
    worst = 0.0
    tier_of = {name: tier for tier in ('primary', 'secondary', 'trail') for name in L['roads'][tier]}
    for name, prof in hf.road_profiles.items():
        limit = elev['flatten'].get(f'{tier_of[name]}_max_grade', elev['flatten']['road_max_grade'])
        for (p, _), (q, _) in zip(prof, prof[1:]):
            gap = math.dist(p, q)
            if gap < 1 or wet(p, 20) or wet(q, 20):  # bridges span the water and banks
                continue
            g = abs(hf.at(*q) - hf.at(*p)) / gap
            if tier_of[name] != 'trail':
                worst = max(worst, g)
            if g > limit + 0.02:
                problems.append(f'{name} grade {g:.2f} at {p[0]:.0f},{p[1]:.0f}')
                break
    lo, hi = min(hf.h), max(hf.h)
    return problems, {'markers_max_error': round(max(abs(e) for e in errs.values()), 1),
                      'road_max_grade': round(worst, 3), 'height_range': [round(lo, 1), round(hi, 1)]}


if __name__ == '__main__':
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent / 'silverbrook-town-layout.json'
    layout = json.loads(path.read_text(encoding='utf-8'))
    hf = Heightfield(layout)
    problems, stats = check(layout, hf)
    for p in problems:
        print('FAIL', p)
    flat = Heightfield(dict(layout, elevation=dict(layout['elevation'], river_levels=[30] * len(layout['river']['points']))))
    assert check(layout, flat)[0], 'self-test: a river drawn above its banks must fail'
    if problems:
        sys.exit(1)
    n, sp, z = backdrop(layout, hf)
    print(f'TERRAIN_OK {path.name}: {hf.nx}x{hf.ny} at {hf.step} m, {stats}, backdrop {n}x{n} at {sp} m up to {max(z):.0f} m')
