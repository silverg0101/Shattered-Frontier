"""Decide where a district's dressing goes, from the biome look profile (stage 3 of docs/settlement-world-pipeline.md).

Standard library only, so the rules run and are checked without Unreal; build_silverbrook.py hands the result to a
PCG graph that spawns it as instanced meshes. Usage:
    python dress_zone.py [district]      # default Eastwood; checks the rules and determinism, prints DRESS_OK
Each role's 'place' block in the look profile sets density, spacing, slope, height above the valley floor, sun or
shade, clearance from roads, water and buildings, scale and sink. Canopy goes first, so the layers below can tell
shade from sun. Roles with no mesh borrow a fallback role's meshes and are reported as gaps. A district's 'dress' block
in the layout scales each layer's density (forest 1 by default; town streets and fields far less).
"""
import hashlib, json, math, random, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from heightfield import in_polygon, _dist  # noqa: E402
from validate_layout import waters  # noqa: E402

RULES_VERSION = 1  # bump when a rule here changes meaning; part of the content version
CELL = 2.5         # m, raster for clearances, slope and valley height
SHADE = 6.0        # m from a canopy trunk counts as shade
CULL_BOUNDS = (10, 200)  # m, bounds on a role's place.cull_m (pcg_dressing.py applies it as the instance end-cull distance)
SOLID = 2.5        # m kept between any two trees or rocks, whatever their role
SOLID_LAYERS = ('canopy', 'rocks')
ORDER = ('canopy', 'rocks', 'understory', 'ground_cover', 'litter')
# Keep the shipped Silverbrook distribution when decoupling placement RNG from
# content_version. New layouts use the explicit layout seed and district name.
SHIPPED = {
    # (legacy RNG seed, transform hash). Keep existing candidate IDs when transforms match.
    (1019, 'Northridge'): ('northridge-1019-1-8eb710c87a', '57862b8cef95'),
    (1019, 'The Commons'): ('the_commons-1019-1-b4f08fb645', '6d2e636bb32b'),
    (1019, 'Riverside'): ('riverside-1019-1-d78654a13d', '93d124f5f086'),
    (1019, 'Westfields'): ('westfields-1019-1-27e683c3b2', '19b5d972fcc8'),
    (1019, 'Eastwood'): ('eastwood-1019-1-3a26e33f1b', '47193802f220'),
}


def content_version(seed, district, roles):
    """Identify the generated placement, so unrelated layout edits cannot change candidate IDs."""
    h = hashlib.sha1(json.dumps(roles, sort_keys=True).encode()).hexdigest()
    shipped = SHIPPED.get((seed, district))
    if shipped and h.startswith(shipped[1]):
        return shipped[0]
    return f'{district.lower().replace(" ", "_")}-{seed}-{RULES_VERSION}-{h[:10]}'


class Site:
    """Rasters over one district: inside, clearance to roads, water and buildings, slope, valley height."""

    def __init__(self, L, hf, placed, district):
        self.poly = L['districts'][district]['polygon']
        xs, ys = [p[0] for p in self.poly], [p[1] for p in self.poly]
        self.x0, self.y0 = min(xs), min(ys)
        self.nx, self.ny = int((max(xs) - self.x0) / CELL) + 1, int((max(ys) - self.y0) / CELL) + 1
        self.hf = hf
        n = self.nx * self.ny
        self.inside = bytearray(n)
        self.clear = [99.0] * n  # metres to the nearest road edge, water edge or building footprint
        self.road = [99.0] * n   # metres to the nearest road edge (paths), for roles that hug or avoid paths
        self.slope, self.valley = [0.0] * n, [0.0] * n
        lines = [(pts, L['roads'][f'{tier}_width'] / 2) for tier in ('primary', 'secondary', 'trail') for pts in L['roads'][tier].values()]
        n_roads = len(lines)
        lines += [(w['points'], w['width'] / 2) for w in waters(L)]
        lines += [(b['polygon'] + b['polygon'][:1], 0.0) for b in L.get('basins', [])]
        close = lambda pts: _dist(((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2), pts) < max(max(xs) - min(xs), max(ys) - min(ys))
        near_roads = [(pts, hw) for pts, hw in lines[:n_roads] if close(pts)]
        near_other = [(pts, hw) for pts, hw in lines[n_roads:] if close(pts)]
        plots = L['construction_plots']
        pads = list(placed) + [(x, y, plots['size'] * 0.71) for x, y in plots['centres']]  # plots stay clear even before they are built
        floor = self._floor(hf, xs, ys)
        for j in range(self.ny):
            for i in range(self.nx):
                k = j * self.nx + i
                x, y = self.x0 + i * CELL, self.y0 + j * CELL
                if not in_polygon((x, y), self.poly):
                    continue
                self.inside[k] = 1
                self.road[k] = min([_dist((x, y), pts) - hw for pts, hw in near_roads] + [99.0])
                c = min([self.road[k], *[_dist((x, y), pts) - hw for pts, hw in near_other]] + [99.0])
                c = min([c] + [math.hypot(x - px, y - py) - r for px, py, r in pads if abs(x - px) < 40 and abs(y - py) < 40])
                if any(in_polygon((x, y), b['polygon']) for b in L.get('basins', [])):
                    c = -1.0
                self.clear[k] = c
                gx = (hf.at(x + 2, y) - hf.at(x - 2, y)) / 4; gy = (hf.at(x, y + 2) - hf.at(x, y - 2)) / 4
                self.slope[k] = math.degrees(math.atan(math.hypot(gx, gy)))
                self.valley[k] = hf.at(x, y) - floor(x, y)

    @staticmethod
    def _floor(hf, xs, ys, cs=20.0, r=120.0):
        """Lowest ground within r, on a cs grid, read back by nearest cell."""
        x0, y0 = min(xs) - r, min(ys) - r
        nx, ny = int((max(xs) + r - x0) / cs) + 1, int((max(ys) + r - y0) / cs) + 1
        h = [[hf.at(x0 + i * cs, y0 + j * cs) for i in range(nx)] for j in range(ny)]
        k = int(r / cs)
        lo = [[min(h[jj][ii] for jj in range(max(0, j - k), min(ny, j + k + 1)) for ii in range(max(0, i - k), min(nx, i + k + 1)))
               for i in range(nx)] for j in range(ny)]
        return lambda x, y: lo[min(ny - 1, max(0, round((y - y0) / cs)))][min(nx - 1, max(0, round((x - x0) / cs)))]

    def cell(self, x, y):
        i, j = round((x - self.x0) / CELL), round((y - self.y0) / CELL)
        return j * self.nx + i if 0 <= i < self.nx and 0 <= j < self.ny else None


def dress(L, P, hf, placed, district, seed):
    """Returns {version, roles: {role: [dict(mesh, x, y, z, yaw, scale)]}, gaps, area_ha}."""
    site = Site(L, hf, placed, district)
    roles = {l['role']: l for l in P['layers']}
    out, gaps, canopy = {}, {}, []
    area_ha = sum(site.inside) * CELL * CELL / 10000
    bucket = {}  # spatial hash of canopy trunks for the shade test
    solids = {}  # spatial hash of every tree and rock, so roles don't overlap each other

    def shaded(x, y):
        bx, by = int(x // SHADE), int(y // SHADE)
        return any(math.hypot(x - tx, y - ty) <= SHADE for i in (-1, 0, 1) for j in (-1, 0, 1) for tx, ty in bucket.get((bx + i, by + j), ()))

    for layer in ORDER:
        for role, spec in roles.items():
            if spec['layer'] != layer or 'place' not in spec:
                continue
            rule = spec['place']
            meshes = spec['have'] or roles.get(rule.get('fallback'), {}).get('have', [])
            if not spec['have']:
                gaps[role] = rule.get('fallback') or 'none'
            if not meshes:
                continue
            per_ha = rule['per_ha'] * L['districts'][district].get('dress', {}).get(layer, 1.0)
            if per_ha <= 0:
                continue
            placement_seed = SHIPPED.get((seed, district), (f'{seed}:{district}:{RULES_VERSION}',))[0]
            rng = random.Random(f'{placement_seed}:{role}')
            target = round(per_ha * area_ha)
            cell = math.sqrt(10000 / per_ha)
            spacing, placed_here, grid = rule['spacing'], [], {}

            def free(x, y):
                if layer in SOLID_LAYERS:
                    sx_, sy_ = int(x // SOLID), int(y // SOLID)
                    if any(math.hypot(x - qx, y - qy) < SOLID for i in (-1, 0, 1) for j in (-1, 0, 1) for qx, qy in solids.get((sx_ + i, sy_ + j), ())):
                        return False
                if not spacing:
                    return True
                gx, gy = int(x // spacing), int(y // spacing)
                return all(math.hypot(x - qx, y - qy) >= spacing for i in (-1, 0, 1) for j in (-1, 0, 1) for qx, qy in grid.get((gx + i, gy + j), ()))

            sites = [(site.x0 + (i + rng.random()) * cell, site.y0 + (j + rng.random()) * cell)
                     for j in range(int(site.ny * CELL / cell) + 1) for i in range(int(site.nx * CELL / cell) + 1)]
            rng.shuffle(sites)
            for sx, sy in sites:
                if len(placed_here) >= target:
                    break
                for c in range(rule.get('cluster', 1)):
                    x, y = (sx, sy) if c == 0 else (sx + rng.uniform(-3, 3), sy + rng.uniform(-3, 3))
                    k = site.cell(x, y)
                    if k is None or not site.inside[k] or site.clear[k] < rule['clear']:
                        continue
                    if site.road[k] > rule.get('path', 99.0):  # 'path': only within this many metres of a road edge
                        continue
                    if not (rule['slope'][0] <= site.slope[k] <= rule['slope'][1] and rule['valley'][0] <= site.valley[k] <= rule['valley'][1]):
                        continue
                    if rule['light'] != 'any' and shaded(x, y) != (rule['light'] == 'shade'):
                        continue
                    if not free(x, y):
                        continue
                    scale = rng.uniform(*rule['scale'])
                    placed_here.append(dict(mesh=meshes[rng.randrange(len(meshes))], x=round(x, 2), y=round(y, 2),
                                            z=round(hf.at(x, y), 2), yaw=round(rng.uniform(0, 360), 1), scale=round(scale, 3),
                                            sink=rule.get('sink', 0.0)))
                    if spacing:
                        grid.setdefault((int(x // spacing), int(y // spacing)), []).append((x, y))
                    if layer in SOLID_LAYERS:
                        solids.setdefault((int(x // SOLID), int(y // SOLID)), []).append((x, y))
            if layer == 'canopy':
                for t in placed_here:
                    bucket.setdefault((int(t['x'] // SHADE), int(t['y'] // SHADE)), []).append((t['x'], t['y']))
            out[role] = placed_here
    cull = {r: min(max(roles[r]['place']['cull_m'], CULL_BOUNDS[0]), CULL_BOUNDS[1]) for r in out if 'cull_m' in roles[r]['place']}
    return dict(version=content_version(seed, district, out), district=district, cull=cull,
                area_ha=round(area_ha, 2), roles=out, gaps=gaps)


def candidates(result, P):
    """Gameplay candidates (harvestable trees and stone) with stable IDs from the content version and position."""
    roles = {l['role']: l for l in P['layers']}
    out = []
    for role, items in result['roles'].items():
        kind = roles[role]['place'].get('harvest')
        if kind:
            for t in items:
                key = f"{result['version']}:{role}:{t['x']:.1f}:{t['y']:.1f}"
                out.append(dict(id=hashlib.sha1(key.encode()).hexdigest()[:16], role=role, resource=kind, at=[t['x'], t['y'], t['z']]))
    return out


def check(L, result, cands, placed, min_spacing=2.0):
    """Validator from the pipeline: no duplicate IDs, no candidates too close, none on a road or in a build plot."""
    problems = []
    ids = [c['id'] for c in cands]
    if len(ids) != len(set(ids)):
        problems.append('duplicate candidate IDs')
    grid = {}
    for c in cands:
        x, y = c['at'][0], c['at'][1]
        g = (int(x // min_spacing), int(y // min_spacing))
        if any(math.hypot(x - qx, y - qy) < min_spacing for i in (-1, 0, 1) for j in (-1, 0, 1) for qx, qy in grid.get((g[0] + i, g[1] + j), ())):
            problems.append(f'candidates closer than {min_spacing} m at {x:.0f},{y:.0f}')
            break
        grid.setdefault(g, []).append((x, y))
        if any(_dist((x, y), pts) < L['roads'][f'{tier}_width'] / 2 for tier in ('primary', 'secondary', 'trail') for pts in L['roads'][tier].values()):
            problems.append(f'candidate on a road at {x:.0f},{y:.0f}')
            break
        half = L['construction_plots']['size'] / 2
        if any(abs(x - px) < half and abs(y - py) < half for px, py in L['construction_plots']['centres']):
            problems.append(f'candidate inside a build plot at {x:.0f},{y:.0f}')
            break
    return problems


def transforms_hash(result):
    return hashlib.sha1(json.dumps(result['roles'], sort_keys=True).encode()).hexdigest()[:12]


if __name__ == '__main__':
    from heightfield import Heightfield, zone_region
    district = sys.argv[1] if len(sys.argv) > 1 else 'Eastwood'
    L = json.loads((ROOT / 'silverbrook-town-layout.json').read_text(encoding='utf-8'))
    P = json.loads((ROOT / 'greenlands-look-profile.json').read_text(encoding='utf-8'))
    region, _, _ = zone_region('silverbrook-town-layout.json')
    hf = Heightfield(L, region=region)  # no building pads here; the builder passes its own
    a = dress(L, P, hf, [], district, L['seed'])
    b = dress(L, P, hf, [], district, L['seed'])
    assert transforms_hash(a) == transforms_hash(b), 'same content version must give identical output'
    unrelated = dict(L, arrival=dict(L['arrival'], at=[0, 0]))
    c = dress(unrelated, P, hf, [], district, L['seed'])
    assert c['version'] == a['version'] and c['roles'] == a['roles'], 'arrival edits must not reseed dressing'
    other = dress(L, P, hf, [], district, L['seed'] + 1)
    assert transforms_hash(other) != transforms_hash(a), 'a new seed must change the output'
    cands = candidates(a, P)
    problems = check(L, a, cands, [])
    bad = check(L, a, cands + cands[:1], [])
    assert bad, 'self-test: a duplicated candidate must fail'
    for p in problems:
        print('FAIL', p)
    if problems:
        sys.exit(1)
    counts = {r: len(v) for r, v in a['roles'].items()}
    print(f"DRESS_OK {district} {a['version']}: {a['area_ha']} ha, {sum(counts.values())} instances {counts}, "
          f"{len(cands)} candidates, gaps {a['gaps']}, hash {transforms_hash(a)}")
