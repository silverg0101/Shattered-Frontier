"""Build the starter garment (linen tunic + dark leggings) for a MetaHuman player body, skinned to the body's own skeleton.

Blender 5.1, headless:
  "C:\\Program Files\\Blender Foundation\\Blender 5.1\\blender.exe" --factory-startup --background --python tools/Build-PlayerGarment.py -- \
      --body Artifacts/Characters/PlayerBase/OrlandoBare_body.fbx --out Artifacts/Characters/PlayerBase/garment_tunic_leggings.fbx \
      --render GameClient/ArtSource/Characters/Player/garments/tunic-leggings-blender.png \
      --head Artifacts/Characters/PlayerBase/{OrlandoBare,Orlando,Godric,Edwin,Wulfric,Osric}_head.obj --head-clearance 1.8
Input: the FBX that tools/Export-PlayerBaseBody.py writes from a "bare" build (tools/Build-PlayerMetaHumans.py, faces.json "bare": true).
A normal build omits the torso and thighs its default clothing hides, so it cannot be fitted. Rerun with the female body's FBX for her fit.
--head: every face of the fit's gender (female: HildaBare, Hilda, Aelfwyn, Eadgyth, Ebba, at the default
--head-clearance 1.0). Each face mesh carries a neck and chest bib below the body's neck seam; without it the bib shows as a flat skin patch inside the neckline.
Output: <out>.fbx (empty + armature "root" + ONE skinned mesh "SKM_Tunic_Leggings") and <out>.blend (adds the body, for review).

How it is built (all lengths cm, in the body's own frame):
- Leggings (material slot 1, "M_Leggings"): the body surface from the ankle to just under the skirt join, arms removed, pushed out along normals
  (--leggings-offset) and cut cleanly with bisect planes.
- Tunic (slot 0, "M_Tunic"): the body surface from the hips to the neck opening and to --sleeve-t of the upper arm, pushed out
  (--tunic-offset, extra on the sleeves), smoothed so it hangs looser than the muscles, then kept --clearance off the skin.
  The sleeve ends get a raised band (rolled cuff). A neckband rises from the neck seam (higher at the back), and a V-split is cut into the front of the neck opening. Below the hips a skirt is lofted
  from the hip border down to --hem (fraction of the thigh) as ONE tube around both thighs: each ring is the convex hull of the body's
  cross-section plus ease and flare. Two side slits are left open.
- Weights: nearest body triangle, barycentric, for every vertex; skirt weights are then smoothed so the tube does not tear between
  the legs; max 8 influences, normalised.
- UVs: one box projection, 1 UV unit per 40 cm, for a tiling cloth material. Seams fall where the dominant normal axis changes.
Offset shells follow the body's detail, so pass a larger --tunic-offset for heavier builds.
Female fit (round 3; each option defaults off, so the male output is unchanged):
  --drape 1 --drape-top 8 --skirt-ease 2.0 --skirt-flare 0 --skirt-straight 1 --hem 0.7 --join-smooth 6 --slit-height 18 --slit-open 3
  --thickness 0.3 --neck-trim 1.0 --neck-trim-height 0.25 --v-depth 7 --neck-radius 5.5 --cuff-width 4 --cuff-bulge 0.7 --cuff-fold 6
  --leggings-offset 0.4 --hem-thigh 0.95 --hem-blend 12 --side-even 0.15 --slit-backing 0.8
The --render sheet adds an approximate idle stance (front) and a stride from behind, to check the hem under the game's poses.
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
    p.add_argument('--sleeve-extra', type=float, default=0.5)
    p.add_argument('--leggings-offset', type=float, default=0.8)
    p.add_argument('--clearance', type=float, default=0.9, help='minimum tunic distance from the skin')
    p.add_argument('--sleeve-t', type=float, default=1.2, help='sleeve end along shoulder to elbow (>1: past the elbow, the rolled 3/4 sleeve)')
    p.add_argument('--cuff-width', type=float, default=3.5)
    p.add_argument('--cuff-bulge', type=float, default=1.3)
    p.add_argument('--hem', type=float, default=0.6, help='hem as a fraction of the thigh length down from the hip')
    p.add_argument('--skirt-ease', type=float, default=1.2)
    p.add_argument('--skirt-flare', type=float, default=1.0)
    p.add_argument('--slit-height', type=float, default=12.0)
    p.add_argument('--collar-front', type=float, default=4.5, help='cm the neckband rises above the body neck seam at the front')
    p.add_argument('--collar-back', type=float, default=4.0)
    p.add_argument('--neck-radius', type=float, default=6.5, help='cm: the neckband closes in to this radius (the head mesh carries the neck)')
    p.add_argument('--v-depth', type=float, default=10.0)
    p.add_argument('--v-half-angle', type=float, default=18.0)
    p.add_argument('--ankle-above-foot', type=float, default=2.5)
    p.add_argument('--uv-cm', type=float, default=40.0)
    p.add_argument('--drape', type=float, default=0.0, help='0..1: the torso hangs straight down from the chest instead of following the waist (1 for the female fit)')
    # Female round 2 (defaults leave the male output unchanged):
    p.add_argument('--drape-top', type=float, default=0.0, help='cm above spine_05 where the drape starts; the drape also bridges hollows per slice (no cling under or between the bust)')
    p.add_argument('--skirt-straight', type=int, default=0, help='1: skirt rings never narrow going down (hangs straight from the seat, no taper or bell)')
    p.add_argument('--join-smooth', type=int, default=0, help='smoothing passes across the hip join band (no seam/band where the skirt meets the torso)')
    p.add_argument('--slit-open', type=float, default=0.0, help='cm the side slits open at the hem (tapering shut at the slit top)')
    p.add_argument('--thickness', type=float, default=0.0, help='cm: give the tunic an inner layer and rims (backfaces and edges at hem, slits, neck, cuffs)')
    p.add_argument('--neck-trim', type=float, default=0.0, help='cm width of a raised trim band round the neck opening and slit')
    p.add_argument('--neck-trim-height', type=float, default=0.3)
    p.add_argument('--pelvis-hem', type=float, default=0.0, help='0..1: weight the front/back skirt panels to the pelvis, thighs only near the side slits')
    # Female round 3 (defaults off):
    p.add_argument('--hem-thigh', type=float, default=0.0, help='0..1: thigh share at the hem, rest pelvis; each half follows its own thigh (replaces --pelvis-hem)')
    p.add_argument('--hem-blend', type=float, default=12.0, help='cm either side of the centre line over which the halves blend to an even share of both thighs')
    p.add_argument('--side-even', type=float, default=0.5, help='0..1: at the side slits this part of the own-thigh share becomes even (no bell in a wide stance)')
    p.add_argument('--slit-backing', type=float, default=0.0, help='cm: a cloth facing this far behind the neck slit (no void at its base)')
    p.add_argument('--cuff-fold', type=float, default=0.0, help='cm: taper the last cm of the sleeve to the forearm before the folded cuff band')
    p.add_argument('--head', nargs='*', default=[], help="head mesh OBJs (tools/Export-PlayerBaseBody.py) of every face that wears this fit: the tunic is kept outside their necks and chest bibs")
    p.add_argument('--head-clearance', type=float, default=1.0, help='cm the tunic keeps off the head meshes')
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
    ramp = max(2.0, rows * 0.4)  # ease from the shell's waist into the hull over the top rows: a step there reads as a skirt seam
    for k in range(1, rows + 1):
        f = min(1.0, k / ramp)
        f = f * f * (3 - 2 * f)
        radii[k] = [(1 - f) * a + f * b for a, b in zip(r0, targets[k - 1])]
    for _ in range(24):  # smooth around the ring (the hull steps read as pleats), then down the rows
        for row in radii[1:]:
            row[:] = [(row[i - 1] + 2 * row[i] + row[(i + 1) % n]) / 4 for i in range(n)]
    if args.skirt_straight:
        for k in range(1, rows + 1):
            radii[k] = [max(a, b) for a, b in zip(radii[k], radii[k - 1])]
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
    if args.slit_open:  # part the slit's two edges round the ring, wide at the hem, shut at the slit top
        for k in range(1, rows + 1):
            z = z0 - (z0 - z_hem) * k / rows
            u = max(0.0, min(1.0, (z_hem + args.slit_height - z) / args.slit_height))
            for i in slit_cols:
                for col, sgn in ((i, -1), ((i + 1) % n, 1)):
                    v = ring[k][col]
                    t = Vector((-math.sin(th[col]), math.cos(th[col]), 0))
                    v.co += t * (sgn * 0.5 * args.slit_open * u)
    for k in range(rows):
        zmid = z0 - (z0 - z_hem) * (k + 0.5) / rows
        for i in range(n):
            if i in slit_cols and zmid < z_hem + args.slit_height:
                continue
            j = (i + 1) % n
            bm.faces.new((ring[k][i], ring[k + 1][i], ring[k + 1][j], ring[k][j]))
    return [v for row in ring[1:] for v in row]


def drape(bm, src, z_bottom, z_top, amount, bins=72, bridge=False):
    """Cloth falls from the widest point above: below z_top each torso vertex is pushed out to the largest radius (around the torso axis,
    per angular bin) of any vertex above it down from z_top, so the tunic hangs off the chest instead of hugging the waist."""
    bm.verts.index_update()
    armv = vert_arm(src, bm)
    band = [v for v in bm.verts if z_bottom <= v.co.z <= z_top and armv[v.index] < 0.2]
    if not band:
        return
    cx = sum(v.co.x for v in band) / len(band)
    cy = sum(v.co.y for v in band) / len(band)
    radius = lambda v: math.hypot(v.co.x - cx, v.co.y - cy)
    if bridge:  # cloth spans hollows: push each slice out to its convex hull (between and under the bust, along the spine)
        slices = {}
        for v in band:
            slices.setdefault(int(v.co.z // 1.5), []).append(v)
        for vs in slices.values():
            if len(vs) < 6:
                continue
            poly = hull([(v.co.x, v.co.y) for v in vs])
            sx, sy = sum(v.co.x for v in vs) / len(vs), sum(v.co.y for v in vs) / len(vs)
            for v in vs:
                dx, dy = v.co.x - sx, v.co.y - sy
                r = math.hypot(dx, dy)
                h = ray_hull(poly, (sx, sy), math.atan2(dy, dx))
                if r > 1e-6 and h > r:
                    v.co.x, v.co.y = sx + dx * h / r, sy + dy * h / r
    floor = {v: radius(v) for v in band}  # the shell is already clear of the skin: never pull cloth inside it
    best = [0.0] * bins
    for v in sorted(band, key=lambda v: -v.co.z):
        dx, dy = v.co.x - cx, v.co.y - cy
        r = math.hypot(dx, dy)
        b = int((math.atan2(dy, dx) + math.pi) / (2 * math.pi) * bins) % bins
        target = max(r, best[b])
        best[b] = target
        if r > 1e-6:
            s = (r + amount * (target - r)) / r
            v.co.x, v.co.y = cx + dx * s, cy + dy * s
    for _ in range(8):  # neighbouring bins pick different maxima: smooth the band so it reads as one fall of cloth, not lumps
        bmesh.ops.smooth_vert(bm, verts=band, factor=0.5, use_axis_x=True, use_axis_y=True, use_axis_z=False)
    for v in band:
        r = radius(v)
        if 1e-6 < r < floor[v]:
            v.co.x, v.co.y = cx + (v.co.x - cx) * floor[v] / r, cy + (v.co.y - cy) * floor[v] / r


def load_head(path):
    """A head OBJ as (BVH with outward normals, lowest z)."""
    lines = open(path).read().splitlines()
    vs = [(float(x), -float(y), float(z)) for _, x, y, z in (l.split() for l in lines if l.startswith('v '))]  # Unreal -> FBX-import frame
    head = bmesh.new()
    hv = [head.verts.new(c) for c in vs]
    for l in lines:
        if l.startswith('f '):
            try:
                head.faces.new([hv[int(i) - 1] for i in l.split()[1:4]])
            except ValueError:
                pass  # duplicate face
    head.normal_update()
    centre = sum((v.co for v in head.verts), Vector()) / len(head.verts)
    if sum((f.calc_center_median() - centre).dot(f.normal) for f in head.faces) < 0:  # the y flip mirrors the winding: face outward
        bmesh.ops.reverse_faces(head, faces=head.faces[:])
        head.normal_update()
    return BVHTree.FromBMesh(head), min(c[2] for c in vs)


def push_off_head(bm, paths, clearance):
    """The head mesh carries the neck and a chest bib down over the collarbones, ~9 cm below the body's neck seam; a tunic fitted to
    the body alone runs inside it, so the bib shows through the neckband and upper chest. Every face shares this tunic and each bib
    differs by a few cm, so push every tunic vertex out of all of them."""
    heads = [load_head(p) for p in paths]
    zmin = min(z for _, z in heads)

    def push_one(v, bvh):
        loc, nor, _i, d = bvh.find_nearest(v.co)
        if loc is None:
            return False
        off = v.co - loc
        # Only where the vertex lies over the bib (offset along its normal); past the bib's rim the nearest point is on the rim edge.
        if d > 1e-4 and abs(off.dot(nor)) < 0.9 * d:
            return False
        if off.dot(nor) >= clearance:
            return False
        v.co = loc + nor * clearance
        return True

    def push(v):
        return any([push_one(v, bvh) for bvh, _z in heads])  # a list, not a generator: every head gets its push

    moved = set()
    for _ in range(3):  # push, smooth the pushed band so the bib's edge leaves no step, push again
        moved |= {v for v in bm.verts if v.co.z >= zmin - 1.0 and push(v)}
        band = {o for v in moved for e in v.link_edges for o in e.verts if not o.is_boundary}  # smoothing an open edge lowers the neckline
        bmesh.ops.smooth_vert(bm, verts=list(band), factor=0.5, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    for _ in range(2):  # final passes: nothing inside any head (one head's push can land inside another)
        for v in bm.verts:
            if v.co.z >= zmin - 1.0:
                push(v)
    print('GARMENT_HEAD pushed', len(moved), 'verts')


def raise_collar(bm, args):
    """A neckband rising from the neck opening and closing in round the neck: the body mesh stops at the collarbones (the head mesh
    carries the neck), so its seam alone leaves a wide scoop."""
    border = {v for v in bm.verts if v.is_boundary and v.co.z > args.zneck}
    cx = sum(v.co.x for v in border) / len(border)
    cy = sum(v.co.y for v in border) / len(border)
    r = max(abs(v.co.y - cy) for v in border) or 1.0
    top = {}
    for v in border:
        back = min(1.0, max(0.0, 0.5 + 0.5 * (v.co.y - cy) / r))  # front is -y
        out = Vector((v.co.x - cx, v.co.y - cy, 0))
        rise = args.collar_front + (args.collar_back - args.collar_front) * back
        p = Vector((cx, cy, v.co.z + rise)) + out.normalized() * min(out.length, args.neck_radius) if out.length > 1e-6 else v.co
        top[v] = bm.verts.new(p)
    for e in [e for e in bm.edges if e.is_boundary and e.verts[0] in top and e.verts[1] in top]:
        a, b = e.verts
        bm.faces.new((a, b, top[b], top[a]))
    bm.verts.ensure_lookup_table()
    bm.verts.index_update()


def add_v_split(bm, args):
    border = [v for v in bm.verts if v.is_boundary and v.co.z > args.zneck]
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
    args.v_faces = [[v.co.copy() for v in f.verts] for f in inside]
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


def taper_cuff(bm, src, a_pt, direction, plane_d, width, length, clearance, armv):
    """Pull the sleeve's last `length` cm above the cuff band in to `clearance` off the forearm: a cuff close to the arm, no bell."""
    for v in bm.verts:
        along = (v.co - a_pt).dot(direction)
        if not (plane_d - width - length < along <= plane_d + 0.01 and armv[v.index] > 0.3 and (v.co.x - a_pt.x) * direction.x > 0):
            continue
        t = min(1.0, (along - (plane_d - width - length)) / length)
        t = t * t * (3 - 2 * t)
        _loc, _n, _i, dist = src.nearest(v.co)
        radial = (v.co - a_pt) - direction * along
        if dist > clearance and radial.length > 1e-6:
            v.co -= radial.normalized() * (dist - clearance) * t


def slit_backing(bm, src, args):
    """A facing behind the neck slit: its faces again, pushed toward the skin and widened so its edges sit under the cloth."""
    polys = args.v_faces
    if not polys:
        return
    pts = [p for f in polys for p in f]
    c = sum(pts, Vector()) / len(pts)
    made = {}

    def vert(p):
        k = tuple(round(x, 4) for x in p)
        if k not in made:
            q = Vector((c.x + (p.x - c.x) * 1.6, p.y, p.z))
            loc, nor, _i, dist = src.nearest(q)
            d = q - loc
            out = d.normalized() if d.length > 1e-6 else nor
            made[k] = bm.verts.new(loc + out * max(0.4, dist - args.slit_backing))
        return made[k]
    for f in polys:
        try:
            bm.faces.new([vert(p) for p in f])
        except ValueError:
            pass
    bm.verts.ensure_lookup_table()
    bm.verts.index_update()


def away_from_skin(src, v):
    loc, nor, _i, _d = src.nearest(v.co)
    d = v.co - loc
    return d.normalized() if d.length > 1e-6 else nor


def neck_trim(bm, src, args, armv):
    """A raised band of --neck-trim cm along the neck opening and its slit: the stitched trim."""
    import heapq
    edge = [v for v in bm.verts if v.is_boundary and v.co.z > args.zjoin + 4 and armv.get(v.index, 0) < 0.3]
    dist = {v: 0.0 for v in edge}
    heap = [(0.0, v.index, v) for v in edge]
    while heap:
        d, _i, v = heapq.heappop(heap)
        if d > dist[v]:
            continue
        for e in v.link_edges:
            o = e.other_vert(v)
            nd = d + e.calc_length()
            if nd < args.neck_trim and nd < dist.get(o, 1e9):
                dist[o] = nd
                heapq.heappush(heap, (nd, o.index, o))
    for v, d in dist.items():
        v.co += away_from_skin(src, v) * args.neck_trim_height * (1.0 if d < args.neck_trim * 0.8 else 0.5)


def smooth_join(bm, src, args):
    """Blend the torso shell into the skirt's top rows so the hip join reads as one panel, not a band."""
    bm.verts.index_update()
    armv = vert_arm(src, bm)
    band = [v for v in bm.verts if abs(v.co.z - args.zjoin) < 8 and armv[v.index] < 0.2 and not v.is_boundary]
    for _ in range(args.join_smooth):
        bmesh.ops.smooth_vert(bm, verts=band, factor=0.5, use_axis_x=True, use_axis_y=True, use_axis_z=False)
    for v in band:
        loc, nor, _i, dist = src.nearest(v.co)
        if dist < args.clearance:
            v.co = loc + away_from_skin(src, v) * args.clearance


def thicken(bm, t, skirt):
    """Inner layer t cm inside the cloth plus rims on every open edge, so hems, slits and the neck show an edge and a back side."""
    bm.normal_update()
    rim = []
    for e in bm.edges:
        if e.is_boundary:
            l = e.link_loops[0]
            rim.append((l.vert, l.link_loop_next.vert))
    faces = bm.faces[:]
    inner = {v: bm.verts.new(v.co - v.normal * t) for v in bm.verts[:]}
    for f in faces:
        bm.faces.new([inner[v] for v in reversed(f.verts)])
    rims = {bm.faces.new((b, a, inner[a], inner[b])) for a, b in rim}
    bm.verts.ensure_lookup_table()
    return skirt | {inner[v] for v in skirt}, rims


def build(args, src, arm):
    bones = arm.data.bones
    head = lambda n: Vector(bones[n].head_local)
    z_ankle = head('foot_l').z + args.ankle_above_foot
    z_join = head('pelvis').z - 3.0
    args.zjoin = z_join
    # Neck opening search height: 140 cm on the male body, a little under spine_05 (scales to shorter bodies).
    args.zneck = head('spine_05').z - 0.2 * (head('neck_01').z - head('spine_05').z)
    z_hem = head('thigh_l').z - args.hem * (head('thigh_l').z - head('calf_l').z)
    args.zhem, args.thigh_l_x = z_hem, head('thigh_l').x

    # --- leggings: ankle to hip, no arms ---
    legs = shell(src, args.leggings_offset, clearance=0.35)
    cut(legs, Vector((0, 0, z_ankle)), Vector((0, 0, 1)), keep_negative=False)
    # Ends just under the skirt join: higher, the leggings shell meets the tunic shell and shows through it at the waist.
    cut(legs, Vector((0, 0, z_join - 1.0)), Vector((0, 0, 1)), keep_negative=True)
    armv = vert_arm(src, legs)
    drop(legs, [v for v in legs.verts if armv[v.index] > 0.4])
    keep_largest(legs)

    # --- tunic: hip to neck and upper arms ---
    tun = shell(src, args.tunic_offset, extra_arm=args.sleeve_extra, smooth=4, clearance=args.clearance)
    cut(tun, Vector((0, 0, z_join)), Vector((0, 0, 1)), keep_negative=False)
    if args.drape:
        drape(tun, src, z_join, head('spine_05').z + args.drape_top, args.drape, bridge=args.drape_top > 0)
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
        if args.cuff_fold:
            taper_cuff(tun, src, a, d, dist, args.cuff_width, args.cuff_fold, args.clearance, armv)
        rolled_cuff(tun, a, d, dist, args.cuff_width, args.cuff_bulge, armv)
    raise_collar(tun, args)
    add_v_split(tun, args)
    if args.neck_trim or args.thickness:  # close the body mesh's pinholes first, or they get trims and rims (a dash on the shoulder)
        bmesh.ops.holes_fill(tun, edges=[e for e in tun.edges if e.is_boundary], sides=12)
    if args.neck_trim:
        tun.verts.index_update()
        neck_trim(tun, src, args, vert_arm(src, tun))
    skirt = loft_skirt(tun, src, args, z_hem)
    if args.join_smooth:
        smooth_join(tun, src, args)
    if args.slit_backing:  # after the smoothing passes: they would pull the loose facing about
        slit_backing(tun, src, args)
    if args.head:
        push_off_head(tun, args.head, args.head_clearance)
    bmesh.ops.recalc_face_normals(tun, faces=tun.faces[:])
    skirt = set(skirt)
    hard = set()
    if args.thickness:
        skirt, hard = thicken(tun, args.thickness, skirt)  # rims shade flat: a lit edge, not a dark smoothed seam
    tun.verts.index_update()
    skirt_idx = {v.index for v in skirt}

    # --- one bmesh: tunic (slot 0) then leggings (slot 1) ---
    out = bmesh.new()
    skirt_new = set()
    for part, slot in ((tun, 0), (legs, 1)):
        part.verts.index_update()
        vmap = {v.index: out.verts.new(v.co) for v in part.verts}
        for f in part.faces:
            nf = out.faces.new([vmap[v.index] for v in f.verts])
            nf.material_index = slot
            nf.smooth = not (slot == 0 and f in hard)
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


def skin(out, src, skirt_ids, args):
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
    if args.hem_thigh and skirt_ids:
        # Each half of the skirt follows its own thigh (forward leg stays inside the cloth), blending to an even share of both across
        # the centre (one panel, no split up the back). At the side slits part of the own share turns even: abduction is mirror
        # symmetric and cancels in an even share, so a wide stance does not swing the sides out into a bell. Ramps in from the join.
        cx = sum(out.verts[i].co.x for i in skirt_ids) / len(skirt_ids)
        cy = sum(out.verts[i].co.y for i in skirt_ids) / len(skirt_ids)
        for i in skirt_ids:
            c = out.verts[i].co
            dx, dy = c.x - cx, c.y - cy
            sideness = abs(math.cos(math.atan2(dy, dx))) ** 4  # 1 at the slits, 0 on the front/back centre line
            g = min(1.0, abs(dx) / args.hem_blend) * (1 - args.side_even * sideness)
            k = args.hem_thigh * min(1.0, max(0.0, (args.zjoin - c.z) / max(1.0, 0.25 * (args.zjoin - args.zhem))))  # full from 25% down: the thigh fills the upper skirt
            own, other = ('thigh_l', 'thigh_r') if dx * args.thigh_l_x > 0 else ('thigh_r', 'thigh_l')
            new = {'pelvis': 1 - k, own: k * (0.5 + 0.5 * g), other: k * (0.5 - 0.5 * g)}
            depth = min(1.0, max(0.0, (args.zjoin - c.z) / 12.0))  # upper skirt keeps the body's weights (its corrective bones): no step, no poke at the hip
            W[i] = {n: depth * new.get(n, 0.0) + (1 - depth) * W[i].get(n, 0.0) for n in set(new) | set(W[i])}
    elif args.pelvis_hem and skirt_ids:  # front/back panels ride the pelvis (no wrap between the thighs); thighs only near the slits
        cx = sum(out.verts[i].co.x for i in skirt_ids) / len(skirt_ids)
        cy = sum(out.verts[i].co.y for i in skirt_ids) / len(skirt_ids)
        for i in skirt_ids:
            c = out.verts[i].co
            side = abs(math.cos(math.atan2(c.y - cy, c.x - cx)))
            f = (1 - args.pelvis_hem) + args.pelvis_hem * side ** 2
            depth = min(1.0, max(0.0, (args.zjoin - c.z) / 6.0))  # top rows keep the shell's weights: no step at the join
            f = 1 - depth * (1 - f)
            w = {n: x * f for n, x in W[i].items()}
            w['pelvis'] = w.get('pelvis', 0.0) + (1 - f)
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
        p.use_smooth = f.smooth
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
            if n not in groups:  # e.g. thigh_l: the body skins only its corrective children
                groups[n] = ob.vertex_groups.new(name=n)
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


def pose_idle(arm):
    """The game's idle stance, roughly (from the in-game captures): knees a little out, the left thigh 20 deg forward."""
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()
    for name in ('thigh_l', 'thigh_r'):
        pb = arm.pose.bones[name]
        h = Vector(arm.data.bones[name].head_local)
        out = -6 if h.x > 0 else 6
        r = Matrix.Translation(h) @ Matrix.Rotation(math.radians(out), 4, 'Y') @ Matrix.Rotation(math.radians(-20 if name == 'thigh_l' else -3), 4, 'X') @ Matrix.Translation(-h)
        pb.matrix = r @ pb.matrix
        bpy.context.view_layer.update()


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
             ('back', (0, 5, 0.8), (90, 0, 180), False), ('idle', (0, -5, 0.8), (90, 0, 0), 'idle'),
             ('stride', (5, 0, 0.8), (90, 0, 90), True), ('stride_back', (0, 5, 0.8), (90, 0, 180), True)]
    tiles = []
    for name, loc, rotd, stride in views:
        if stride == 'idle':
            pose_idle(arm)
        elif stride and name == 'stride':
            for pb in arm.pose.bones:
                pb.matrix_basis = Matrix.Identity(4)
            bpy.context.view_layer.update()
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
    W = skin(bm, src, skirt_ids, args)
    ob = make_object(bm, W, body, arm)
    print('GARMENT', len(bm.verts), 'verts', len(bm.faces), 'faces')
    if args.render:
        render(args, body, ob, arm)
    export(args, ob, arm)
    print('GARMENT_DONE')


main()
