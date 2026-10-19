"""Build the Silverbrook graybox zone map from silverbrook-town-layout.json.

Run with the UE 5.8 Python commandlet (-run=pythonscript) or full editor. Replaces
/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook with a blocked-out town at real
scale: ground, Silverbrook River, roads and bridges, wall with four gates, key buildings,
district fill, construction plots, labels and the arrival player start. North is -Y,
east is +X. Script any zone-only change here; the map is regenerated, not hand-edited.
See docs/greenlands-silverbrook-zone-plan.md.
"""
import json, math, random
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parent
L = json.loads((ROOT / 'silverbrook-town-layout.json').read_text(encoding='utf-8'))
ZONE = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
MAT_DIR = '/Game/Frontier/Core/Graybox'
CUBE = u.load_asset('/Engine/BasicShapes/Cube')
COLOURS = {'Ground': (0.16, 0.30, 0.11), 'Road': (0.55, 0.45, 0.30), 'Trail': (0.38, 0.30, 0.19),
           'River': (0.08, 0.25, 0.55), 'Wall': (0.42, 0.42, 0.45), 'Civic': (0.82, 0.80, 0.72),
           'House': (0.62, 0.42, 0.28), 'Plot': (0.88, 0.76, 0.18), 'Field': (0.42, 0.48, 0.14),
           'Marker': (0.75, 0.10, 0.08)}
TYPES = L['building_types']
HALF = L['zone_half_extent']
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
counts = {}


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


def box(label, x, y, w, d, h, mat, z0=0.0, yaw=0.0, collide=True, folder='Graybox'):
    a = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(x * 100, y * 100, (z0 + h / 2) * 100), u.Rotator(roll=0, pitch=0, yaw=yaw))
    a.static_mesh_component.set_static_mesh(CUBE)
    a.set_actor_scale3d(u.Vector(w, d, h))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    a.static_mesh_component.set_material(0, M[mat])
    if not collide:
        a.static_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    counts[folder] = counts.get(folder, 0) + 1
    return a


def strip(label, points, width, mat, h, folder, collide=False):
    for i, (p, q) in enumerate(zip(points, points[1:])):
        length = math.dist(p, q)
        yaw = math.degrees(math.atan2(q[1] - p[1], q[0] - p[0]))
        box(f'{label}_{i:02d}', (p[0] + q[0]) / 2, (p[1] + q[1]) / 2, length + width, width, h, mat, yaw=yaw, collide=collide, folder=folder)


def label(text, x, y, z, size):
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
RIVER = L['river']['points']; RIVER_W = L['river']['width']
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
blocked = []  # required places that overlap a road, the river, the wall or a building


def wall_dist(p):
    return min(math.dist(p, e[1]) for e in ellipse[::8])


def inside(p):
    return (p[0] / A) ** 2 + (p[1] / B) ** 2 < 1


def clear(x, y, w, d, margin=2.0, on_water=False):
    r = math.hypot(w, d) / 2
    p = (x, y)
    if abs(x) + r > HALF or abs(y) + r > HALF:
        return False
    if not on_water and line_dist(p, RIVER) < r + RIVER_W / 2 + 4:
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
        box(label_text, x, y, w, d, h, mat, yaw=yaw, folder=folder)
    return ok


# --- build ------------------------------------------------------------------
if u.EditorAssetLibrary.does_asset_exist(ZONE):
    assert u.EditorAssetLibrary.delete_asset(ZONE), 'Could not delete the previous zone map'
M = {name: material(name, rgb) for name, rgb in COLOURS.items()}
world = u.EditorLoadingAndSavingUtils.new_blank_map(False)

sun = actors.spawn_actor_from_class(u.DirectionalLight, u.Vector(0, 0, 50000), u.Rotator(roll=0, pitch=-40, yaw=-60))
sun.light_component.set_editor_property('atmosphere_sun_light', True)
actors.spawn_actor_from_class(u.SkyAtmosphere, u.Vector(0, 0, 0))
sky = actors.spawn_actor_from_class(u.SkyLight, u.Vector(0, 0, 1000))
sky.light_component.set_editor_property('real_time_capture', True)
actors.spawn_actor_from_class(u.ExponentialHeightFog, u.Vector(0, 0, 0))

box('Ground', 0, 0, 2 * HALF, 2 * HALF, 1, 'Ground', z0=-1, folder='Ground')
strip('River', RIVER, RIVER_W, 'River', 0.04, 'River')
for name, pts, width, tier in ROADS:
    strip(name.replace(' ', ''), pts, width, 'Trail' if tier == 'trail' else 'Road', 0.06, f'Roads/{tier.title()}')

# Bridges where primary or secondary roads cross the river.
bridges = []
for name, pts, width, tier in ROADS:
    if tier == 'trail':
        continue
    for a, b in zip(pts, pts[1:]):
        for c, d in zip(RIVER, RIVER[1:]):
            hit = crossing(a, b, c, d)
            if hit and all(math.dist(hit, q) > 5 for q in bridges):
                bridges.append(hit)
                yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
                is_main = math.dist(hit, L['places'][18]['at']) < 40
                box('SilverbrookBridge' if is_main else f'Bridge_{name.replace(" ", "")}', hit[0], hit[1],
                    L['river']['bridge_length'], max(L['river']['bridge_width'], width), 0.3, 'Wall', yaw=yaw, folder='Bridges')

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
    near_river = line_dist(mid, RIVER) < RIVER_W / 2 + step / 2 + 2
    if not near_gate and not near_river:
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
        building(tag, kind, x, y, place.get('yaw', 0), 'Civic', 'Places')
        if kind == 'mill':
            building(f'{tag}_Granary', 'granary', x + 17, y - 6, 0, 'Civic', 'Places')
    elif kind == 'plaza':
        w, d = place['size']
        box(tag, x, y, w, d, 0.08, 'Civic', collide=False, folder='Places')
        placed.append((x, y, 6))  # keep the centre open; stalls ring the edge
        stalls = 0
        for k in range(32):
            if stalls == L['market_stalls']:
                break
            ang = 2 * math.pi * k / 32
            sx, sy = x + (w / 2 - 4) * math.cos(ang), y + (d / 2 - 4) * math.sin(ang)
            stalls += building(f'MarketStall_{stalls:02d}', 'market_stall', sx, sy, 0, 'House', 'Places', require=False)
    elif kind in ('fields', 'yard'):
        w, d = place['size']
        box(tag, x, y, w, d, 0.1, 'Field' if kind == 'fields' else 'Plot', collide=False, folder='Places')
    elif kind == 'ruins':
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
    box(f'ConstructionPlot_{i}', x, y, size, size, 0.05, 'Plot', collide=False, folder='Plots')

assert not blocked, f'Blocked places (move them in the layout): {blocked}'

# District fill (seeded, so rebuilds are stable).
rng = random.Random(L['seed'])
fill_report = {}
for name, dist in L['districts'].items():
    cx, cy = dist['centre']; kind = dist['fill']; w, d, _ = TYPES[kind]; made = 0
    for attempt in range(dist['count'] * 80):
        if made == dist['count']:
            break
        ang, rad = rng.uniform(0, 2 * math.pi), dist['radius'] * math.sqrt(rng.random())
        x, y = cx + rad * math.cos(ang), cy + rad * math.sin(ang)
        if inside((x, y)) != dist['inside_wall']:
            continue
        yaw = round(math.degrees(math.atan2(cy - y, cx - x)) / 15) * 15
        made += building(f'{name.replace(" ", "")}_{kind}_{made:02d}', kind, x, y, yaw, 'House', f'Districts/{name}', require=False)
    fill_report[name] = f"{made}/{dist['count']}"
    label(name, cx, cy, 30, 10)

# Arrival.
ax, ay = L['arrival']['at']; fx, fy = L['arrival']['face']
yaw = math.degrees(math.atan2(fy - ay, fx - ax))
actors.spawn_actor_from_class(u.PlayerStart, u.Vector(ax * 100, ay * 100, 120), u.Rotator(roll=0, pitch=0, yaw=yaw)).set_actor_label('Arrival_PlayerStart')
box('Arrival_Waystone', ax + 7, ay - 9, 1.5, 1.5, 3, 'Civic', folder='Places')  # roadside, ahead and to the right
label('Arrival', ax, ay, 8, 3)

# Aerial review camera, looking north-west over the town from beyond the South Gate.
cam = actors.spawn_actor_from_class(u.CameraActor, u.Vector(30000, 90000, 60000), u.Rotator(roll=0, pitch=-38, yaw=-105))
cam.set_actor_label('ReviewCamera_Aerial')
cam.camera_component.set_editor_property('field_of_view', 70)

assert len(bridges) >= 3, f'Expected at least 3 bridges, built {len(bridges)}'
assert u.EditorLoadingAndSavingUtils.save_map(world, ZONE), 'Could not save the zone map'
u.log(f'SILVERBROOK_BUILT Map={ZONE} Bridges={len(bridges)} Fill={fill_report} Counts={counts}')
