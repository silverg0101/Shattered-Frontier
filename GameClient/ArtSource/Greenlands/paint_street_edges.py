"""Repaint the open map's terrain vertex colours so streets read as cobble with packed earth at the margins, grass only beyond.

python tools/Run-InEditor.py GameClient/ArtSource/Greenlands/paint_street_edges.py   (open Greenlands_Silverbrook)
Same fields as build_silverbrook.py (ROAD_G cobble +STREET_COBBLE_EXTRA_M, ROAD_B earth +STREET_EARTH_EXTRA_M, plazas cobble plus a 10 m earth band, ramp 1 grid step),
so a rebuild gives the identical paint; this just avoids a full rebuild. Idempotent (G and B only rise; earth is painted under the cobble as well as outside it).
"""
import json, math, sys
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / 'Silverbrook'))
import silverbrook_props as sp  # _h (garden side hash) and _base_box (wall base), same as dress_house_details
COBBLE_EXTRA, EARTH_EXTRA = 2.0, 8.0
TERRAIN = '/Game/Frontier/Environment/Greenlands/Terrain/SM_Silverbrook_Terrain'
L = json.loads((ROOT / 'silverbrook-town-layout.json').read_text(encoding='utf-8'))
sm = u.load_asset(TERRAIN)
mesh = u.DynamicMesh()
mesh, outcome = u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(sm, mesh, u.GeometryScriptCopyMeshFromAssetOptions(), u.GeometryScriptMeshReadLOD())
assert outcome == u.GeometryScriptOutcomePins.SUCCESS, outcome
pos = u.GeometryScript_List.convert_vector_list_to_array(u.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False)[1])
clist = u.GeometryScript_VertexColors.get_mesh_per_vertex_colors(mesh)[1]; has = True
cols = list(u.GeometryScript_List.convert_color_list_to_array(clist))
assert has and len(cols) == len(pos), (has, len(cols), len(pos))
xs = sorted({round(p.x) for p in pos[:3000]})
step = min(b - a for a, b in zip(xs, xs[1:]) if b > a) / 100.0
cell = {}
for i, p in enumerate(pos):
    cell.setdefault((round(p.x / 100 / step), round(p.y / 100 / step)), []).append(i)
ramp = step  # field is 1 to the edge, then falls to 0 over one grid step (WP6d review fix 2)

def field(extra):
    best = {}
    for tier in ('primary', 'secondary'):
        hw = L['roads'][tier + '_width'] / 2 + extra
        for name, pts in L['roads'][tier].items():
            for a, b in zip(pts, pts[1:]):
                n = max(1, math.ceil(math.dist(a, b)))
                for s in range(n + 1):
                    x, y = a[0] + (b[0] - a[0]) * s / n, a[1] + (b[1] - a[1]) * s / n
                    r = hw + ramp
                    for ci in range(math.floor((x - r) / step), math.ceil((x + r) / step) + 1):
                        for cj in range(math.floor((y - r) / step), math.ceil((y + r) / step) + 1):
                            d = math.hypot(ci * step - x, cj * step - y)
                            if d <= r:
                                f = min(1.0, max(0.0, 1.0 - (d - hw) / ramp))
                                if f > best.get((ci, cj), 0.0):
                                    best[(ci, cj)] = f
    return best

g, b = field(COBBLE_EXTRA), field(EARTH_EXTRA)
PLAZA_EARTH = 10.0  # same as build_silverbrook.PLAZA_EARTH_M
def rect(at, size, grow, out):
    (x, y), (w, d) = at, size
    for ci in range(math.floor((x - w / 2 - grow - ramp) / step), math.ceil((x + w / 2 + grow + ramp) / step) + 1):
        for cj in range(math.floor((y - d / 2 - grow - ramp) / step), math.ceil((y + d / 2 + grow + ramp) / step) + 1):
            dx, dy = abs(ci * step - x) - w / 2 - grow, abs(cj * step - y) - d / 2 - grow
            f = min(1.0, max(0.0, 1.0 - math.hypot(max(dx, 0), max(dy, 0)) / ramp))
            if f > out.get((ci, cj), 0.0):
                out[(ci, cj)] = f
for at, size in [(p['at'], p['size']) for p in L['places'] if p['type'] == 'plaza']:
    rect(at, size, 0.0, g); rect(at, size, PLAZA_EARTH, b)

# Door paths (WP6b review fix 5, widened by WP6c fix 8): B = 1 in a 2.4 m strip from each house's top stoop step to the nearest street centre line.
SEGS = [(a, c) for tier in ('primary', 'secondary') for pts in L['roads'][tier].values() for a, c in zip(pts, pts[1:])]
def nearest(p, a, c):
    dx, dy = c[0] - a[0], c[1] - a[1]
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / ((dx * dx + dy * dy) or 1)))
    return a[0] + dx * t, a[1] + dy * t
def strip(p, q, half=max(1.2, step / 2 + 0.05)):  # one continuous vertex column on the terrain grid
    for ci in range(math.floor((min(p[0], q[0]) - half) / step), math.ceil((max(p[0], q[0]) + half) / step) + 1):
        for cj in range(math.floor((min(p[1], q[1]) - half) / step), math.ceil((max(p[1], q[1]) + half) / step) + 1):
            if math.dist((ci * step, cj * step), nearest((ci * step, cj * step), p, q)) <= half:
                b[(ci, cj)] = 1.0
paths = lanes = 0
BAND = sp.val(sp.cfg()['footing']['base_band_cm'])
for act in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if act.get_actor_label().endswith('_Stoop0'):
        loc = act.get_actor_location(); p = (loc.x / 100, loc.y / 100)
        q = min((nearest(p, a, c) for a, c in SEGS), key=lambda q: math.dist(p, q))
        strip(p, q); paths += 1
        continue
    # Lot-edge lane (WP6c fix 8): along the side opposite the house's garden, from 4 m in front to 3 m behind the house.
    house = act.static_mesh_component.static_mesh if isinstance(act, u.StaticMeshActor) else None
    if not house or not str(act.get_folder_path()).startswith("Districts/") or not house.get_name().startswith("SM_Silverbrook") or "_Door" in house.get_name():
        continue
    label = act.get_actor_label(); x0, x1, y0, y1 = sp._base_box(house, BAND)
    gside = (-1 if sp._h(label + 'side') < .5 else 1) if sp._h(label) < .75 else -1  # no garden: lane on the +X side
    lx = x0 - 120 if gside > 0 else x1 + 120
    loc = act.get_actor_location(); yaw = math.radians(act.get_actor_rotation().yaw); co, si = math.cos(yaw), math.sin(yaw)
    pt = lambda ax, ay: ((loc.x + co * ax - si * ay) / 100, (loc.y + si * ax + co * ay) / 100)
    strip(pt(lx, y1 + 400), pt(lx, y0 - 300)); lanes += 1
changed = 0
for key in set(g) | set(b):
    for i in cell.get(key, []):
        c = cols[i]
        flat = 1.0 - c.r   # R = rockiness: no cobble or earth paint on banks (WP6h review, same as build_silverbrook.road_field)
        ng = max(c.g, g.get(key, 0.0) * flat); nb = max(c.b, b.get(key, 0.0) * flat)  # earth under the cobble too (M_GN_Ground blends cobble over earth)
        if ng != c.g or nb != c.b:
            cols[i] = u.LinearColor(c.r, ng, nb, c.a); changed += 1
mesh = u.GeometryScript_VertexColors.set_mesh_per_vertex_colors(mesh, u.GeometryScript_List.convert_array_to_color_list(cols))
opts = u.GeometryScriptCopyMeshToAssetOptions()
opts.set_editor_property('enable_recompute_normals', False)
opts.set_editor_property('replace_materials', False)
_, outcome = u.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, sm, opts, u.GeometryScriptMeshWriteLOD())
assert outcome == u.GeometryScriptOutcomePins.SUCCESS, outcome
assert u.EditorAssetLibrary.save_loaded_asset(sm)
print('STREET_EDGES_PAINTED vertices', changed, 'step', step, 'door_paths', paths, 'lot_lanes', lanes)
