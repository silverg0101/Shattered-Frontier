"""Shape a zone heightfield from a settlement layout's elevation data (stage 2 of docs/settlement-world-pipeline.md).

Standard library only, so the shape can be checked without Unreal. Usage:
    python heightfield.py [layout.json]     # checks landmarks, river banks and road grades; prints TERRAIN_OK
Order: control points (markers, ridges, river banks, cliffs) -> smooth base -> rolling hills ->
flat building pads -> graded road beds -> river channel. Later steps win. Heights in metres,
north is -Y, east is +X. The grid is a regenerable stand-in until TER-01 picks the surface type.
"""
import json, math, random, sys
from array import array
from pathlib import Path

RIVER_BANK = 1.5    # m above water where the river meets the land
CHANNEL_DEPTH = 2.0  # m below water at the channel centre
BANK_SLOPE = 0.6     # m of rise per m away from the water's edge
SHOULDER = 6.0       # m blend outside a road bed or building pad


def _samples(pts, step):
    """Points every `step` m along a polyline, with (segment index, t) for each."""
    out = []
    for i, (a, b) in enumerate(zip(pts, pts[1:])):
        n = max(1, math.ceil(math.dist(a, b) / step))
        out += [((a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n), i, k / n) for k in range(n)]
    out.append((tuple(pts[-1]), len(pts) - 2, 1.0))
    return out


def river_level(elev, i, t):
    """Water surface at fraction t of river segment i; declared waterfalls drop at the segment midpoint."""
    h0, h1 = elev['river_levels'][i], elev['river_levels'][i + 1]
    if [i, i + 1] in [w['between_points'] for w in elev['waterfalls']]:
        return h0 if t < 0.5 else h1
    return h0 + (h1 - h0) * t


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
        self.road_profiles = self._roads(elev['flatten']['road_max_grade'])
        self._river(elev)

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
        river = self.L['river']['points']
        ctrl = [(tuple(p), h) for p, h in elev['markers'].values()]
        ctrl += [(p, r['height']) for r in elev['ridges'].values() for p, _, _ in _samples(r['points'], 50)]
        ctrl += [(p, river_level(elev, i, t) + RIVER_BANK) for p, i, t in _samples(river, 50)]

        def idw(x, y, pts):
            num = den = 0.0
            for (px, py), h in pts:
                d2 = (x - px) ** 2 + (y - py) ** 2
                if d2 < 1:
                    return h
                w = 1 / d2
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
                ctrl += [((x + 6 * nx, y + 6 * ny), mid + c['drop'] / 2), ((x - 6 * nx, y - 6 * ny), mid - c['drop'] / 2)]

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
                 for amp, wl in ((3.0, 420), (1.8, 190), (0.7, 70))]
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

    def _roads(self, max_grade):
        """Grade-limited centreline heights, then a flat bed with blended shoulders. Returns profiles for checks."""
        profiles, samples = {}, []
        for tier in ('primary', 'secondary', 'trail'):
            hw = max(self.L['roads'][f'{tier}_width'] / 2, self.step)  # the grid can't hold a narrower bed
            for name, pts in self.L['roads'][tier].items():
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
                # Tilt back so both ends keep the terrain height and junctions agree (adds about 0.01 grade).
                d0, d1, n = z[0] - avg[0], z[-1] - avg[-1], len(z) - 1
                z = [a + d0 + (d1 - d0) * k / n for k, a in enumerate(avg)]
                profiles[name] = [(p, h) for (p, _, _), h in zip(s, z)]
                for ((p, _, _), a), ((q, _, _), b) in zip(zip(s, z), zip(s[1:], z[1:])):  # 1 m stamps, no steps
                    n = max(1, round(math.dist(p, q)))
                    samples += [((p[0] + (q[0] - p[0]) * k / n, p[1] + (q[1] - p[1]) * k / n), (a + (b - a) * k / n, hw))
                                for k in range(n)]
        for k, (d, (z, hw)) in self._nearest(samples, 4 + SHOULDER + 5).items():
            self.h[k] += (z - self.h[k]) * (1 - _smoothstep((d - hw) / SHOULDER))
        return profiles

    def _pad(self, x, y, r):
        z = self.at(x, y)
        for k, cx, cy in self._cells(x, y, r + SHOULDER):
            d = math.hypot(cx - x, cy - y)
            self.h[k] += (z - self.h[k]) * (1 - _smoothstep((d - r) / SHOULDER))

    def _river(self, elev):
        """Carve the channel below water; banks rise away from the edge and never sit below the water."""
        hw = self.L['river']['width'] / 2
        samples = [(p, river_level(elev, i, t)) for p, i, t in _samples(self.L['river']['points'], 2.5)]
        for k, (d, level) in self._nearest(samples, hw + 40).items():
            if d <= hw - 4:
                bed = level - CHANNEL_DEPTH
            elif d <= hw:
                bed = level - CHANNEL_DEPTH + (CHANNEL_DEPTH + 0.3) * (d - hw + 4) / 4
            else:
                bed = level + 0.3 + (d - hw) * BANK_SLOPE
                self.h[k] = max(self.h[k], level + 0.3)  # banks always hold the water
            self.h[k] = min(self.h[k], bed)


def check(L, hf):
    """Exit checks from the pipeline's workflow step 3. Returns (problems, stats)."""
    problems, elev = [], L['elevation']
    errs = {name: hf.at(*p) - h for name, (p, h) in elev['markers'].items()}
    for name, e in errs.items():
        if abs(e) > 6:
            problems.append(f'{name} is {e:+.1f} m off its marker')
    hw = L['river']['width'] / 2
    pts = L['river']['points']
    for (x, y), i, t in _samples(pts, 25):
        level = river_level(elev, i, t)
        a, b = pts[i], pts[i + 1]
        nx, ny = b[1] - a[1], a[0] - b[0]; n = math.hypot(nx, ny) or 1
        for side in (-1, 1):
            bx, by = x + side * (hw + 3) * nx / n, y + side * (hw + 3) * ny / n
            if abs(bx) < L['zone_half_extent'] and abs(by) < L['zone_half_extent']                     and _dist((bx, by), pts) > hw + 1 and hf.at(bx, by) < level:  # skip points back over the water
                problems.append(f'river bank below water at {bx:.0f},{by:.0f}')
        a_fall = [i, i + 1] in [w['between_points'] for w in elev['waterfalls']]
        if hf.at(x, y) > level - 1 and not (a_fall and abs(t - 0.5) * math.dist(a, b) < 15):
            problems.append(f'river bed above water at {x:.0f},{y:.0f}')
    worst = 0.0
    river_clear = hw + 20  # bridges span the channel and banks
    for name, prof in hf.road_profiles.items():
        for (p, _), (q, _) in zip(prof, prof[1:]):
            gap = math.dist(p, q)
            if gap < 1 or min(_dist(p, pts), _dist(q, pts)) < river_clear:
                continue
            g = abs(hf.at(*q) - hf.at(*p)) / gap
            worst = max(worst, g)
            if g > elev['flatten']['road_max_grade'] + 0.02:
                problems.append(f'{name} grade {g:.2f} at {p[0]:.0f},{p[1]:.0f}')
                break
    lo, hi = min(hf.h), max(hf.h)
    return problems, {'markers_max_error': round(max(abs(e) for e in errs.values()), 1),
                      'road_max_grade': round(worst, 3), 'height_range': [round(lo, 1), round(hi, 1)]}


def _dist(p, pts):
    best = 1e18
    for a, b in zip(pts, pts[1:]):
        dx, dy = b[0] - a[0], b[1] - a[1]
        t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / ((dx * dx + dy * dy) or 1)))
        best = min(best, math.dist(p, (a[0] + t * dx, a[1] + t * dy)))
    return best


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
    print(f'TERRAIN_OK {path.name}: {hf.nx}x{hf.ny} at {hf.step} m, {stats}')
