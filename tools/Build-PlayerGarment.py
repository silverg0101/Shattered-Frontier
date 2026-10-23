"""Build the starter garment (linen tunic + dark leggings) for a MetaHuman player body, skinned to the body's own skeleton.

Blender 5.1, headless:
  "C:\\Program Files\\Blender Foundation\\Blender 5.1\\blender.exe" --factory-startup --background --python tools/Build-PlayerGarment.py -- \
      --body Artifacts/Characters/PlayerBase/OrlandoBare_body.fbx --out Artifacts/Characters/PlayerBase/garment_tunic_leggings.fbx \
      --render GameClient/ArtSource/Characters/Player/garments/tunic-leggings-blender.png
Input: the FBX that tools/Export-PlayerBaseBody.py writes from a "bare" build (tools/Build-PlayerMetaHumans.py, faces.json "bare": true).
A normal build omits the torso and thighs its default clothing hides, so it cannot be fitted. Rerun with the female body's FBX for her fit.
Output: <out>.fbx (empty + armature "root" + ONE skinned mesh "SKM_Tunic_Leggings") and <out>.blend (adds the body, for review).

How it is built (all lengths cm, in the body's own frame):
- Leggings (material slot 1, "M_Leggings"): the body surface from the ankle to the hips, arms removed, pushed out along normals
  (--leggings-offset) and cut cleanly with bisect planes.
- Tunic (slot 0, "M_Tunic"): the body surface from the hips to the neck opening and to --sleeve-t of the upper arm, pushed out
  (--tunic-offset, extra on the sleeves), smoothed so it hangs looser than the muscles, then kept --clearance off the skin.
  The sleeve ends get a raised band (rolled cuff). A V-split is cut into the front of the neck opening. Below the hips a skirt is lofted
  from the hip border down to --hem (fraction of the thigh) as ONE tube around both thighs: each ring is the convex hull of the body's
  cross-section plus ease and flare. Two side slits are left open.
- Weights: nearest body triangle, barycentric, for every vertex; skirt weights are then smoothed so the tube does not tear between
  the legs; max 8 influences, normalised.
- UVs: one box projection, 1 UV unit per 40 cm, for a tiling cloth material. Seams fall where the dominant normal axis changes.
Offset shells follow the body's detail, so pass a larger --tunic-offset for heavier builds.
"""
import argparse
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

ARM_KEYS = ('upperarm', 'lowerarm', 'hand', 'wrist', 'pinky', 'ring_', 'thumb', 'index_', 'middle_')


def parse():
    p = argparse.ArgumentParser()
    p.add_argument('--body', required=True)
    p.add_argument('--out', required=True)
    p.add_argument('--render', default='')
    p.add_argument('--tunic-offset', type=float, default=1.2)
    p.add_argument('--sleeve-extra', type=float, default=0.8)
    p.add_argument('--leggings-offset', type=float, default=0.6)
    p.add_argument('--clearance', type=float, default=0.9, help='minimum tunic distance from the skin')
    p.add_argument('--sleeve-t', type=float, default=0.80, help='sleeve end along shoulder to elbow')
    p.add_argument('--cuff-width', type=float, default=3.5)
    p.add_argument('--cuff-bulge', type=float, default=1.3)
    p.add_argument('--hem', type=float, default=0.5, help='hem as a fraction of the thigh length down from the hip')
    p.add_argument('--skirt-ease', type=float, default=2.5)
    p.add_argument('--skirt-flare', type=float, default=3.5)
    p.add_argument('--slit-height', type=float, default=16.0)
    p.add_argument('--v-depth', type=float, default=10.0)
    p.add_argument('--v-half-angle', type=float, default=18.0)
    p.add_argument('--ankle-above-foot', type=float, default=2.5)
    p.add_argument('--uv-cm', type=float, default=40.0)
    return p.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])


def import_body(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.abspath(path))
    body = next(o for o in bpy.data.objects if o.type == 'MESH')
    arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    return body, arm


def body_weights(body):
    names = {g.index: g.name for g in body.vertex_groups}
    return [{names[g.group]: g.weight for g in v.groups if g.weight > 1e-4} for v in body.data.vertices]


def arm_amount(w):
    return min(1.0, sum(x for n, x in w.items() if any(k in n for k in ARM_KEYS)))


def bary(p, a, b, c):
    v0, v1, v2 = b - a, c - a, p - a
    d00, d01, d11, d20, d21 = v0.dot(v0), v0.dot(v1), v1.dot(v1), v2.dot(v0), v2.dot(v1)
    den = d00 * d11 - d01 * d01
    if abs(den) < 1e-12:
        return 1.0, 0.0, 0.0
    v = (d11 * d20 - d01 * d21) / den
    w = (d00 * d21 - d01 * d20) / den
    return 1 - v - w, v, w


class Source:
    """The untouched body: nearest-surface queries and skin weights."""

    def __init__(self, body):
        # The FBX import splits vertices along UV seams and hard edges; weld them or offset shells would crack open there.
        self.bm = bmesh.new()
        self.bm.from_mesh(body.data)
        self.bm.verts.ensure_lookup_table()
        orig = self.bm.verts.layers.int.new('orig')
        for v in self.bm.verts:
            v[orig] = v.index
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts[:], dist=1e-4)
        self.bm.verts.ensure_lookup_table()
        self.bm.verts.index_update()
        all_w = body_weights(body)
        self.weights = [all_w[v[orig]] for v in self.bm.verts]
        self.arm = [arm_amount(w) for w in self.weights]
        tri = self.bm.copy()
        bmesh.ops.triangulate(tri, faces=tri.faces[:])
        tri.faces.ensure_lookup_table()
        self.tri = tri
        self.bvh = BVHTree.FromBMesh(tri)

    def nearest(self, p):
        return self.bvh.find_nearest(p)  # (location, normal, face index, distance)

    def weights_at(self, p):
        loc, _n, idx, _d = self.bvh.find_nearest(p)
        f = self.tri.faces[idx]
        vs = [l.vert for l in f.loops]
        lam = bary(loc, vs[0].co, vs[1].co, vs[2].co)
        out = {}
        for v, l in zip(vs, lam):
            for n, x in self.weights[v.index].items():
                out[n] = out.get(n, 0.0) + x * l
        return out


def shell(src, offset, extra_arm=0.0, smooth=0, clearance=0.0):
    """A copy of the body pushed out along normals, optionally smoothed and kept clear of the skin."""
    bm = src.bm.copy()
    bm.normal_update()
    for v in bm.verts:
        v.co += v.normal * (offset + extra_arm * src.arm[v.index])
    for _ in range(smooth):
        bmesh.ops.smooth_vert(bm, verts=bm.verts[:], factor=0.5, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    if clearance:
        for v in bm.verts:
            loc, nor, _i, dist = src.nearest(v.co)
            if dist < clearance:
                d = v.co - loc
                v.co = loc + (d.normalized() if d.length > 1e-6 else nor) * clearance
    bm.verts.ensure_lookup_table()
    return bm


def cut(bm, co, no, keep_negative=True):
    """Bisect by the plane and drop the side the normal points to (or the opposite one)."""
    geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
    bmesh.ops.bisect_plane(bm, geom=geom, plane_co=co, plane_no=no, clear_outer=keep_negative, clear_inner=not keep_negative)
    bm.verts.ensure_lookup_table()
    bm.verts.index_update()


def cut_arm(bm, src, a, d, dist):
    """Cut an arm across at `dist` along d from a: only arm geometry is split and the far part dropped (the torso lies beyond that plane too)."""
    bm.verts.index_update()
    armv = vert_arm(src, bm)
    inarm = {v for v in bm.verts if armv[v.index] > 0.3 and v.co.x * a.x > 0}
    geom = list(inarm) + [e for e in bm.edges if e.verts[0] in inarm and e.verts[1] in inarm] + \
        [f for f in bm.faces if all(v in inarm for v in f.verts)]
    bmesh.ops.bisect_plane(bm, geom=geom, plane_co=a + d * dist, plane_no=d)
    bm.verts.ensure_lookup_table()
    bm.verts.index_update()
    armv = vert_arm(src, bm)
    drop(bm, [v for v in bm.verts if (v.co - a).dot(d) > dist + 1e-3 and armv[v.index] > 0.3 and v.co.x * a.x > 0])


def drop(bm, verts):
    bmesh.ops.delete(bm, geom=list(verts), context='VERTS')
    bm.verts.ensure_lookup_table()
    bm.verts.index_update()


def keep_largest(bm):
    bm.verts.index_update()
    seen, best = set(), []
    for v in bm.verts:
        if v.index in seen:
            continue
        comp, stack = [], [v]
        seen.add(v.index)
        while stack:
            c = stack.pop()
            comp.append(c)
            for e in c.link_edges:
                o = e.other_vert(c)
                if o.index not in seen:
                    seen.add(o.index)
                    stack.append(o)
        if len(comp) > len(best):
            best = comp
    keep = {v.index for v in best}
    drop(bm, [v for v in bm.verts if v.index not in keep])


def vert_arm(src, bm):
    """Arm amount per vertex of a shell (vertex order equals the body's until cuts: recomputed from the nearest skin)."""
    out = {}
    for v in bm.verts:
        loc, _n, idx, _d = src.nearest(v.co)
        f = src.tri.faces[idx]
        out[v.index] = max(src.arm[l.vert.index] for l in f.loops)
    return out


def hull(points):
    pts = sorted(set((round(x, 3), round(y, 3)) for x, y in points))
    if len(pts) < 3:
        return pts

    def half(seq):
        h = []
        for p in seq:
            while len(h) >= 2 and (h[-1][0] - h[-2][0]) * (p[1] - h[-2][1]) - (h[-1][1] - h[-2][1]) * (p[0] - h[-2][0]) <= 0:
                h.pop()
            h.append(p)
        return h
    lo, up = half(pts), half(pts[::-1])
    return lo[:-1] + up[:-1]


def ray_hull(poly, c, theta):
    dx, dy = math.cos(theta), math.sin(theta)
    best = 0.0
    for i in range(len(poly)):
        ax, ay = poly[i][0] - c[0], poly[i][1] - c[1]
        bx, by = poly[(i + 1) % len(poly)][0] - c[0], poly[(i + 1) % len(poly)][1] - c[1]
        ex, ey = bx - ax, by - ay
        den = dx * ey - dy * ex
        if abs(den) < 1e-9:
            continue
        t = (ax * ey - ay * ex) / den
        s = (ax * dy - ay * dx) / den
        if t > 0 and -1e-6 <= s <= 1 + 1e-6:
            best = max(best, t)
    return best


def loft_skirt(bm, src, args, z_hem):
    """Rings below the hip border: hull of the body cross-section + ease + flare. Returns the new verts per row and slit columns."""
    border = [v for v in bm.verts if v.is_boundary and v.co.z < args.zjoin + 4]
    cx = sum(v.co.x for v in border) / len(border)
    cy = sum(v.co.y for v in border) / len(border)
    border.sort(key=lambda v: math.atan2(v.co.y - cy, v.co.x - cx))
    n = len(border)
    th = [math.atan2(v.co.y - cy, v.co.x - cx) for v in border]
    r0 = [math.hypot(v.co.x - cx, v.co.y - cy) for v in border]
    z0 = sum(v.co.z for v in border) / n
    rows = max(4, int(round((z0 - z_hem) / 2.6)))
    arm_verts = {v.index for v in src.bm.verts if src.arm[v.index] > 0.3}
    targets = []
    for k in range(1, rows + 1):
        z = z0 - (z0 - z_hem) * k / rows
        pts = [(v.co.x, v.co.y) for v in src.bm.verts if abs(v.co.z - z) < 3.0 and v.index not in arm_verts]
        poly = hull(pts)
        t = k / rows
        margin = args.skirt_ease + args.skirt_flare * t
        targets.append([ray_hull(poly, (cx, cy), a) + margin for a in th])
    radii = [r0] + targets
    radii[1] = [0.3 * a + 0.7 * b for a, b in zip(r0, targets[0])]
    for _ in range(8):  # smooth around the ring, then down the rows
        for row in radii[1:]:
            row[:] = [(row[i - 1] + 2 * row[i] + row[(i + 1) % n]) / 4 for i in range(n)]
    for k in range(1, rows):
        radii[k] = [(radii[k - 1][i] * 0.25 + radii[k][i] * 0.5 + radii[k + 1][i] * 0.25) for i in range(n)]
    ring = [border]
    for k in range(1, rows + 1):
        z = z0 - (z0 - z_hem) * k / rows
        row = []
        for i in range(n):
            p = Vector((cx + radii[k][i] * math.cos(th[i]), cy + radii[k][i] * math.sin(th[i]), z))
            loc, nor, _i, dist = src.nearest(p)
            if dist < args.clearance + 1.0:  # a fold of cloth never sits in the skin
                d = p - loc
                p = loc + (d.normalized() if d.length > 1e-6 else nor) * (args.clearance + 1.0)
            row.append(bm.verts.new(p))
        ring.append(row)
    right = min(range(n), key=lambda i: abs(th[i]))
    left = min(range(n), key=lambda i: abs(abs(th[i]) - math.pi))
    slit_cols = {right, left}
    for k in range(rows):
        zmid = z0 - (z0 - z_hem) * (k + 0.5) / rows
        for i in range(n):
            if i in slit_cols and zmid < z_hem + args.slit_height:
                continue
            j = (i + 1) % n
            bm.faces.new((ring[k][i], ring[k + 1][i], ring[k + 1][j], ring[k][j]))
    return [v for row in ring[1:] for v in row]


def add_v_split(bm, args):
    border = [v for v in bm.verts if v.is_boundary and v.co.z > 140]
    cx = sum(v.co.x for v in border) / len(border)
    cy = sum(v.co.y for v in border) / len(border)
    front = [v for v in border if abs(v.co.x - cx) < 4 and v.co.y < cy]
    apex_z = min(v.co.z for v in front) - args.v_depth
    apex_y = min(v.co.y for v in front)
    a = math.radians(args.v_half_angle)
    apex = Vector((cx, apex_y, apex_z))
    n_right = Vector((-math.cos(a), 0, math.sin(a)))  # inside the V is the positive side of both planes
    n_left = Vector((math.cos(a), 0, math.sin(a)))
    # Bisect with the two planes, then delete the faces inside the V on the front half of the neck.
    for nrm in (n_right, n_left):
        geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
        bmesh.ops.bisect_plane(bm, geom=geom, plane_co=apex, plane_no=nrm)
    bm.verts.ensure_lookup_table()
    bm.faces.ensure_lookup_table()
    inside = [f for f in bm.faces
              if (f.calc_center_median() - apex).dot(n_right) > 1e-4 and (f.calc_center_median() - apex).dot(n_left) > 1e-4
              and f.calc_center_median().y < cy - 1.0]
    bmesh.ops.delete(bm, geom=inside, context='FACES_ONLY')
    # Drop the verts left loose, then close the V's point.
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context='VERTS')
    bm.verts.ensure_lookup_table()


def rolled_cuff(bm, a_pt, direction, plane_d, width, bulge, armv):
    """Raise the last `width` cm of a sleeve radially: a rolled band."""
    bm.verts.index_update()
    for v in bm.verts:
        along = (v.co - a_pt).dot(direction)
        if plane_d - width - 0.01 < along <= plane_d + 0.01 and armv[v.index] > 0.3 and (v.co.x - a_pt.x) * direction.x > 0:
            radial = (v.co - a_pt) - direction * along
            if radial.length > 1e-6:
                s = 1.0 if along > plane_d - width * 0.85 else 0.5  # a step at the band's inner edge
                v.co += radial.normalized() * bulge * s


def build(args, src, arm):
    bones = arm.data.bones
    head = lambda n: Vector(bones[n].head_local)
    z_ankle = head('foot_l').z + args.ankle_above_foot
    z_join = head('pelvis').z - 3.0
    args.zjoin = z_join
    z_hem = head('thigh_l').z - args.hem * (head('thigh_l').z - head('calf_l').z)

    # --- leggings: ankle to hip, no arms ---
    legs = shell(src, args.leggings_offset, clearance=0.35)
    cut(legs, Vector((0, 0, z_ankle)), Vector((0, 0, 1)), keep_negative=False)
    cut(legs, Vector((0, 0, head('spine_01').z)), Vector((0, 0, 1)), keep_negative=True)
    armv = vert_arm(src, legs)
    drop(legs, [v for v in legs.verts if armv[v.index] > 0.4])
    keep_largest(legs)

    # --- tunic: hip to neck and upper arms ---
    tun = shell(src, args.tunic_offset, extra_arm=args.sleeve_extra, smooth=4, clearance=args.clearance)
    cut(tun, Vector((0, 0, z_join)), Vector((0, 0, 1)), keep_negative=False)
    cuff = []
    for side in ('l', 'r'):
        a, e = head(f'upperarm_{side}'), head(f'lowerarm_{side}')
        length = (e - a).length
        d = (e - a).normalized()
        cut_arm(tun, src, a, d, length * args.sleeve_t)
        cuff.append((a, d, length * args.sleeve_t))
    keep_largest(tun)
    armv = vert_arm(src, tun)
    for a, d, dist in cuff:
        rolled_cuff(tun, a, d, dist, args.cuff_width, args.cuff_bulge, armv)
    add_v_split(tun, args)
    skirt = loft_skirt(tun, src, args, z_hem)
    tun.verts.index_update()
    skirt_idx = {v.index for v in skirt}
    bmesh.ops.recalc_face_normals(tun, faces=tun.faces[:])

    # --- one bmesh: tunic (slot 0) then leggings (slot 1) ---
    out = bmesh.new()
    skirt_new = set()
    for part, slot in ((tun, 0), (legs, 1)):
        part.verts.index_update()
        vmap = {v.index: out.verts.new(v.co) for v in part.verts}
        for f in part.faces:
            nf = out.faces.new([vmap[v.index] for v in f.verts])
            nf.material_index = slot
            nf.smooth = True
        if slot == 0:
            skirt_new = {vmap[i] for i in skirt_idx if i in vmap}
    out.verts.ensure_lookup_table()
    out.verts.index_update()
    out.faces.ensure_lookup_table()
    skirt_ids = {v.index for v in skirt_new}
    uvl = out.loops.layers.uv.new('UVMap')
    s = 1.0 / args.uv_cm
    out.normal_update()
    for f in out.faces:
        n = [abs(x) for x in f.normal]
        ax = n.index(max(n))
        for l in f.loops:
            c = l.vert.co
            l[uvl].uv = ((c.y, c.z), (c.x, c.z), (c.x, c.y))[ax]
            l[uvl].uv = (l[uvl].uv[0] * s, l[uvl].uv[1] * s)
    return out, skirt_ids


def skin(out, src, skirt_ids):
    W = [src.weights_at(v.co) for v in out.verts]
    for _ in range(14):  # smooth the skirt so it follows both thighs and does not tear between them
        nxt = {}
        for i in skirt_ids:
            v = out.verts[i]
            acc = {}
            nb = [e.other_vert(v).index for e in v.link_edges]
            for j in nb:
                for n, x in W[j].items():
                    acc[n] = acc.get(n, 0.0) + x / len(nb)
            new = {n: 0.5 * W[i].get(n, 0.0) + 0.5 * acc.get(n, 0.0) for n in set(W[i]) | set(acc)}
            nxt[i] = new
        for i, w in nxt.items():
            W[i] = w
    for i, w in enumerate(W):
        top = sorted(w.items(), key=lambda kv: -kv[1])[:8]
        t = sum(x for _n, x in top) or 1.0
        W[i] = {n: x / t for n, x in top if x / t > 1e-3}
    return W


def make_object(out, W, body, arm, name='SKM_Tunic_Leggings'):
    verts = [tuple(v.co) for v in out.verts]
    faces = [[v.index for v in f.verts] for f in out.faces]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    uvl = out.loops.layers.uv.active
    uv = mesh.uv_layers.new(name='UVMap')
    li = 0
    for f in out.faces:
        for l in f.loops:
            uv.data[li].uv = l[uvl].uv
            li += 1
    for p, f in zip(mesh.polygons, out.faces):
        p.material_index = f.material_index
        p.use_smooth = True
    for slot, (mname, col) in enumerate((('M_Tunic', (0.72, 0.62, 0.45, 1)), ('M_Leggings', (0.20, 0.14, 0.10, 1)))):
        m = bpy.data.materials.new(mname)
        m.diffuse_color = col
        mesh.materials.append(m)
    ob = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(ob)
    ob.parent = arm
    ob.matrix_parent_inverse = body.matrix_parent_inverse.copy()
    ob.matrix_basis = body.matrix_basis.copy()
    groups = {g.name: ob.vertex_groups.new(name=g.name) for g in body.vertex_groups}
    for i, w in enumerate(W):
        for n, x in w.items():
            groups[n].add([i], x, 'REPLACE')
    mod = ob.modifiers.new('Armature', 'ARMATURE')
    mod.object = arm
    return ob


def export(args, ob, arm):
    """The skeleton must be the UE one: "root" with no Empty above it (that Empty would become the skeleton root and break merging)."""
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()
    arm.parent = None  # the importer put the FBX unit scale (0.01) on that Empty; the data itself is already cm
    for o in [o for o in bpy.data.objects if o.type == 'EMPTY']:
        bpy.data.objects.remove(o)
    unit = bpy.context.scene.unit_settings
    unit.system, unit.length_unit, unit.scale_length = 'METRIC', 'CENTIMETERS', 0.01
    for o in bpy.data.objects:
        o.select_set(False)
    for o in (arm, ob):
        o.select_set(True)
    out = os.path.abspath(args.out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=out, use_selection=True, object_types={'ARMATURE', 'MESH'},
                             use_armature_deform_only=False, add_leaf_bones=False, bake_anim=False,
                             mesh_smooth_type='FACE', path_mode='STRIP')
    bpy.ops.wm.save_as_mainfile(filepath=os.path.splitext(out)[0] + '.blend')


def pose_stride(arm):
    """Mid-stride: left leg forward, right back, arms swung opposite. Rotations about the bones' heads, in armature space."""
    def rot(name, deg):
        pb = arm.pose.bones[name]
        h = Vector(arm.data.bones[name].head_local)
        r = Matrix.Translation(h) @ Matrix.Rotation(math.radians(deg), 4, 'X') @ Matrix.Translation(-h)
        pb.matrix = r @ pb.matrix
        bpy.context.view_layer.update()
    rot('thigh_l', -28)
    rot('thigh_r', 22)
    rot('calf_r', 18)
    rot('upperarm_l', 18)
    rot('upperarm_r', -18)


def render(args, body, ob, arm):
    import numpy as np
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.render.resolution_x, sc.render.resolution_y = 560, 900
    sc.display.shading.light = 'STUDIO'
    sc.display.shading.color_type = 'MATERIAL'
    sc.display.shading.show_cavity = True
    sc.render.film_transparent = False
    sc.world = bpy.data.worlds.new('w')
    sc.world.color = (0.9, 0.9, 0.9)
    bm = bpy.data.materials.new('skin')
    bm.diffuse_color = (0.55, 0.42, 0.34, 1)
    body.data.materials.clear()
    body.data.materials.append(bm)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = 2.0
    body.hide_render = False
    views = [('front', (0, -5, 0.8), (90, 0, 0), False), ('side', (5, 0, 0.8), (90, 0, 90), False),
             ('back', (0, 5, 0.8), (90, 0, 180), False), ('stride', (5, 0, 0.8), (90, 0, 90), True)]
    tiles = []
    for name, loc, rotd, stride in views:
        if stride:
            pose_stride(arm)
        cam.location = loc
        cam.rotation_euler = [math.radians(a) for a in rotd]
        path = os.path.join(bpy.app.tempdir, f'garment_{name}.png')
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(path)
        tiles.append(np.array(img.pixels[:]).reshape(img.size[1], img.size[0], 4))
    sheet = np.concatenate(tiles, axis=1)
    h, w = sheet.shape[:2]
    out = bpy.data.images.new('sheet', w, h, alpha=True)
    out.pixels = sheet.ravel().tolist()
    out.filepath_raw = os.path.abspath(args.render)
    out.file_format = 'PNG'
    os.makedirs(os.path.dirname(out.filepath_raw), exist_ok=True)
    out.save()


def main():
    args = parse()
    body, arm = import_body(args.body)
    src = Source(body)
    bm, skirt_ids = build(args, src, arm)
    W = skin(bm, src, skirt_ids)
    ob = make_object(bm, W, body, arm)
    print('GARMENT', len(bm.verts), 'verts', len(bm.faces), 'faces')
    if args.render:
        render(args, body, ob, arm)
    export(args, ob, arm)
    print('GARMENT_DONE')


main()
