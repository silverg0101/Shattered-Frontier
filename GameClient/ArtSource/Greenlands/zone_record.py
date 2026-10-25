"""Write the Silverbrook zone record (content/world/zones/greenlands_silverbrook.json) from the layout and the
dressing candidates. Standard library only:  python zone_record.py   prints ZONE_RECORD_OK.

Covers Zone Definition Standard rows Z1, Z2 (default spawn and respawn points), Z4 (level areas), Z5 (the walled
town is protected and no-edit; water volumes), Z8 (gathering spawn groups from silverbrook-*-candidates.json) and
Z9 (boar, wolf and bandit groups placed per Settlement Silverbrook section 11). Layout iron_veins designate
outdoor rock candidates as iron ore while keeping their dressing point IDs. Field names follow the World
Simulation zone record and its spawn group table. Positions are zone-local metres in Unreal axes (north is -Y);
NPC points carry x, y only and are dropped onto the ground when they spawn. Re-run after a rebuild, since the
candidates change with it.
"""
import json, math, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OUT = ROOT.parents[2] / 'content' / 'world' / 'zones' / 'greenlands_silverbrook.json'
sys.path.insert(0, str(ROOT))
from validate_layout import SILVERBROOK_LAYOUT, waters  # noqa: E402
from heightfield import in_polygon  # noqa: E402

NODE = {'tree_pine_tall': 'gathering_node/pine_tree', 'tree_oak': 'gathering_node/oak_tree',
        'tree_broadleaf_young': 'gathering_node/young_broadleaf', 'rock_medium': 'gathering_node/stone_outcrop',
        'boulder_large_moss': 'gathering_node/mossy_boulder', 'iron_vein': 'gathering_node/iron_vein'}
SLOTS_PER_KM2 = 40  # common tier, World Simulation "Balancing targets"
PLAYABLE_RADIUS = 750  # Settlement Silverbrook, rings
HOSTILE_CLEARANCE = 80  # m from the wall, build plots, the arrival point and respawn points
# Hostile groups: (group, definitions, centre, radius, points, active, content level). Section 11: boar at forest and
# farm edges (starter combat); wolves and bandits beyond the playable radius, wolves away from arrival, bandits on a trail.
HOSTILE = [
    ('boar_farm_edge', [('npc/boar', 1)], (-660, 300), 70, 12, 5, 2),
    ('boar_forest_edge', [('npc/boar', 1)], (690, -20), 60, 12, 5, 2),
    ('wolf_northwest_den', [('npc/wolf', 1)], (-760, -760), 80, 8, 4, 6),
    ('bandit_forest_trail_camp', [('npc/bandit_thug', 3), ('npc/bandit_archer', 1)], (880, -640), 60, 8, 5, 5),
]


def cluster_boar_points(points):
    """Keep each boar group in three clearings where common pack assist can fire."""
    centres = [points[j]['position'] for j in (0, 4, 8)]
    offsets = [(-2.5, -2.5), (2.5, -2.5), (-2.5, 2.5), (2.5, 2.5)]
    return [dict(point_id=points[j * 4 + i]['point_id'],
                 position=[round(x + dx, 1), round(y + dy, 1)],
                 facing=round(math.degrees(math.atan2(-dy, -dx)) % 360, 1))
            for j, (x, y) in enumerate(centres) for i, (dx, dy) in enumerate(offsets)]


def ellipse(L, n=48):
    W = L['wall']; cx, cy = W['centre']
    return [[round(cx + W['semi_axis_x'] * math.cos(2 * math.pi * k / n), 1),
             round(cy + W['semi_axis_y'] * math.sin(2 * math.pi * k / n), 1)] for k in range(n)]


def in_wall(L, p, margin=0.0):
    W = L['wall']
    return ((p[0] - W['centre'][0]) / (W['semi_axis_x'] + margin)) ** 2 + ((p[1] - W['centre'][1]) / (W['semi_axis_y'] + margin)) ** 2 <= 1


def seg_dist(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / ((dx * dx + dy * dy) or 1)))
    return math.dist(p, (a[0] + dx * t, a[1] + dy * t))


def wet(L, p, margin=2.0):
    if any(seg_dist(p, a, b) < w['width'] / 2 + margin for w in waters(L) for a, b in zip(w['points'], w['points'][1:])):
        return True
    return any(in_polygon(p, b['polygon']) for b in L.get('basins', []))


def circle(c, r, n=16):
    return [[round(c[0] + r * math.cos(2 * math.pi * k / n), 1), round(c[1] + r * math.sin(2 * math.pi * k / n), 1)] for k in range(n)]


def area_km2(poly):
    return abs(sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(poly, poly[1:] + poly[:1]))) / 2 / 1e6


def build(L, candidates):
    ax, ay = L['arrival']['at']; fx, fy = L['arrival']['face']
    plots = [[[x - s / 2, y - s / 2], [x + s / 2, y - s / 2], [x + s / 2, y + s / 2], [x - s / 2, y + s / 2]]
             for (x, y), s in ((c, L['construction_plots']['size']) for c in L['construction_plots']['centres'])]
    respawn = [dict(point_id='arrival', position=[ax, ay], facing=round(math.degrees(math.atan2(fy - ay, fx - ax)), 1))]
    for name, (gx, gy) in L['gates'].items():  # just inside each gate, facing the town centre
        d = math.hypot(gx, gy); px, py = gx - gx / d * 25, gy - gy / d * 25
        respawn.append(dict(point_id=name.lower().replace(' ', '_'), position=[round(px, 1), round(py, 1)],
                            facing=round(math.degrees(math.atan2(-py, -px)), 1)))
    plaza = next(p for p in L['places'] if p['type'] == 'plaza')
    respawn.append(dict(point_id='market_square', position=plaza['at'], facing=0.0))

    town = ellipse(L)
    level_areas = [dict(area_id='silverbrook_countryside', polygon=[[-1250, -1250], [1250, -1250], [1250, 1250], [-1250, 1250]],
                        content_level=3),
                   dict(area_id='silverbrook_town', polygon=town, content_level=1)]
    areas = [dict(area_id='silverbrook_town', purpose='settlement', safety='protected', volumes=['protected', 'no_edit'],
                  polygon=town, note='Inside the wall: no hostile spawns, no PvP, no terrain edits. Enforced on the server.'),
             dict(area_id='silverbrook_build_plots', purpose='claims', safety='open', volumes=[], polygons=plots)]
    volumes = [dict(kind='water', area_id=w['name'].lower().replace(' ', '_'), polyline=w['points'], width=w['width'])
               for w in waters(L)]
    volumes += [dict(kind='water', area_id=b['name'].lower().replace(' ', '_'), polygon=b['polygon'], level=b['level'])
                for b in L.get('basins', [])]

    groups = []
    for district, cands in candidates.items():
        poly = L['districts'][district]['polygon']
        iron_ids = L.get('iron_veins', {}).get(district, [])
        if len(iron_ids) != len(set(iron_ids)):
            raise ValueError(f'{district}: duplicate iron vein candidate IDs')
        by_id = {c['id']: c for c in cands}
        missing = set(iron_ids) - by_id.keys()
        if missing:
            raise ValueError(f'{district}: missing iron vein candidates {sorted(missing)}')
        if any(by_id[i]['role'] != 'rock_medium' or in_wall(L, by_id[i]['at']) for i in iron_ids):
            raise ValueError(f'{district}: iron veins must be outdoor rock candidates')
        outside = [dict(c, role='iron_vein', resource='iron_ore') if c['id'] in iron_ids else c
                   for c in cands if not in_wall(L, c['at']) and NODE.get(c['role'])]
        for resource in sorted({c['resource'] for c in outside}):
            pts = [c for c in outside if c['resource'] == resource]
            weights = {}
            for c in pts:
                weights[NODE[c['role']]] = weights.get(NODE[c['role']], 0) + 1
            k = min(len(pts) // 2, max(2, round(SLOTS_PER_KM2 * area_km2(poly) * len(pts) / len(outside))))
            slug = district.lower().replace(' ', '_')
            groups.append(dict(group_id=f'{slug}_{resource}', spawn_kind='gathering_node',
                               definitions=[dict(definition_id=d, weight=w) for d, w in sorted(weights.items())],
                               points=[dict(point_id=c['id'], position=c['at'], facing=0.0) for c in pts],
                               active_count=k, area=poly, phase_filter='any', layer_policy='all_layers'))
    for gid, defs, c, r, n, k, level in HOSTILE:
        pts, i = [], 0
        while len(pts) < n and i < 400:  # a golden-angle spiral, skipping water
            i += 1; rr = 0.9 * r * math.sqrt(min(1.0, i / (1.5 * n))); a = i * 2.39996
            p = [round(c[0] + rr * math.cos(a), 1), round(c[1] + rr * math.sin(a), 1)]
            if not wet(L, p) and all(math.dist(p, q['position']) > 6 for q in pts):
                pts.append(dict(point_id=f'{gid}_{len(pts):02d}', position=p, facing=round(math.degrees(a) % 360, 1)))
        if gid.startswith('boar_'):
            # Three authored clearings of four points. Any five active slots include
            # a pair within the common 8 m assist radius, even after point rotation.
            pts = cluster_boar_points(pts)
        ring = circle(c, r)
        groups.append(dict(group_id=gid, spawn_kind='npc', definitions=[dict(definition_id=d, weight=w) for d, w in defs],
                           points=pts, active_count=k, area=ring, phase_filter='any', layer_policy='all_layers'))
        level_areas.append(dict(area_id=gid, polygon=ring, content_level=level))

    return dict(
        _about='Generated by GameClient/ArtSource/Greenlands/zone_record.py from silverbrook-town-layout.json and the dressing '
               'candidates; do not hand-edit. Zone Definition Standard rows Z1, Z2, Z4, Z5, Z8 and Z9.',
        id='zone/greenlands_silverbrook', display_name='Silverbrook', world_region_id='world_region/greenlands',
        archetype='starter', zone_index=1, map_id='/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook',
        maturity='graybox', origin=[0, 0], origin_note='Region-local (Silverbrook town centre); the continent frame is not fixed yet.',
        extent=[[-1250, -1250], [1250, 1250]], level_range=[1, 8], layerable=False,
        default_spawn=respawn[0], respawn_points=respawn, level_areas=level_areas, areas=areas, volumes=volumes,
        spawn_groups=groups)


def check(L, Z):
    """Problems with a zone record: ids, counts, nesting, and hostile spawns kept clear of safe places."""
    out = []
    ids = [g['group_id'] for g in Z['spawn_groups']]
    if len(ids) != len(set(ids)):
        out.append('duplicate group ids')
    lo, hi = Z['level_range']
    for a in Z['level_areas']:
        if not lo <= a['content_level'] <= hi:
            out.append(f"{a['area_id']}: content level outside {Z['level_range']}")
    for a in Z['level_areas']:
        for b in Z['level_areas']:
            if a is not b:
                inside = [in_polygon(p, b['polygon']) for p in a['polygon']]
                if any(inside) and not all(inside) and not all(in_polygon(p, a['polygon']) for p in b['polygon']):
                    out.append(f"level areas {a['area_id']} and {b['area_id']} partially overlap")
    safe = [p['position'] for p in Z['respawn_points']] + [c for c in L['construction_plots']['centres']]
    for p in Z['respawn_points']:
        if wet(L, p['position']):
            out.append(f"respawn point {p['point_id']} is in water")
    for g in Z['spawn_groups']:
        n, k = len(g['points']), g['active_count']
        if not g['points'] or k < 1 or any(d['weight'] <= 0 for d in g['definitions']):
            out.append(f"{g['group_id']}: empty points, no active slots or a non-positive weight")
        if g['spawn_kind'] == 'gathering_node' and n < 2 * k:
            out.append(f"{g['group_id']}: {n} points for {k} active slots; gathering groups need N >= 2K")
        pids = [p['point_id'] for p in g['points']]
        if len(pids) != len(set(pids)):
            out.append(f"{g['group_id']}: duplicate point ids")
        for p in g['points']:
            if in_wall(L, p['position']):
                out.append(f"{g['group_id']}: point {p['point_id']} is inside the protected town")
                break
        if g['spawn_kind'] == 'npc':
            for p in g['points']:
                pos = p['position']
                if in_wall(L, pos, HOSTILE_CLEARANCE) or any(math.dist(pos, s) < HOSTILE_CLEARANCE for s in safe) or wet(L, pos):
                    out.append(f"{g['group_id']}: point {p['point_id']} is in water or within {HOSTILE_CLEARANCE} m of the wall, a plot or a respawn point")
                    break
            if g['group_id'].startswith(('wolf', 'bandit')) and min(math.hypot(*p['position']) for p in g['points']) < PLAYABLE_RADIUS:
                out.append(f"{g['group_id']}: wolves and bandits belong beyond the {PLAYABLE_RADIUS} m playable radius")
    return out


def main():
    L = json.loads(SILVERBROOK_LAYOUT.read_text(encoding='utf-8'))
    candidates = {}
    for d in L['districts']:
        f = ROOT / f"silverbrook-{d.lower().replace(' ', '-')}-candidates.json"
        candidates[d] = json.loads(f.read_text(encoding='utf-8'))['candidates'] if f.exists() else []
    Z = build(L, candidates)
    problems = check(L, Z)
    bad = json.loads(json.dumps(Z))
    bad['spawn_groups'][-1]['points'][0]['position'] = [0, 0]  # bandits in the market square
    bad['spawn_groups'][0]['active_count'] = len(bad['spawn_groups'][0]['points'])
    found = check(L, bad)
    assert any('protected town' in p for p in found) and any('N >= 2K' in p for p in found), f'self-test: {found}'
    for p in problems:
        print('FAIL', p)
    if problems:
        sys.exit(1)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(Z, indent=1) + '\n', encoding='utf-8', newline='\n')
    g = Z['spawn_groups']
    print(f"ZONE_RECORD_OK {OUT.name}: {len(Z['respawn_points'])} respawn points, {len(Z['level_areas'])} level areas, "
          f"{sum(1 for x in g if x['spawn_kind'] == 'gathering_node')} gathering groups "
          f"({sum(len(x['points']) for x in g if x['spawn_kind'] == 'gathering_node')} points, "
          f"{sum(x['active_count'] for x in g if x['spawn_kind'] == 'gathering_node')} active), "
          f"{sum(1 for x in g if x['spawn_kind'] == 'npc')} NPC groups")


if __name__ == '__main__':
    main()
