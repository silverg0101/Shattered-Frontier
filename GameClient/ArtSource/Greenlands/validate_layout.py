"""Validate a settlement layout file (stage 1 of docs/settlement-world-pipeline.md).

Standard library only. Usage:
    python validate_layout.py [layout.json]      # default: silverbrook-town-layout.json beside this file
Prints each problem and exits 1 if any; running it also self-tests that broken copies fail.
Frame: metres, north is -Y, east is +X, origin at the town centre.
"""
import copy, json, math, sys
from pathlib import Path

DIRECTIONS = {'N': -90, 'NE': -45, 'E': 0, 'SE': 45, 'S': 90, 'SW': 135, 'W': 180, 'NW': -135}  # degrees, atan2(y, x) with -Y north


def _cross(o, a, b):
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])


def segments_cross(a, b, c, d):
    """True if segments ab and cd properly intersect (shared endpoints do not count)."""
    d1, d2, d3, d4 = _cross(c, d, a), _cross(c, d, b), _cross(a, b, c), _cross(a, b, d)
    return (d1 * d2 < 0) and (d3 * d4 < 0)


def intersection(a, b, c, d):
    r = (b[0] - a[0], b[1] - a[1]); s = (d[0] - c[0], d[1] - c[1])
    den = r[0] * s[1] - r[1] * s[0]
    if abs(den) < 1e-9:
        return None
    t = ((c[0] - a[0]) * s[1] - (c[1] - a[1]) * s[0]) / den
    u = ((c[0] - a[0]) * r[1] - (c[1] - a[1]) * r[0]) / den
    return (a[0] + t * r[0], a[1] + t * r[1]) if 0 <= t <= 1 and 0 <= u <= 1 else None


def seg_dist(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / ((dx * dx + dy * dy) or 1)))
    return math.dist(p, (a[0] + t * dx, a[1] + t * dy))


def line_dist(p, pts):
    return min(seg_dist(p, a, b) for a, b in zip(pts, pts[1:]))


def self_intersects(poly):
    edges = list(zip(poly, poly[1:] + poly[:1]))
    for i in range(len(edges)):
        for j in range(i + 2, len(edges)):
            if i == 0 and j == len(edges) - 1:
                continue  # first and last edge share a vertex
            if segments_cross(*edges[i], *edges[j]):
                return True
    return False


def wall_distance(p, wall):
    a, b = wall['semi_axis_x'], wall['semi_axis_y']
    cx, cy = wall['centre']
    return min(math.dist(p, (cx + a * math.cos(t), cy + b * math.sin(t)))
               for t in (2 * math.pi * k / 720 for k in range(720)))


def validate(L):
    errors = []
    req = L['requirements']
    x0, y0, x1, y1 = L['zone_bounds']
    roads = {name: pts for tier in ('primary', 'secondary', 'trail') for name, pts in L['roads'][tier].items()}
    primary = L['roads']['primary']

    # 1. Required places and districts.
    places = {p['key']: p for p in L['places']}
    for key in req['place_keys']:
        if key not in places:
            errors.append(f'missing place {key}')
        elif places[key]['type'] not in ('gate', 'plaza', 'fields', 'yard', 'ruins', 'marker', 'bridge') \
                and places[key]['type'] not in L['building_types']:
            errors.append(f'place {key} has unknown building type {places[key]["type"]}')
    for name in req['districts']:
        d = L['districts'].get(name)
        if not d or len(d.get('polygon', [])) < 3:
            errors.append(f'missing district polygon {name}')

    # 2. Walled-town radius and zone bounds.
    wall = L['wall']
    if max(wall['semi_axis_x'], wall['semi_axis_y']) > req['walled_town_max_radius']:
        errors.append(f'wall exceeds walled_town_max_radius {req["walled_town_max_radius"]} m')
    points = [p['at'] for p in L['places'] if 'at' in p] + list(L['gates'].values()) + L['river']['points']
    points += [q for pts in roads.values() for q in pts]
    points += [q for d in L['districts'].values() for q in d.get('polygon', [])]
    for q in points:
        if not (x0 <= q[0] <= x1 and y0 <= q[1] <= y1):
            errors.append(f'point {q} outside zone bounds')
            break

    # 3. Gates sit on the wall and on a primary road.
    tol = req['gate_tolerance']
    for name, g in L['gates'].items():
        if wall_distance(g, wall) > tol:
            errors.append(f'{name} is not on the wall')
        if min(line_dist(g, pts) for pts in primary.values()) > L['roads']['primary_width'] / 2 + tol:
            errors.append(f'{name} is not on a primary road')

    # 4. Roads out reach the zone edge in the stated direction.
    for name, direction in req['roads_out'].items():
        pts = roads.get(name)
        if not pts:
            errors.append(f'missing road out {name}')
            continue
        end = pts[-1]
        if not (abs(end[0] - x0) < 1 or abs(end[0] - x1) < 1 or abs(end[1] - y0) < 1 or abs(end[1] - y1) < 1):
            errors.append(f'{name} does not reach the zone edge')
        heading = math.degrees(math.atan2(end[1] - pts[0][1], end[0] - pts[0][0]))
        if abs((heading - DIRECTIONS[direction] + 180) % 360 - 180) > 45:
            errors.append(f'{name} heads {heading:.0f} deg, not {direction}')

    # 5. The main river passes under the main bridge.
    bridge = places.get(req['main_bridge_key'], {}).get('at')
    river = L['river']['points']
    crossings = [hit for pts in list(L['roads']['primary'].values()) + list(L['roads']['secondary'].values())
                 for a, b in zip(pts, pts[1:]) for c, d in zip(river, river[1:]) if (hit := intersection(a, b, c, d))]
    if not bridge or not any(math.dist(hit, bridge) <= L['river']['bridge_length'] for hit in crossings):
        errors.append('river does not pass under the main bridge')

    # 6. District polygons do not self-intersect.
    for name, d in L['districts'].items():
        if len(d.get('polygon', [])) >= 4 and self_intersects(d['polygon']):
            errors.append(f'district {name} polygon self-intersects')

    # Elevation sanity: water flows downhill; big drops are declared waterfalls.
    elev = L.get('elevation')
    if elev:
        levels = elev['river_levels']
        if len(levels) != len(river):
            errors.append('river_levels count differs from river points')
        falls = {tuple(w['between_points']) for w in elev['waterfalls']}
        for i, (h0, h1) in enumerate(zip(levels, levels[1:])):
            if h1 > h0:
                errors.append(f'river flows uphill between points {i} and {i + 1}')
            elif h0 - h1 > elev['max_drop_without_waterfall'] and (i, i + 1) not in falls:
                errors.append(f'undeclared waterfall between points {i} and {i + 1}')
    return errors


def _self_test(L):
    """Each deliberately broken copy must fail with the expected message."""
    def broken(change, expect):
        bad = copy.deepcopy(L); change(bad)
        found = validate(bad)
        assert any(expect in e for e in found), f'expected "{expect}", got {found}'

    broken(lambda b: b['places'].pop(0), 'missing place 1')
    broken(lambda b: b['districts'].pop('Eastwood'), 'missing district polygon Eastwood')
    broken(lambda b: b['wall'].update(semi_axis_y=900), 'walled_town_max_radius')
    broken(lambda b: b['places'][4].update(at=[5000, 0]), 'outside zone bounds')
    broken(lambda b: b['gates'].update({'North Gate': [0, 0]}), 'North Gate is not on the wall')
    broken(lambda b: b['roads']['primary']['Silvermere Road'].pop(), 'Silvermere Road does not reach the zone edge')
    broken(lambda b: b['requirements']['roads_out'].update({'Valemere Road': 'N'}), 'not N')
    broken(lambda b: b['places'][18].update(at=[600, -600]), 'main bridge')
    broken(lambda b: b['districts']['Westfields'].update(polygon=[[-700, -100], [-400, 300], [-700, 300], [-400, -100]]), 'self-intersects')
    broken(lambda b: b['elevation']['river_levels'].__setitem__(5, 20), 'uphill')


if __name__ == '__main__':
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent / 'silverbrook-town-layout.json'
    layout = json.loads(path.read_text(encoding='utf-8'))
    problems = validate(layout)
    for p in problems:
        print('FAIL', p)
    if problems:
        sys.exit(1)
    _self_test(layout)
    print(f'LAYOUT_OK {path.name}: {len(layout["places"])} places, {len(layout["districts"])} districts, '
          f'{sum(len(v) for v in layout["roads"].values() if isinstance(v, dict))} roads; self-test passed')
