"""Validate a region layout (step 0 of docs/settlement-world-pipeline.md: region first).

Standard library only. Usage:
    python validate_region.py [region.json]     # default: greenlands-region-layout.json beside this file
Prints each problem and exits 1 if any; running it also self-tests that broken copies fail.
Frame: metres, north is -Y, east is +X; heights in metres above sea level.
"""
import copy, json, math, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from validate_layout import DIRECTIONS, line_dist, self_intersects  # noqa: E402

ROOT = Path(__file__).resolve().parent


def in_polygon(p, poly):
    hit = False
    for (ax, ay), (bx, by) in zip(poly, poly[1:] + poly[:1]):
        if (ay > p[1]) != (by > p[1]) and p[0] < ax + (p[1] - ay) * (bx - ax) / (by - ay):
            hit = not hit
    return hit


def level_near(river, p):
    """Water level of `river` at the point of its course nearest p, and that distance."""
    best = (1e18, None)
    pts, lv = river['points'], river['levels']
    for i, (a, b) in enumerate(zip(pts, pts[1:])):
        dx, dy = b[0] - a[0], b[1] - a[1]
        t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / ((dx * dx + dy * dy) or 1)))
        d = math.dist(p, (a[0] + t * dx, a[1] + t * dy))
        if d < best[0]:
            best = (d, lv[i] + (lv[i + 1] - lv[i]) * t)
    return best[1], best[0]


def tile_of(R, p):
    x0, y0 = R['bounds'][:2]
    return int((p[0] - x0) // R['tile_size']), int((p[1] - y0) // R['tile_size'])


def validate(R, zones=None):
    """zones: {settlement: zone layout dict}; defaults to loading each settlement's zone_layout file."""
    errors = []
    x0, y0, x1, y1 = R['bounds']
    ts, coast = R['tile_size'], R['coastline']
    S, rivers = R['settlements'], R['rivers']

    # 1. Bounds are whole tiles; the coast is a simple polygon.
    if (x1 - x0) % ts or (y1 - y0) % ts:
        errors.append('bounds are not whole tiles')
    if self_intersects(coast):
        errors.append('coastline self-intersects')

    # 2. Required settlements exist, sit on land inside the bounds, one per tile.
    tiles = {}
    for name in R['requirements']['settlements']:
        if name not in S:
            errors.append(f'missing settlement {name}')
    for name, s in S.items():
        p = s['at']
        if not (x0 <= p[0] < x1 and y0 <= p[1] < y1):
            errors.append(f'{name} is outside the region bounds')
        if not in_polygon(p, coast):
            errors.append(f'{name} is in the sea')
        t = tile_of(R, p)
        if t in tiles:
            errors.append(f'{name} shares tile {t} with {tiles[t]}')
        tiles[t] = name

    # 3. Rivers run downhill from a source on land (or a parent river) to the sea or another river.
    for name, r in rivers.items():
        pts, lv = r['points'], r['levels']
        if len(lv) != len(pts):
            errors.append(f'{name} levels count differs from its points')
            continue
        falls = {tuple(f['between_points']) for f in r['waterfalls']}
        for i, (h0, h1) in enumerate(zip(lv, lv[1:])):
            if h1 > h0:
                errors.append(f'{name} flows uphill between points {i} and {i + 1}')
            elif h0 - h1 > R['max_drop_without_waterfall'] and (i, i + 1) not in falls:
                errors.append(f'{name}: undeclared waterfall between points {i} and {i + 1}')
        if 'source' in r:
            level, d = level_near(rivers[r['source']], pts[0])
            if d > 100 or lv[0] > level + 0.5:
                errors.append(f'{name} does not leave {r["source"]}')
        elif not in_polygon(pts[0], coast):
            errors.append(f'{name} source is in the sea')
        if r['mouth'] == 'sea':
            if in_polygon(pts[-1], coast) and line_dist(pts[-1], coast + coast[:1]) > 150:
                errors.append(f'{name} does not reach the sea')
            if lv[-1] > R['sea_level'] + 0.5:
                errors.append(f'{name} meets the sea above sea level')
        else:
            level, d = level_near(rivers[r['mouth']], pts[-1])
            if d > 100:
                errors.append(f'{name} does not reach {r["mouth"]}')
            elif lv[-1] < level - 0.5:
                errors.append(f'{name} joins {r["mouth"]} below its level')

    # 4. Roads link the required pairs and stay on land.
    pairs = {frozenset(rd) for rd in R['roads']}
    for a, b in R['roads']:
        if a not in S or b not in S:
            errors.append(f'road {a}-{b} names an unknown settlement')
            continue
        pa, pb = S[a]['at'], S[b]['at']
        if not all(in_polygon((pa[0] + (pb[0] - pa[0]) * k / 20, pa[1] + (pb[1] - pa[1]) * k / 20), coast) for k in range(21)):
            errors.append(f'road {a}-{b} crosses the sea')
    for name in S:
        if not any(name in pr for pr in pairs):
            errors.append(f'{name} has no road')

    # 5. Zones built so far agree with the region.
    if zones is None:
        zones = {n: json.loads((ROOT / s['zone_layout']).read_text(encoding='utf-8')) for n, s in S.items() if 'zone_layout' in s}
    for name, Z in zones.items():
        s = S[name]
        cx = x0 + (tile_of(R, s['at'])[0] + 0.5) * ts
        cy = y0 + (tile_of(R, s['at'])[1] + 0.5) * ts
        if 2 * Z['zone_half_extent'] != ts or [cx, cy] != list(s['at']):
            errors.append(f'{name} zone is not its tile (zone frame origin must be the tile centre)')
        for road, direction in Z['requirements']['roads_out'].items():
            to = road.replace(' Road', '')
            if to not in S or frozenset((name, to)) not in pairs:
                errors.append(f'{name} zone road {road} has no region road to {to}')
                continue
            p = S[to]['at']
            heading = math.degrees(math.atan2(p[1] - s['at'][1], p[0] - s['at'][0]))
            if abs((heading - DIRECTIONS[direction] + 180) % 360 - 180) > 45:
                errors.append(f'{name} zone sends {road} {direction}, but {to} lies at {heading:.0f} deg')
        zr = Z['river']['points']
        rr = rivers.get(Z['river']['name'])
        if not rr or [list(p) for p in rr['points'][:len(zr)]] != [[s['at'][0] + p[0], s['at'][1] + p[1]] for p in zr] \
                or rr['levels'][:len(zr)] != [round(s['datum'] + h, 1) for h in Z['elevation']['river_levels']]:
            errors.append(f'{name} zone river differs from region river {Z["river"]["name"]}')
    return errors


def _self_test(R):
    def broken(change, expect):
        bad = copy.deepcopy(R); change(bad)
        found = validate(bad)
        assert any(expect in e for e in found), f'expected "{expect}", got {found}'

    broken(lambda b: b['settlements'].pop('Everwood'), 'missing settlement Everwood')
    broken(lambda b: b['settlements']['Meadowgate'].update(at=[-6000, 3000]), 'Meadowgate is')
    broken(lambda b: b['settlements']['Meadowgate'].update(at=list(b['settlements']['Everwood']['at'])), 'shares tile')
    broken(lambda b: b['rivers']['Greenlands River']['levels'].__setitem__(7, 90), 'Greenlands River flows uphill')
    broken(lambda b: [b['rivers']['Eldenfall Branch'][k].__delitem__(slice(-3, None)) for k in ('points', 'levels')], 'Eldenfall Branch does not reach the sea')
    broken(lambda b: b['rivers']['Silverbrook River']['levels'].__setitem__(-1, 20), 'joins Greenlands River below its level')
    broken(lambda b: b['roads'].remove(['Rivermoor', 'Stonehollow']) or b['roads'].remove(['Meadowgate', 'Stonehollow']), 'Stonehollow has no road')
    broken(lambda b: b['settlements']['Silverbrook'].update(datum=90), 'Silverbrook zone river differs')
    broken(lambda b: b.update(bounds=[-3750, -3750, 6000, 8750]), 'whole tiles')


if __name__ == '__main__':
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'greenlands-region-layout.json'
    region = json.loads(path.read_text(encoding='utf-8'))
    problems = validate(region)
    for p in problems:
        print('FAIL', p)
    if problems:
        sys.exit(1)
    _self_test(region)
    cols = (region['bounds'][2] - region['bounds'][0]) // region['tile_size']
    rows = (region['bounds'][3] - region['bounds'][1]) // region['tile_size']
    print(f'REGION_OK {path.name}: {cols}x{rows} tiles of {region["tile_size"]} m, {len(region["settlements"])} settlements, '
          f'{len(region["rivers"])} rivers, {len(region["roads"])} roads; self-test passed')
