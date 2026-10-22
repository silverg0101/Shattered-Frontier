"""Rebuild SM_MightyOak_LowerTrunk as a smooth, root-flared closed trunk (felling review r1 fix 6).

The prepared lower trunk is the source's 8-sided 0-1 m chunk (88 triangles): its stump reads faceted. This loft samples
that chunk's outer surface by ray casts, low-pass smooths each ring, flares five root buttresses over the bottom 0.25 m,
buries 0.2 m below ground (no gap under the downhill side on a 15 deg slope) and returns to the exact sampled section at
the 1.0 m join so the upper structure still meets it. Bark slot 0 (cylindrical UVs at the source texel density),
cut-wood slot 1 on both caps. Writes SmoothLowerTrunk/SM_MightyOak_LowerTrunk.fbx and build.json.

& 'C:/Program Files/Blender Foundation/Blender 5.1/blender.exe' --background --factory-startup --python GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/tools/Build-SmoothLowerTrunk.py
Import: py -3 tools/Run-InEditor.py tools/Update-HarvestOakFellingArt.py
"""
import json
import math
import os

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, '..', '..', 'MightyOak-v1', 'exports', 'MightyOak_v1.blend')
OUT = os.path.join(HERE, '..', 'SmoothLowerTrunk')
SEGMENTS, RINGS = 24, 10                 # around, along 0-1 m
BURY_M, FLARE_M, FLARE_GAIN, ROOTS = 0.20, 0.25, 0.35, 5
JOIN_BLEND_M = 0.20                      # top band returning to the raw sampled section

bpy.ops.wm.open_mainfile(filepath=os.path.abspath(SOURCE))
src = bpy.data.objects['SM_MightyOak_LowerTrunk']
mesh = src.data
world = [src.matrix_world @ v.co for v in mesh.vertices]
tree = BVHTree.FromPolygons(world, [tuple(p.vertices) for p in mesh.polygons])
upper_obj = bpy.data.objects['SM_MightyOak_UpperStructure']
upper_world = [upper_obj.matrix_world @ v.co for v in upper_obj.data.vertices]
upper_tree = BVHTree.FromPolygons(upper_world, [tuple(p.vertices) for p in upper_obj.data.polygons])
xs, ys = [v.x for v in world], [v.y for v in world]
centre = Vector(((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, 0))
top = max(v.z for v in world)

# Source bark texel density (UV units per metre), so the bark tiles as before.
uv = mesh.uv_layers.active.data
area3d = areauv = 0.0
for p in mesh.polygons:
    if p.material_index != 0 or len(p.loop_indices) != 3:
        continue
    a, b, c = (uv[i].uv for i in p.loop_indices)
    areauv += abs((b - a).cross(c - a)) / 2
    area3d += p.area
density = math.sqrt(areauv / area3d) if area3d else 1.0


def radius(z, theta, bvh=None):
    d = Vector((math.cos(theta), math.sin(theta), 0))
    hit = (bvh or tree).ray_cast(centre + Vector((0, 0, z)) + d * 3.0, -d)
    return (hit[0] - (centre + Vector((0, 0, z)))).length if hit[0] else None


def ring(z):
    zs = min(max(z, 0.02), top - 0.002)
    # The join ring is the upper structure's own base section, so the log has no step at 1 m.
    raw = [radius(top + 0.003, 2 * math.pi * k / SEGMENTS, upper_tree) if z >= top - 1e-6 else radius(zs, 2 * math.pi * k / SEGMENTS)
           for k in range(SEGMENTS)]
    assert all(raw), ('ray miss', z)
    w = SEGMENTS // 8                    # one source facet span
    smooth = [sum(raw[(k + j) % SEGMENTS] for j in range(-w, w + 1)) / (2 * w + 1) for k in range(SEGMENTS)]
    t = 1.0 if z >= top - 0.05 - 1e-6 else min(1.0, max(0.0, (z - (top - JOIN_BLEND_M)) / JOIN_BLEND_M))  # raw last two rings
    out = []
    for k in range(SEGMENTS):
        r = smooth[k] * (1 - t) + raw[k] * t
        if z < FLARE_M:
            theta = 2 * math.pi * k / SEGMENTS
            root = max(0.0, math.cos(ROOTS * theta + 0.7)) ** 1.5
            r *= 1 + FLARE_GAIN * ((FLARE_M - max(z, 0)) / FLARE_M) ** 2 * (0.45 + 0.55 * root)
        out.append(r)
    return out


# Six of the rings in the bottom 0.3 m round the flare; the last two are the raw join section.
heights = [-BURY_M, -0.1, 0, 0.05, 0.1, 0.17, 0.25, 0.35, 0.5, 0.7, top - 0.05, top]
bm = bmesh.new()
uvl = bm.loops.layers.uv.new('UVMap')
rings = []
for z in heights:
    rs = ring(z)
    rings.append([bm.verts.new(centre + Vector((math.cos(2 * math.pi * k / SEGMENTS) * rs[k],
                                                 math.sin(2 * math.pi * k / SEGMENTS) * rs[k], z)))
                  for k in range(SEGMENTS)])
circ = 2 * math.pi * sum(ring(0.5)) / SEGMENTS
# r4 fix 3: one affine fit of the upper structure's bark UVs near the join (U = a*angle + b, V = c*z + d), applied to
# every ring, so the pattern continues across 1 m without the per-column copy that striped the bark.
pts = []
for poly in upper_obj.data.polygons:
    if poly.material_index != 0:
        continue
    for li in poly.loop_indices:
        co = upper_world[upper_obj.data.loops[li].vertex_index]
        if co.z <= top + 0.25:
            uvv = upper_obj.data.uv_layers.active.data[li].uv
            pts.append(((math.atan2(co.y - centre.y, co.x - centre.x)) % (2 * math.pi), co.z, uvv.x, uvv.y))


def fit(xs, ys):
    n = len(xs); mx = sum(xs) / n; my = sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs) or 1e-9
    a = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx
    return a, my - a * mx, (sum((y - (a * x + my - a * mx)) ** 2 for x, y in zip(xs, ys)) / n) ** 0.5


ring_pts = [p for p in pts if p[1] <= top + 0.002]
ua, ub, ures = fit([p[0] for p in ring_pts], [p[2] for p in ring_pts])
vc, vd, vres = fit([p[1] for p in pts], [p[3] for p in pts])
UV_FIT = {'u_per_rad': ua, 'u0': ub, 'u_rms': ures, 'v_per_m': vc, 'v0': vd, 'v_rms': vres}
if ures > 0.02 or abs(ua) < 1e-4:
    ua, ub = circ * density / (2 * math.pi), 0.0      # upper U is not affine in angle: keep cylindrical U, match V only
if vres > 0.05 or abs(vc) < 1e-4:
    vc, vd = density, 0.0
for i in range(len(rings) - 1):
    for k in range(SEGMENTS):
        k2 = (k + 1) % SEGMENTS
        for tri in ((rings[i][k], rings[i][k2], rings[i + 1][k2]), (rings[i][k], rings[i + 1][k2], rings[i + 1][k])):
            f = bm.faces.new(tri)
            f.material_index = 0; f.smooth = True
            for loop in f.loops:
                j = rings[i].index(loop.vert) if loop.vert in rings[i] else rings[i + 1].index(loop.vert)
                if j == 0 and (k == SEGMENTS - 1):
                    j = SEGMENTS
                # The upper base ring's own UVs (r3 fix 5) mapped to a 24-column loft stretched the bark into stripes;
                # keep cylindrical UVs at the source texel density (a texture break at the join remains).
                loop[uvl].uv = (ua * (2 * math.pi * j / SEGMENTS) + ub, vc * loop.vert.co.z + vd)
for verts, up in ((rings[0], False), (rings[-1], True)):
    c = bm.verts.new(sum((v.co for v in verts), Vector()) / SEGMENTS)
    for k in range(SEGMENTS):
        a, b = verts[k], verts[(k + 1) % SEGMENTS]
        f = bm.faces.new((c, a, b) if up else (c, b, a))
        f.material_index = 0 if up else 1; f.smooth = False    # top cap is hidden inside the join: bark, so no cut-wood rim
        for loop in f.loops:
            loop[uvl].uv = (loop.vert.co.x * 2 + 0.5, loop.vert.co.y * 2 + 0.5)
bm.normal_update()
assert all(e.is_manifold for e in bm.edges), 'not closed'
for f in bm.faces:                        # outward normals
    if f.smooth and f.normal.dot(f.calc_center_median() - Vector((centre.x, centre.y, f.calc_center_median().z))) < 0:
        raise RuntimeError('inward side face')
new = bpy.data.meshes.new('SM_MightyOak_LowerTrunk_Smooth')
bm.to_mesh(new)
for m in mesh.materials:
    new.materials.append(m)
for o in list(bpy.data.objects):
    bpy.data.objects.remove(o)
obj = bpy.data.objects.new('SM_MightyOak_LowerTrunk', new)
bpy.context.scene.collection.objects.link(obj)
obj.select_set(True); bpy.context.view_layer.objects.active = obj
os.makedirs(OUT, exist_ok=True)
fbx = os.path.join(OUT, 'SM_MightyOak_LowerTrunk.fbx')
bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                         apply_scale_options='FBX_SCALE_UNITS', bake_space_transform=False, add_leaf_bones=False,
                         mesh_smooth_type='FACE')
report = {'uv_fit': UV_FIT, 'triangles': len(bm.faces), 'segments': SEGMENTS, 'rings': len(heights), 'bury_m': BURY_M,
          'flare': [FLARE_M, FLARE_GAIN, ROOTS], 'top_m': top, 'bark_uv_per_m': density,
          'bounds_m': [[min(v.co[i] for v in bm.verts) for i in range(3)], [max(v.co[i] for v in bm.verts) for i in range(3)]]}
with open(os.path.join(OUT, 'build.json'), 'w') as f:
    json.dump(report, f, indent=2)
print('SMOOTH_LOWER_TRUNK_READY', json.dumps(report))
