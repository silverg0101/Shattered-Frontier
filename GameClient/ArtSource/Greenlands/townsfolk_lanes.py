"""Footway lanes for the townsfolk prototype (AI Townsfolk doc, section 9 step 3): ZoneGraph shapes along every street
inside the town wall that joins the Market Square.

    python townsfolk_lanes.py                          # any Python 3: plans the network, prints LANES_PLAN_OK
    python tools/Run-InEditor.py GameClient/ArtSource/Greenlands/townsfolk_lanes.py   # in the open editor, on the zone map

plan() splits the selected streets wherever they meet, so every junction is a node. In the editor, each street piece
becomes a spline ZoneShape with the 'Footway' lane profile (one lane each way, DefaultEngine.ini), and each junction a
polygon ZoneShape whose arms meet those splines. Then the ZoneGraph is rebuilt and the map saved. The zone builder passes
ground heights (and which points are over water, where heights run straight from bank to bank along the bridge deck);
a standalone run traces them in SFTownsfolkLaneLibrary. Reruns replace the earlier lanes (folder Townsfolk/Lanes).
"""
import json, math, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from heightfield import in_polygon  # noqa: E402
JUNCTION_R = 4.0   # m from a junction centre to its polygon arms
SNAP = 12.0        # m: a street end this close to another street joins it
SPACING = 5.0      # m between shape points, the heightfield's grid step


def _cross(a, b, c, d):
    """Intersection point of segments ab and cd, or None (endpoints touching counts)."""
    r = (b[0] - a[0], b[1] - a[1]); s = (d[0] - c[0], d[1] - c[1])
    den = r[0] * s[1] - r[1] * s[0]
    if abs(den) < 1e-9:
        return None
    t = ((c[0] - a[0]) * s[1] - (c[1] - a[1]) * s[0]) / den
    u = ((c[0] - a[0]) * r[1] - (c[1] - a[1]) * r[0]) / den
    if -1e-6 <= t <= 1 + 1e-6 and -1e-6 <= u <= 1 + 1e-6:
        return (a[0] + r[0] * t, a[1] + r[1] * t)
    return None


def _nearest(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / ((dx * dx + dy * dy) or 1)))
    return (a[0] + dx * t, a[1] + dy * t)


def plan(L):
    """(edges, junctions): edges are point lists in metres between nodes; junctions are (centre, [arm directions])."""
    W = L['wall']
    sq = next(p['at'] for p in L['places'] if p['type'] == 'plaza')
    keep = lambda m: (((m[0] - W['centre'][0]) / W['semi_axis_x']) ** 2 + ((m[1] - W['centre'][1]) / W['semi_axis_y']) ** 2 < 1
                      or any(in_polygon(m, d['polygon']) for d in L['districts'].values()))  # the farms past the wall too
    lines = []
    for tier in ('primary', 'secondary', 'trail'):
        for name, pts in L['roads'][tier].items():
            run = []
            pts = [tuple(x) for x in pts]
            for a, b in zip(pts, pts[1:]):
                m = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
                if keep(m):
                    run = run or [a]
                    run.append(b)
                elif run:
                    lines.append(run); run = []
            if len(run) > 1:
                lines.append(run)
    # A street that ends just short of another (a T-junction drawn a few metres off) is snapped onto it.
    for i, li in enumerate(lines):
        for end in (0, -1):
            p = li[end]
            best = None
            for j, lj in enumerate(lines):
                if j != i:
                    for a, b in zip(lj, lj[1:]):
                        q = _nearest(p, a, b)
                        if best is None or math.dist(p, q) < math.dist(p, best):
                            best = q
            if best and 0.05 < math.dist(p, best) < SNAP:
                li[end] = best
    # Split every line where it meets another, so junctions are shared nodes.
    cuts = [set() for _ in lines]
    for i, li in enumerate(lines):
        for j, lj in enumerate(lines):
            if i < j:
                for a, b in zip(li, li[1:]):
                    for c, d in zip(lj, lj[1:]):
                        x = _cross(a, b, c, d)
                        if x:
                            k = (round(x[0], 1), round(x[1], 1)); cuts[i].add(k); cuts[j].add(k)
    pieces = []
    for li, cs in zip(lines, cuts):
        pts = [li[0]]
        for a, b in zip(li, li[1:]):
            seg = math.dist(a, b) or 1
            on = sorted((math.dist(a, c) / seg, c) for c in cs if abs(math.dist(a, c) + math.dist(c, b) - seg) < 0.05)
            for _, c in on:
                if math.dist(pts[-1], c) > 0.05:
                    pts.append(c)
                if len(pts) > 1:
                    pieces.append(pts); pts = [c]
            if math.dist(pts[-1], b) > 0.05:
                pts.append(b)
        if len(pts) > 1:
            pieces.append(pts)
    # Nodes where three or more piece ends meet (or two from different streets) become junctions.
    key = lambda p: (round(p[0], 1), round(p[1], 1))
    ends = {}
    for e in pieces:
        for p, q in ((e[0], e[1]), (e[-1], e[-2])):
            k = key(p)  # from the node, not the raw end: a street snapped onto another ends a few cm off the rounded node
            ends.setdefault(k, []).append((q[0] - k[0], q[1] - k[1]))
    junctions = []
    for k, dirs in ends.items():
        if len(dirs) >= 2:
            junctions.append((k, [(dx / math.hypot(dx, dy), dy / math.hypot(dx, dy)) for dx, dy in dirs]))
    # Keep only the network joined to the Market Square: residents must be able to walk there.
    linked, todo = set(), [key(sq)]
    near_sq = min((key(e[0]) for e in pieces), key=lambda k: math.dist(k, sq))
    todo = [near_sq]
    while todo:
        k = todo.pop()
        if k in linked:
            continue
        linked.add(k)
        for e in pieces:
            a, b = key(e[0]), key(e[-1])
            if k in (a, b):
                todo.append(b if k == a else a)
    pieces = [e for e in pieces if key(e[0]) in linked]
    junctions = [j for j in junctions if j[0] in linked]
    jset = {k for k, _ in junctions}
    edges = []
    for e in pieces:  # pull piece ends back to the junction arms
        e = list(e)
        for idx, nxt in ((0, 1), (-1, -2)):
            if key(e[idx]) in jset:
                p, q = key(e[idx]), e[nxt]; d = math.dist(p, q) or 1  # arms are built from the same node, so spline end and polygon arm coincide
                e[idx] = (p[0] + (q[0] - p[0]) / d * JUNCTION_R, p[1] + (q[1] - p[1]) / d * JUNCTION_R)
        if math.dist(e[0], e[-1]) > 1.0:
            edges.append(e)
    return edges, junctions


def heights(e, ground, wet):
    """Points every SPACING m along edge e as (x, y, z metres). Over water, z runs straight between the dry points either
    side, as the bridge decks do (they pitch from bank to bank)."""
    pts = [e[0]]
    for a, b in zip(e, e[1:]):
        n = max(1, round(math.dist(a, b) / SPACING))
        pts += [(a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n) for k in range(1, n + 1)]
    z = [None if wet(*p) else ground(*p) for p in pts]
    dry = [i for i, v in enumerate(z) if v is not None]
    if not dry:
        return None
    for i, v in enumerate(z):
        if v is None:
            lo = max((d for d in dry if d < i), default=None); hi = min((d for d in dry if d > i), default=None)
            lo, hi = lo if lo is not None else hi, hi if hi is not None else lo
            z[i] = z[lo] + (z[hi] - z[lo]) * ((i - lo) / (hi - lo) if hi != lo else 0)
    return [(x, y, h) for (x, y), h in zip(pts, z)]


def check(edges, junctions):
    out = []
    if len(edges) < 5:
        out.append(f'only {len(edges)} street pieces')
    for (c, dirs) in junctions:
        if len(dirs) > 6:
            out.append(f'junction at {c} has {len(dirs)} arms')
    return out


def build_in_editor(edges, junctions, ground=None, wet=lambda x, y: False):
    """ground(x, y) -> metres: heights from the zone builder's heightfield; wet(x, y): over water (a bridge carries the
    street). Without ground, run on the saved map and trace."""
    import unreal as u
    z = (lambda x, y: ground(x, y) * 100 + 10) if ground else (lambda x, y: 0)
    zone = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
    if not ground and u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_path_name().split('.')[0] != zone:
        u.EditorLoadingAndSavingUtils.load_map(zone)
        u.log('TOWNSFOLK_LANES map loaded; run again once it has finished loading (ground traces need its collision)')
        return
    eas = u.get_editor_subsystem(u.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        if str(a.get_folder_path()) == 'Townsfolk/Lanes':
            eas.destroy_actor(a)
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    lib = u.SFTownsfolkLaneLibrary
    for i, e in enumerate(edges):
        pts = heights(e, ground, wet) if ground else [(x, y, None) for x, y in e]
        lib.spawn_footway(world, [u.Vector(x * 100, y * 100, h * 100 + 10 if ground else 0) for x, y, h in pts or []], False, f'Footway_{i:03d}', u.Vector())
    for i, (c, dirs) in enumerate(junctions):
        dirs = sorted(dirs, key=lambda d: math.atan2(d[1], d[0]))  # polygon points go round the junction
        arms = [u.Vector((c[0] + dx * JUNCTION_R) * 100, (c[1] + dy * JUNCTION_R) * 100, z(c[0] + dx * JUNCTION_R, c[1] + dy * JUNCTION_R)) for dx, dy in dirs]
        lib.spawn_footway(world, arms, True, f'Junction_{i:03d}', u.Vector(c[0] * 100, c[1] * 100, sum(a.z for a in arms) / len(arms)))
    lanes = lib.rebuild_zone_graph(world)
    for a in eas.get_all_level_actors():  # authoring visualisers: never drawn in game (GFX-02 J1b)
        if a.get_class().get_name() in ('ZoneShape', 'ZoneGraphData') or str(a.get_folder_path()) == 'Townsfolk/Lanes':
            a.set_actor_hidden_in_game(True)
    u.log(f'TOWNSFOLK_LANES pieces={len(edges)} junctions={len(junctions)} lanes={lanes}')
    assert lanes > 0, 'ZoneGraph built no lanes'
    if not ground:  # the zone builder saves the map itself
        assert u.EditorLoadingAndSavingUtils.save_current_level(), 'Could not save the zone map'
    return lanes


if __name__ == '__main__':
    L = json.loads((ROOT / 'silverbrook-town-layout.json').read_text(encoding='utf-8'))
    edges, junctions = plan(L)
    problems = check(edges, junctions)
    for p in problems:
        print('FAIL', p)
    if problems:
        sys.exit(1)
    try:
        import unreal  # noqa: F401
    except ImportError:
        # Over water the height runs straight between the last dry points, x=5 (z 2.33) and x=25 (z 3.67), whatever the bed.
        h = heights([(0, 0), (30, 0)], lambda x, y: 2 + x / 15 if not 10 <= x <= 20 else -8, lambda x, y: 10 <= x <= 20)
        assert len(h) == 7 and abs(h[3][2] - 3.0) < 1e-6, h
        print(f'LANES_PLAN_OK {len(edges)} street pieces, {len(junctions)} junctions, '
              f'{sum(math.dist(a, b) for e in edges for a, b in zip(e, e[1:])):.0f} m of footway')
    else:
        build_in_editor(edges, junctions)
