"""Repaint the open map's terrain vertex colours so streets read as cobble with packed earth at the margins, grass only beyond.

python tools/Run-InEditor.py GameClient/ArtSource/Greenlands/paint_street_edges.py   (open Greenlands_Silverbrook)
Same fields as build_silverbrook.py (ROAD_G cobble +STREET_COBBLE_EXTRA_M, ROAD_B earth +STREET_EARTH_EXTRA_M, ramp 2 grid steps),
so a rebuild gives the identical paint; this just avoids a full rebuild. Idempotent (G only rises; B in the street margin is recomputed, so a trail's own B inside it is replaced).
"""
import json, math
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parent
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
ramp = 2 * step

def field(extra):
    best = {}
    for tier in ('primary', 'secondary'):
        hw = L['roads'][tier + '_width'] / 2 + extra
        for name, pts in L['roads'][tier].items():
            for a, b in zip(pts, pts[1:]):
                n = max(1, math.ceil(math.dist(a, b)))
                for s in range(n + 1):
                    x, y = a[0] + (b[0] - a[0]) * s / n, a[1] + (b[1] - a[1]) * s / n
                    r = hw + ramp / 2
                    for ci in range(math.floor((x - r) / step), math.ceil((x + r) / step) + 1):
                        for cj in range(math.floor((y - r) / step), math.ceil((y + r) / step) + 1):
                            d = math.hypot(ci * step - x, cj * step - y)
                            if d <= r:
                                f = min(1.0, max(0.0, 0.5 + (hw - d) / ramp))
                                if f > best.get((ci, cj), 0.0):
                                    best[(ci, cj)] = f
    return best

g, b = field(COBBLE_EXTRA), field(EARTH_EXTRA)

# Door paths (WP6b review fix 5): B = 1 in a 1.2 m strip from each house's top stoop step to the nearest street centre line.
SEGS = [(a, c) for tier in ('primary', 'secondary') for pts in L['roads'][tier].values() for a, c in zip(pts, pts[1:])]
def nearest(p, a, c):
    dx, dy = c[0] - a[0], c[1] - a[1]
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / ((dx * dx + dy * dy) or 1)))
    return a[0] + dx * t, a[1] + dy * t
paths = 0
for act in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if not act.get_actor_label().endswith('_Stoop0'):
        continue
    loc = act.get_actor_location(); p = (loc.x / 100, loc.y / 100)
    q = min((nearest(p, a, c) for a, c in SEGS), key=lambda q: math.dist(p, q))
    for ci in range(math.floor((min(p[0], q[0]) - 1) / step), math.ceil((max(p[0], q[0]) + 1) / step) + 1):
        for cj in range(math.floor((min(p[1], q[1]) - 1) / step), math.ceil((max(p[1], q[1]) + 1) / step) + 1):
            if math.dist((ci * step, cj * step), nearest((ci * step, cj * step), p, q)) <= 0.6:
                b[(ci, cj)] = 1.0
    paths += 1
changed = 0
for key in set(g) | set(b):
    for i in cell.get(key, []):
        c = cols[i]
        ng = max(c.g, g.get(key, 0.0)); nb = b.get(key, 0.0) * (1.0 - ng)  # earth only outside the cobble; replaces B in the street margin
        if ng != c.g or nb != c.b:
            cols[i] = u.LinearColor(c.r, ng, nb, c.a); changed += 1
mesh = u.GeometryScript_VertexColors.set_mesh_per_vertex_colors(mesh, u.GeometryScript_List.convert_array_to_color_list(cols))
opts = u.GeometryScriptCopyMeshToAssetOptions()
opts.set_editor_property('enable_recompute_normals', False)
opts.set_editor_property('replace_materials', False)
_, outcome = u.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, sm, opts, u.GeometryScriptMeshWriteLOD())
assert outcome == u.GeometryScriptOutcomePins.SUCCESS, outcome
assert u.EditorAssetLibrary.save_loaded_asset(sm)
print('STREET_EDGES_PAINTED vertices', changed, 'step', step, 'door_paths', paths)
