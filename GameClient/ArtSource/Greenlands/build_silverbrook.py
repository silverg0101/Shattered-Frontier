"""Build the Silverbrook graybox zone map from silverbrook-town-layout.json.

Run with the UE 5.8 Python commandlet (-run=pythonscript) or full editor. Replaces
/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook with a blocked-out town at real
scale: terrain shaped by heightfield.py with its mountain backdrop, Silverbrook River, brooks and
basins, roads and bridges, wall with four gates, key buildings, district houses in rows along the
streets, construction plots, labels and the arrival player start.
Everything is planned first, then spawned onto the terrain once its pads are known. North is -Y,
east is +X. Script any zone-only change here; the map is regenerated, not hand-edited.
See docs/greenlands-silverbrook-zone-plan.md.
"""
import json, math, random, sys, zlib
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
import importlib, validate_layout, validate_region, region_heightfield, heightfield, dress_zone, pcg_dressing  # an open editor
for _m in (validate_layout, validate_region, region_heightfield, heightfield, dress_zone, pcg_dressing):        # keeps modules between
    importlib.reload(_m)                                                                                         # runs; take the files' state
from validate_layout import validate, waters
from heightfield import Heightfield, backdrop, check as check_terrain, in_polygon, water_level, zone_region
L = json.loads((ROOT / 'silverbrook-town-layout.json').read_text(encoding='utf-8'))
_problems = validate(L)
assert not _problems, f'Layout is invalid; run validate_layout.py: {_problems}'
ZONE = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
MAT_DIR = '/Game/Frontier/Core/Graybox'
TERRAIN = '/Game/Frontier/Environment/Greenlands/Terrain/SM_Silverbrook_Terrain'
BACKDROP = '/Game/Frontier/Environment/Greenlands/Terrain/SM_Silverbrook_Backdrop'
CUBE = u.load_asset('/Engine/BasicShapes/Cube')
COLOURS = {'Ground': (0.16, 0.30, 0.11), 'Road': (0.55, 0.45, 0.30), 'Trail': (0.38, 0.30, 0.19),
           'River': (0.08, 0.25, 0.55), 'Wall': (0.42, 0.42, 0.45), 'Civic': (0.82, 0.80, 0.72),
           'House': (0.62, 0.42, 0.28), 'Plot': (0.88, 0.76, 0.18), 'Field': (0.42, 0.48, 0.14),
           'Marker': (0.75, 0.10, 0.08), 'Rock': (0.36, 0.37, 0.40)}
TYPES = L['building_types']
PROFILE = json.loads((ROOT / 'greenlands-look-profile.json').read_text(encoding='utf-8'))
# Layout building type -> look-profile building; its delivered meshes ('have') replace the graybox box.
KIND_BUILDING = {'town_hall': 'Town Hall', 'adventurers_guild': "Adventurers' Guild", 'inn': 'The Golden Stag Inn',
                 'blacksmith': 'Blacksmith', 'temple': 'Temple of the Dawn', 'market_stall': 'Market stall',
                 'watchtower': 'Watchtower', 'stables': 'Stables', 'workshop': 'Workshop', 'farmhouse': 'Farmhouse',
                 'cottage': 'Cottage', 'townhouse': 'Townhouse', 'elite_house': 'Northridge house'}
_have = {k: PROFILE['buildings'].get(b, {}).get('have', []) for k, b in KIND_BUILDING.items()}
_meshes = pcg_dressing._meshes({n for names in _have.values() for n in names})
VARIANTS = {k: [_meshes[n] for n in names] for k, names in _have.items() if names}
HALF = L['zone_half_extent']
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
counts = {}
pending = []  # spawns wait for the terrain, which needs every building pad first
HF = None


def material(name, rgb):
    path = f'{MAT_DIR}/MI_Graybox_{name}'
    if u.EditorAssetLibrary.does_asset_exist(path):
        mi = u.load_asset(path)
    else:
        mi = u.AssetToolsHelpers.get_asset_tools().create_asset(
            f'MI_Graybox_{name}', MAT_DIR, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
        u.MaterialEditingLibrary.set_material_instance_parent(mi, u.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
    names = [str(n) for n in u.MaterialEditingLibrary.get_vector_parameter_names(mi.get_editor_property('parent'))]
    colour = next((n for n in names if 'colo' in n.lower()), None)
    assert colour, f'No colour parameter on BasicShapeMaterial: {names}'
    u.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, colour, u.LinearColor(*rgb, 1.0))
    got = u.MaterialEditingLibrary.get_material_instance_vector_parameter_value(mi, colour)
    assert abs(got.r - rgb[0]) < 1e-3 and abs(got.b - rgb[2]) < 1e-3, f'{name} colour not applied: {got}'
    u.EditorAssetLibrary.save_loaded_asset(mi)
    return mi


def box(label, x, y, w, d, h, mat, z0=0.0, yaw=0.0, collide=True, folder='Graybox', ground='min'):
    """Queue a box. ground='min' rests it on the lowest footprint corner, sunk 1 m so slopes show no gap;
    'centre' puts z0 on the ground at its centre; None keeps z0 absolute."""
    pending.append(lambda: _box(label, x, y, w, d, h, mat, z0, yaw, collide, folder, ground))
    counts[folder] = counts.get(folder, 0) + 1


def _box(label, x, y, w, d, h, mat, z0, yaw, collide, folder, ground, pitch=0.0):
    if ground == 'min':
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        z0 += min(HF.at(x + c * ox - s * oy, y + s * ox + c * oy) for ox in (-w / 2, w / 2) for oy in (-d / 2, d / 2)) - 1
        h += 1
    elif ground == 'centre':
        z0 += HF.at(x, y)
    a = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(x * 100, y * 100, (z0 + h / 2) * 100), u.Rotator(roll=0, pitch=pitch, yaw=yaw))
    a.static_mesh_component.set_static_mesh(CUBE)
    a.set_actor_scale3d(u.Vector(w, d, h))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    a.static_mesh_component.set_material(0, M[mat])
    if not collide:
        a.static_mesh_component.set_collision_profile_name('NoCollision')  # the profile is reapplied on load; a bare flag is not kept
    return a


def model(label, kind, x, y, yaw, folder):
    """Queue a delivered building mesh, chosen by a stable hash of the label. `yaw` turns the front toward -Y; the
    Blender packages face -Y, which the FBX import flips to +Y, so the actor turns a further 180 degrees.
    The terrain is levelled under it (Heightfield foundations); where a 5 m grid can't hold it level, the building
    stands on the highest ground under its footprint and a stone plinth fills down to below the lowest."""
    def spawn(yaw=yaw + 180):
        mesh = VARIANTS[kind][zlib.crc32(label.encode()) % len(VARIANTS[kind])]
        b = mesh.get_bounding_box()
        w, d = (b.max.x - b.min.x) / 100, (b.max.y - b.min.y) / 100
        ox, oy = (b.max.x + b.min.x) / 200, (b.max.y + b.min.y) / 200
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        px, py = x + c * ox - s * oy, y + s * ox + c * oy
        lo, hi = HF.footprint(px, py, w, d, yaw)
        a = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(x * 100, y * 100, hi * 100), u.Rotator(roll=0, pitch=0, yaw=yaw))
        a.static_mesh_component.set_static_mesh(mesh)
        a.set_actor_label(label); a.set_folder_path(folder)
        if kind == 'townhouse' and 'street' not in foundation:
            foundation['street'] = (x, y, yaw - 180, hi)  # first townhouse: the street camera looks at its front
        foundation['max_step'] = max(foundation['max_step'], round(hi - lo, 2))
        if hi - lo > 0.05:
            _box(f'{label}_Plinth', px, py, w, d, hi - lo + 0.3, 'Wall', lo - 0.3, yaw, True, folder, None)
            foundation['plinths'] += 1
    pending.append(spawn)
    counts[folder] = counts.get(folder, 0) + 1
    counts['Models'] = counts.get('Models', 0) + 1


def facing(x, y, tx, ty):
    """Yaw that turns a model's front (-Y) from (x, y) toward (tx, ty)."""
    return math.degrees(math.atan2(ty - y, tx - x)) + 90


def strip(label, points, width, mat, h, folder, z_at, collide=False, piece=10.0):
    """Queue thin boxes along a polyline in pieces of at most `piece` m, pitched to follow z_at(i, t)."""
    for i, (p, q) in enumerate(zip(points, points[1:])):
        length = math.dist(p, q); n = max(1, math.ceil(length / piece))
        yaw = math.degrees(math.atan2(q[1] - p[1], q[0] - p[0]))
        for k in range(n):
            t0 = k / n - (width / 2 / length if k == 0 else 0)  # segment ends overlap so corners stay closed
            t1 = (k + 1) / n + (width / 2 / length if k == n - 1 else 0)
            def spawn(i=i, p=p, q=q, k=k, t0=t0, t1=t1, yaw=yaw, length=length):
                za, zb = z_at(i, max(t0, 0.0)), z_at(i, min(t1, 1.0))
                run = (t1 - t0) * length; tm = (t0 + t1) / 2
                _box(f'{label}_{i:02d}_{k:02d}', p[0] + (q[0] - p[0]) * tm, p[1] + (q[1] - p[1]) * tm, run, width, h, mat,
                     (za + zb) / 2 - h / 2, yaw, collide, folder, None, math.degrees(math.atan2(zb - za, run)))
            pending.append(spawn)
            counts[folder] = counts.get(folder, 0) + 1


def on_ground(pts):
    return lambda i, t: HF.at(pts[i][0] + (pts[i + 1][0] - pts[i][0]) * t, pts[i][1] + (pts[i + 1][1] - pts[i][1]) * t)


def label(text, x, y, z, size):
    pending.append(lambda: _label(text, x, y, z + HF.at(x, y), size))


def _label(text, x, y, z, size):
    a = actors.spawn_actor_from_class(u.TextRenderActor, u.Vector(x * 100, y * 100, z * 100), u.Rotator(roll=0, pitch=0, yaw=90))
    comp = a.text_render
    comp.set_editor_property('text', text)
    comp.set_editor_property('world_size', size * 100)
    comp.set_editor_property('horizontal_alignment', u.HorizTextAligment.EHTA_CENTER)
    comp.set_editor_property('text_render_color', u.Color(255, 240, 200, 255))
    a.set_actor_label(f'Label_{text}')
    a.set_folder_path('Labels')


def seg_dist(p, a, b):
    ax, ay, bx, by = *a, *b
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / (dx * dx + dy * dy)))
    return math.dist(p, (ax + t * dx, ay + t * dy))


def nearest(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / (dx * dx + dy * dy)))
    return (a[0] + t * dx, a[1] + t * dy)


def line_dist(p, pts):
    return min(seg_dist(p, a, b) for a, b in zip(pts, pts[1:]))


def crossing(a, b, c, d):
    """Intersection point of segments ab and cd, or None."""
    r = (b[0] - a[0], b[1] - a[1]); s = (d[0] - c[0], d[1] - c[1])
    den = r[0] * s[1] - r[1] * s[0]
    if abs(den) < 1e-9:
        return None
    t = ((c[0] - a[0]) * s[1] - (c[1] - a[1]) * s[0]) / den
    v = ((c[0] - a[0]) * r[1] - (c[1] - a[1]) * r[0]) / den
    return (a[0] + t * r[0], a[1] + t * r[1]) if 0 <= t <= 1 and 0 <= v <= 1 else None


# --- layout geometry -------------------------------------------------------
W = L['wall']; A, B = W['semi_axis_x'], W['semi_axis_y']
WATERS = waters(L)  # the river and every brook
BASINS = L.get('basins', [])
ROADS = [(name, pts, L['roads'][f'{tier}_width'], tier)
         for tier in ('primary', 'secondary', 'trail') for name, pts in L['roads'][tier].items()]
ellipse = []
prev, total = None, 0.0
for i in range(4001):
    t = 2 * math.pi * i / 4000
    p = (A * math.cos(t), B * math.sin(t))
    total += math.dist(prev, p) if prev else 0.0
    ellipse.append((total, p, t)); prev = p
gate_points = {}
for name, g in L['gates'].items():
    t = math.atan2(g[1] / B, g[0] / A)
    gate_points[name] = (A * math.cos(t), B * math.sin(t))
placed = []  # (x, y, radius)
footprints = []  # (x, y, w, d, yaw) of every building on land, levelled last in the terrain
foundation = dict(max_step=0.0, plinths=0)  # ground height range under the delivered buildings
flats = []  # (x, y, radius) open areas to level: plaza, fields, yard, ruins
blocked = []  # required places that overlap a road, the river, the wall or a building


def wall_dist(p):
    return min(math.dist(p, e[1]) for e in ellipse[::8])


def inside(p):
    return (p[0] / A) ** 2 + (p[1] / B) ** 2 < 1


def water_gap(p):
    """Distance from p to the nearest water edge; negative inside a basin."""
    gaps = [line_dist(p, w['points']) - w['width'] / 2 for w in WATERS]
    gaps += [-1.0 if in_polygon(p, b['polygon']) else line_dist(p, b['polygon'] + b['polygon'][:1]) for b in BASINS]
    return min(gaps)


def clear(x, y, w, d, margin=2.0, on_water=False):
    r = math.hypot(w, d) / 2
    p = (x, y)
    if abs(x) + r > HALF or abs(y) + r > HALF:
        return False
    if not on_water and water_gap(p) < r + 4:
        return False
    if any(line_dist(p, pts) < r + width / 2 + margin for _, pts, width, _ in ROADS):
        return False
    if wall_dist(p) < r + 6:
        return False
    return all(math.dist(p, (q[0], q[1])) > r + q[2] + margin for q in placed)


def building(label_text, kind, x, y, yaw, mat, folder, require=True):
    w, d, h = TYPES[kind]
    ok = clear(x, y, w, d, on_water=kind in ('docks', 'wharf'))
    if require and not ok:
        blocked.append(f'{label_text} at {x:.0f},{y:.0f}')
    if ok:
        placed.append((x, y, math.hypot(w, d) / 2))
        if kind not in ('docks', 'wharf'):
            footprints.append((x, y, w, d, yaw))
        if kind in VARIANTS:
            model(label_text, kind, x, y, yaw, folder)
        else:
            box(label_text, x, y, w, d, h, mat, yaw=yaw, folder=folder)
    return ok


# --- build ------------------------------------------------------------------
if '-run=' not in u.SystemLibrary.get_command_line().lower():
    # Running inside an open editor (tools/Run-InEditor.py): never discard the user's unsaved work.
    dirty = [p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
             + u.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    # Some maps mark themselves dirty on load; --discard-unsaved is the owner's explicit go-ahead.
    assert not dirty or '--discard-unsaved' in sys.argv, \
        f'Save or discard these first, or pass --discard-unsaved: {dirty}'
M = {name: material(name, rgb) for name, rgb in COLOURS.items()}
world = u.EditorLoadingAndSavingUtils.new_blank_map(False)  # also closes the zone map if it is open
if u.EditorAssetLibrary.does_asset_exist(ZONE):
    assert u.EditorAssetLibrary.delete_asset(ZONE), 'Could not delete the previous zone map'

sun = actors.spawn_actor_from_class(u.DirectionalLight, u.Vector(0, 0, 50000), u.Rotator(roll=0, pitch=-40, yaw=-60))
sun.light_component.set_editor_property('atmosphere_sun_light', True)
actors.spawn_actor_from_class(u.SkyAtmosphere, u.Vector(0, 0, 0))
sky = actors.spawn_actor_from_class(u.SkyLight, u.Vector(0, 0, 1000))
sky.light_component.set_editor_property('real_time_capture', True)
actors.spawn_actor_from_class(u.ExponentialHeightFog, u.Vector(0, 0, 0))

for w in WATERS:
    strip(w['name'].replace(' ', ''), w['points'], w['width'], 'River', 0.1, 'Water', lambda i, t, w=w: water_level(w, i, t))
for b in BASINS:  # one flat surface at the basin level; the shore terrain hides what lies outside the polygon
    xs, ys = [q[0] for q in b['polygon']], [q[1] for q in b['polygon']]
    box(b['name'].replace(' ', ''), (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, max(xs) - min(xs), max(ys) - min(ys),
        0.1, 'River', z0=b['level'] - 0.1, collide=False, folder='Water', ground=None)
for name, pts, width, tier in ROADS:
    strip(name.replace(' ', ''), pts, width, 'Trail' if tier == 'trail' else 'Road', 0.3, f'Roads/{tier.title()}', on_ground(pts))

# Bridges where primary or secondary roads cross the river or a brook.
bridges = []
for name, pts, width, tier in ROADS:
    if tier == 'trail':
        continue
    for a, b in zip(pts, pts[1:]):
        for wtr, (c, d) in ((w, cd) for w in WATERS for cd in zip(w['points'], w['points'][1:])):
            hit = crossing(a, b, c, d)
            if hit and all(math.dist(hit, q) > 5 for q in bridges):
                bridges.append(hit)
                yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
                is_main = math.dist(hit, L['places'][18]['at']) < 40
                level = water_level(wtr, wtr['points'].index(c), math.dist(c, hit) / math.dist(c, d))
                cs, sn = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

                def spawn_bridge(n=name, h=hit, yaw=yaw, cs=cs, sn=sn, level=level, is_main=is_main,
                                 w=max(L['river']['bridge_width'], width)):
                    # Each end reaches dry ground (roads cross the river at an angle), and the deck
                    # tilts to meet the ground at both ends, so the walk never meets a step.
                    ends = []
                    for side in (-1, 1):
                        s = L['river']['bridge_length'] / 2
                        while s < 60 and HF.at(h[0] + side * cs * s, h[1] + side * sn * s) < level + 1:
                            s += 1
                        ends.append((s, HF.at(h[0] + side * cs * s, h[1] + side * sn * s)))
                    (s0, z0), (s1, z1) = ends
                    run, mid = s0 + s1, (s1 - s0) / 2
                    _box('SilverbrookBridge' if is_main else f'Bridge_{n.replace(" ", "")}', h[0] + cs * mid, h[1] + sn * mid,
                         run + 2, w, 0.3, 'Wall', (z0 + z1) / 2 - 0.25, yaw, True, 'Bridges', None,
                         math.degrees(math.atan2(z1 - z0, run)))
                pending.append(spawn_bridge)
                counts['Bridges'] = counts.get('Bridges', 0) + 1

# Wall, gate towers and wall towers.
tower = W['tower']
for name, g in gate_points.items():
    t = math.atan2(g[1] / B, g[0] / A)
    tx, ty = -A * math.sin(t), B * math.cos(t); n = math.hypot(tx, ty); tx, ty = tx / n, ty / n
    off = W['gate_gap'] / 2 + tower[0] / 2
    for side in (-1, 1):
        box(f'{name.replace(" ", "")}_Tower{side:+d}', g[0] + side * off * tx, g[1] + side * off * ty, *tower, 'Wall', folder='Wall')
    placed.append((g[0], g[1], off + tower[0]))
step, next_tower = W['segment_length'], W['tower_spacing'] / 2
s, idx = 0.0, 0
while s < total - step:
    while ellipse[idx][0] < s:
        idx += 1
    j = idx
    while j < len(ellipse) - 1 and ellipse[j][0] < s + step:
        j += 1
    p, q = ellipse[idx][1], ellipse[j][1]
    mid = ((p[0] + q[0]) / 2, (p[1] + q[1]) / 2)
    near_gate = min(math.dist(mid, g) for g in gate_points.values()) < W['gate_gap'] / 2 + tower[0] + step / 2
    near_water = water_gap(mid) < step / 2 + 2
    if not near_gate and not near_water:
        yaw = math.degrees(math.atan2(q[1] - p[1], q[0] - p[0]))
        box(f'Wall_{int(s):04d}', mid[0], mid[1], step + 0.5, W['thickness'], W['height'], 'Wall', yaw=yaw, folder='Wall')
        if s >= next_tower:
            box(f'WallTower_{int(s):04d}', mid[0], mid[1], *tower, 'Wall', yaw=yaw, folder='Wall')
            next_tower = s + W['tower_spacing']
    s += step

# Key places.
for place in L['places']:
    kind, name = place['type'], place['name']
    tag = f"{place['key']:02d}_{name.replace(' ', '').replace('&', 'And').replace(chr(39), '')}"
    if kind == 'gate':
        g = gate_points[name]; label(name, g[0], g[1], 16, 6)
        continue
    x, y = place.get('at', (0, 0))
    if kind in TYPES:
        road = min(((name, pts) for name, pts, _, tier in ROADS if tier != 'trail'), key=lambda r: line_dist((x, y), r[1]))[1]
        near = min((nearest((x, y), a, b) for a, b in zip(road, road[1:])), key=lambda q: math.dist(q, (x, y)))
        building(tag, kind, x, y, place.get('yaw') or facing(x, y, *near), 'Civic', 'Places')
        if kind == 'mill':
            building(f'{tag}_Granary', 'granary', x + 17, y - 6, 0, 'Civic', 'Places')
    elif kind == 'plaza':
        w, d = place['size']
        box(tag, x, y, w, d, 0.08, 'Civic', collide=False, folder='Places', ground='centre')
        placed.append((x, y, 6))  # keep the centre open; stalls ring the edge
        flats.append((x, y, math.hypot(w, d) / 2))
        stalls = 0
        for k in range(32):
            if stalls == L['market_stalls']:
                break
            ang = 2 * math.pi * k / 32
            sx, sy = x + (w / 2 - 4) * math.cos(ang), y + (d / 2 - 4) * math.sin(ang)
            stalls += building(f'MarketStall_{stalls:02d}', 'market_stall', sx, sy, 0, 'House', 'Places', require=False)
    elif kind in ('fields', 'yard'):
        w, d = place['size']
        box(tag, x, y, w, d, 0.1, 'Field' if kind == 'fields' else 'Plot', collide=False, folder='Places', ground='centre')
        flats.append((x, y, math.hypot(w, d) / 2))
    elif kind == 'ruins':
        flats.append((x, y, math.hypot(*place['size']) / 2))
        for k, (ox, oy, h) in enumerate(((-8, -5, 3), (6, -6, 2), (-4, 7, 4), (9, 5, 1.5))):
            box(f'{tag}_{k}', x + ox, y + oy, 5, 2, h, 'Wall', folder='Places')
    elif kind == 'marker':
        box(tag, x, y, 1, 1, 10, 'Marker', folder='Places')
    label(name, x, y, 14, 4)

# Wave 0 construction plots, before district fill so houses keep clear of them.
size = L['construction_plots']['size']
for i, (x, y) in enumerate(L['construction_plots']['centres']):
    if not clear(x, y, size, size, margin=1):
        blocked.append(f'ConstructionPlot_{i} at {x},{y}')
        continue
    placed.append((x, y, size * 0.71))
    box(f'ConstructionPlot_{i}', x, y, size, size, 0.05, 'Plot', collide=False, folder='Plots', ground='centre')

assert not blocked, f'Blocked places (move them in the layout): {blocked}'

# District fill: houses in rows along both sides of every street, facing it; seeded scatter fills the rest.
rng = random.Random(L['seed'])
fill_report = {}


def frontage(kind):
    """Lots along both sides of every non-trail road, facing it, spaced so neighbours just clear."""
    w, d, _ = TYPES[kind]; r = math.hypot(w, d) / 2
    for _, pts, width, tier in ROADS:
        if tier == 'trail':
            continue
        off = width / 2 + r + 2.5
        for a, b in zip(pts, pts[1:]):
            seg = math.dist(a, b); ux, uy = (b[0] - a[0]) / seg, (b[1] - a[1]) / seg
            yaw = math.degrees(math.atan2(uy, ux))
            t = r
            while t < seg - r:
                for side in (-1, 1):  # +Y of the lot faces away from the road, so the model's front (-Y) faces it
                    yield a[0] + ux * t - side * uy * off, a[1] + uy * t + side * ux * off, yaw if side == 1 else yaw + 180
                t += 2 * r + 2.5


for name, dist in L['districts'].items():
    cx, cy = dist['centre']; kind = dist['fill']; made = rows = 0
    poly = dist['polygon']
    tag = lambda: f'{name.replace(" ", "")}_{kind}_{made:03d}'
    for x, y, yaw in frontage(kind):
        if made == dist['count']:
            break
        if in_polygon((x, y), poly) and inside((x, y)) == dist['inside_wall']:
            ok = building(tag(), kind, x, y, yaw, 'House', f'Districts/{name}', require=False)
            made += ok; rows += ok
    xs, ys = [q[0] for q in poly], [q[1] for q in poly]
    for attempt in range(dist['count'] * 40):
        if made == dist['count']:
            break
        x, y = rng.uniform(min(xs), max(xs)), rng.uniform(min(ys), max(ys))
        if not in_polygon((x, y), poly) or inside((x, y)) != dist['inside_wall']:
            continue
        yaw = round(facing(x, y, cx, cy) / 15) * 15
        made += building(tag(), kind, x, y, yaw, 'House', f'Districts/{name}', require=False)
    fill_report[name] = f"{made}/{dist['count']} ({rows} in rows)"
    label(name, cx, cy, 30, 10)

# Arrival.
ax, ay = L['arrival']['at']; fx, fy = L['arrival']['face']
yaw = math.degrees(math.atan2(fy - ay, fx - ax))
box('Arrival_Waystone', ax + 7, ay - 9, 1.5, 1.5, 3, 'Civic', folder='Places')  # roadside, ahead and to the right
label('Arrival', ax, ay, 8, 3)

# Terrain: shaped from the layout's elevation with every building, plot and open area levelled.
REGION, DATUM, SEA = zone_region('silverbrook-town-layout.json')  # region first: edges and backdrop come from it
HF = Heightfield(L, pads=placed + flats, region=REGION, foundations=footprints)
terrain_problems, terrain_stats = check_terrain(L, HF)
assert not terrain_problems, f'Terrain checks failed: {terrain_problems}'
if u.EditorAssetLibrary.does_asset_exist(TERRAIN):
    assert u.EditorAssetLibrary.delete_asset(TERRAIN), 'Could not delete the previous terrain mesh'
mesh = u.DynamicMesh()
u.GeometryScript_Primitives.append_rectangle_xy(mesh, u.GeometryScriptPrimitiveOptions(), u.Transform(),
                                                dimension_x=2 * HALF * 100, dimension_y=2 * HALF * 100,
                                                steps_width=HF.nx - 1, steps_height=HF.ny - 1)
grid = u.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False)[1]
moved = [u.Vector(v.x, v.y, HF.at(v.x / 100, v.y / 100) * 100) for v in u.GeometryScript_List.convert_vector_list_to_array(grid)]
u.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(mesh, u.GeometryScript_List.convert_array_to_vector_list(moved))
opts = u.GeometryScriptCreateNewStaticMeshAssetOptions()
opts.set_editor_property('enable_recompute_normals', True)
opts.set_editor_property('enable_collision', True)
opts.set_editor_property('collision_mode', u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
terrain_mesh = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, TERRAIN, opts)[0]
assert terrain_mesh, 'Could not create the terrain mesh'
u.EditorAssetLibrary.save_loaded_asset(terrain_mesh)
ground = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(0, 0, 0))
ground.static_mesh_component.set_static_mesh(terrain_mesh)
ground.static_mesh_component.set_material(0, M['Ground'])
ground.set_actor_label('Terrain'); ground.set_folder_path('Ground')

# Backdrop: the region terrain beyond the zone edge, visual only, with the sea at the region's sea level.
n, sp, bz = backdrop(L, HF, REGION)
ext = L['backdrop']['extent']
if u.EditorAssetLibrary.does_asset_exist(BACKDROP):
    assert u.EditorAssetLibrary.delete_asset(BACKDROP), 'Could not delete the previous backdrop mesh'
far = u.DynamicMesh()
u.GeometryScript_Primitives.append_rectangle_xy(far, u.GeometryScriptPrimitiveOptions(), u.Transform(),
                                                dimension_x=2 * ext * 100, dimension_y=2 * ext * 100,
                                                steps_width=n - 1, steps_height=n - 1)
grid = u.GeometryScript_MeshQueries.get_all_vertex_positions(far, False)[1]
moved = [u.Vector(v.x, v.y, bz[round((v.y / 100 + ext) / sp) * n + round((v.x / 100 + ext) / sp)] * 100)
         for v in u.GeometryScript_List.convert_vector_list_to_array(grid)]
u.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(far, u.GeometryScript_List.convert_array_to_vector_list(moved))
opts.set_editor_property('enable_collision', False)
backdrop_mesh = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(far, BACKDROP, opts)[0]
assert backdrop_mesh, 'Could not create the backdrop mesh'
u.EditorAssetLibrary.save_loaded_asset(backdrop_mesh)
mountains = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(0, 0, 0))
mountains.static_mesh_component.set_static_mesh(backdrop_mesh)
mountains.static_mesh_component.set_material(0, M['Rock'])
mountains.static_mesh_component.set_collision_profile_name('NoCollision')
mountains.set_actor_label('Backdrop'); mountains.set_folder_path('Ground')
_box('Sea', 0, 0, 2 * ext, 2 * ext, 0.1, 'River', SEA - 0.1, 0, False, 'Water', None)

for spawn in pending:
    spawn()
actors.spawn_actor_from_class(u.PlayerStart, u.Vector(ax * 100, ay * 100, (HF.at(ax, ay) + 1.2) * 100),
                              u.Rotator(roll=0, pitch=0, yaw=yaw)).set_actor_label('Arrival_PlayerStart')

# Aerial review camera, looking north-west over the town from beyond the South Gate.
cam = actors.spawn_actor_from_class(u.CameraActor, u.Vector(30000, 90000, 60000), u.Rotator(roll=0, pitch=-38, yaw=-105))
cam.set_actor_label('ReviewCamera_Aerial')
cam.camera_component.set_editor_property('field_of_view', 70)

# Eye-level camera inside Eastwood, looking into the dressed woodland toward the eastern hills.
ex, ey, tx, ty = 520, 120, 640, -150
yaw_e = math.degrees(math.atan2(ty - ey, tx - ex))
cam_e = actors.spawn_actor_from_class(u.CameraActor, u.Vector(ex * 100, ey * 100, (HF.at(ex, ey) + 1.7) * 100), u.Rotator(roll=0, pitch=-3, yaw=yaw_e))
cam_e.set_actor_label('ReviewCamera_Eastwood')
cam_e.camera_component.set_editor_property('field_of_view', 75)

# Street camera 14 m in front of the first townhouse, looking back at its door.
hx, hy, hyaw, hz = foundation.pop('street')
fx_, fy_ = math.sin(math.radians(hyaw)), -math.cos(math.radians(hyaw))  # the front (-Y) direction
cam_s = actors.spawn_actor_from_class(u.CameraActor, u.Vector((hx + 14 * fx_) * 100, (hy + 14 * fy_) * 100, (hz + 5) * 100),
                                      u.Rotator(roll=0, pitch=-10, yaw=math.degrees(math.atan2(-fy_, -fx_))))
cam_s.set_actor_label('ReviewCamera_Street')
cam_s.camera_component.set_editor_property('field_of_view', 75)

assert len(bridges) >= 3, f'Expected at least 3 bridges, built {len(bridges)}'

# Dressing (pipeline stage 3): rules from the look profile decide placements; a PCG graph spawns them.
DRESS_DISTRICTS = list(L['districts'])  # each district's 'dress' block scales the look profile's densities
dressing = {}
for district in DRESS_DISTRICTS:
    result = dress_zone.dress(L, PROFILE, HF, placed, district, L['seed'])
    cands = dress_zone.candidates(result, PROFILE)
    dress_problems = dress_zone.check(L, result, cands, placed)
    assert not dress_problems, f'{district} dressing failed its checks: {dress_problems}'
    (ROOT / f"silverbrook-{district.lower().replace(' ', '-')}-candidates.json").write_text(json.dumps(dict(
        _about='Gameplay candidates (harvestable trees and stone) from dress_zone.py; produced but not used yet (pipeline stage 3).',
        version=result['version'], candidates=cands), indent=1) + chr(10), encoding='utf-8')
    _, expected = pcg_dressing.build(result, {l['role']: l['layer'] for l in PROFILE['layers']}, 'Silverbrook')
    dressing[district] = dict(version=result['version'], instances=expected, hash=dress_zone.transforms_hash(result),
                              candidates=len(cands), gaps=result['gaps'])
assert u.EditorLoadingAndSavingUtils.save_map(world, ZONE), 'Could not save the zone map'
u.log(f'SILVERBROOK_BUILT Map={ZONE} Datum={DATUM} Bridges={len(bridges)} Fill={fill_report} Terrain={terrain_stats} Foundations={foundation} Dressing={dressing} Counts={counts}')
