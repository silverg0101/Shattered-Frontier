"""GEAR-02: fit an Asset Forge garment mesh (GLB) to the player body as a skinned game garment, keeping the generated shape. Deterministic, no hand edits.

Blender 5.1, headless (lengths in cm, the body FBX's own units):
  blender --factory-startup -b --python tools/Clean-GeneratedGarment.py -- --garment GameClient/ArtSource/Characters/Player/outfit-asset/hood_v4/hood.glb --out Artifacts/Characters/PlayerBase/hood/hood.fbx [--slot head] [--stages DIR]
  blender --factory-startup -b --python tools/Clean-GeneratedGarment.py -- --measure --slot chest     (re-measure the slot's body anchor rings on Orlando and write them into its JSON)

The Forge mesh is a closed, thin (2 to 19 mm) double-layer solid. Steps (round 4; round 2's anisotropic neck fit, hem calming and 10 cm shrinkwrap wrecked the shape):
1 weld, drop loose specks, PCA pre-align (reported only; the run fails if the main axis is more than 30 deg off the slot axis);
2 inner layer = faces that see the slot axis without crossing their own wall; its boundary loops are the openings;
3 fit: least-squares per-axis scale + translation that puts the garment features (top, back, waist, slices, boundary loops matched in order along the slot axis)
  on the body anchors of tools/garment-slots/<slot>.json (rings: centre + radius, points: extreme + direction), each target pushed out by its region's clearance;
3b level_hem_below_cm: a per-azimuth vertical stretch so the (blurred) hem lands level on its lowest point;
3c drape (radial regions): per (azimuth, height) bin the radius the inner layer needs, carried down with the raw bell's own flare, convex hull per slice
  (cloth spans hollows), blurred, applied to both layers: the cape clears the shoulders as one smooth bell instead of push lobes;
4 push-out, Shrinkwrap "outside" semantics measured on the INNER layer: each inner vertex needs what it cuts into the skin plus what it lacks of the region offset
  (the offset part fades toward openings and hems); the needs become a smooth voxel displacement field (normalised box-blur convolution, wide to narrow radii) that moves
  BOTH layers together, so the wall keeps its generated thickness; "radial" regions (cape) push away from the slot axis in the early passes, skin normal in the last two;
5 decimate (collapse) only when the triangle budget needs it; clear stale split normals, recalc outward; bake the Forge colour onto a fresh unwrap, grade to
  the wool colour, add a fine slub weave, lift the inner layer's albedo (inner_albedo_lift);
6 checks (fail the run): ring residual, shape drift from the raw mesh outside the pushed region, clearance and overlaps, back rays through the garment, the same in each pose;
7 skin weights from the nearest body triangle, smoothed over the cloth, arm bones keep arm_weight_keep (rest to the clavicle), neck ramp to neck_02/head; FBX + armature + PNG + .blend.
"""
import argparse
import json
import math
import os
import sys
import time

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.join(ROOT, 'Artifacts/Characters/PlayerBase')
AX = {'x': 0, 'y': 1, 'z': 2}
WOOL_SRGB = (0.30, 0.225, 0.185)  # mean colour of the undyed wool, a cooler mid-brown (sRGB 0..1); round 2's (0.34, 0.24, 0.155) read ochre
ARM = ('clavicle', 'upperarm', 'lowerarm', 'hand', 'thumb', 'index', 'middle', 'ring', 'pinky')  # swinging arms are not collision for a cape (it is skinned to them)


def parse():
    p = argparse.ArgumentParser()
    p.add_argument('--garment')
    p.add_argument('--out')
    p.add_argument('--body', default=os.path.join(BASE, 'OrlandoBare_body.fbx'), help='the full body (Orlando_body.fbx has the default-garment faces cut out of the upper torso)')
    p.add_argument('--head', default=os.path.join(BASE, 'Orlando_head.obj'))
    p.add_argument('--slot', default='head', help='tools/garment-slots/<slot>.json')
    p.add_argument('--weld', type=float, default=0.00001, help='garment units: merge vertices closer than this (glTF splits UV seams); 0.0008 welded across the thin wall (17 boundary edges vs 5)')
    p.add_argument('--tex', type=int, default=2048)
    p.add_argument('--stages', default='', help='write an OBJ (metres, Y up) of the mesh after each stage into this folder, for clay sheets')
    p.add_argument('--measure', action='store_true', help='measure the slot body rings that name a bone and write them into the slot JSON')
    p.add_argument('--skip-checks', action='store_true', help='diagnose only: export even when a check fails')
    p.add_argument('--tag', default='')
    a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
    a.cfg_path = os.path.join(ROOT, 'tools/garment-slots', a.slot + '.json')
    a.cfg = json.load(open(a.cfg_path))
    return a


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def dump(a, name, bm):
    if not a.stages:
        return
    os.makedirs(a.stages, exist_ok=True)
    bm.verts.index_update()
    with open(os.path.join(a.stages, f'stage_{name}.obj'), 'w') as fh:
        fh.write(''.join('v %f %f %f\n' % (v.co.x * .01, v.co.z * .01, -v.co.y * .01) for v in bm.verts))
        fh.write(''.join('f ' + ' '.join(str(v.index + 1) for v in f.verts) + '\n' for f in bm.faces))


def load_head(path):
    lines = open(path).read().splitlines()
    vs = [(float(x), -float(y), float(z)) for _, x, y, z in (l.split() for l in lines if l.startswith('v '))]  # Unreal -> FBX frame, cm
    fs = [[int(i.split('/')[0]) - 1 for i in l.split()[1:4]] for l in lines if l.startswith('f ')]
    bm = bmesh.new()
    vv = [bm.verts.new(c) for c in vs]
    for f in fs:
        try:
            bm.faces.new([vv[i] for i in f])
        except ValueError:
            pass
    bm.normal_update()
    c = sum((v.co for v in bm.verts), Vector()) / len(bm.verts)
    if sum((f.calc_center_median() - c).dot(f.normal) for f in bm.faces) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    return bm


def merged(*bms):
    out = bmesh.new()
    for src in bms:
        tmp = bpy.data.meshes.new('t'); src.to_mesh(tmp); out.from_mesh(tmp); bpy.data.meshes.remove(tmp)
    out.normal_update()
    return out


def load_body(a):
    bpy.ops.import_scene.fbx(filepath=os.path.abspath(a.body))
    body = next(o for o in bpy.data.objects if o.type == 'MESH')
    arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    body.parent = None; arm.parent = None  # the importer's 0.01 Empty: the data itself is cm
    for o in [o for o in bpy.data.objects if o.type == 'EMPTY']:
        bpy.data.objects.remove(o)
    bpy.context.view_layer.update()
    return body, arm


def body_bm(body, dg=None):
    bm = bmesh.new()
    bm.from_mesh(body.evaluated_get(dg).to_mesh() if dg else body.data)
    bm.transform(body.matrix_world)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.normal_update()
    return bm


def arm_mask(body, cfg):
    gname = {g.index: g.name for g in body.vertex_groups}  # ARM[1:]: the clavicle (shoulder cap) stays collision
    keep_above = cfg.get('arm_caps_above_cm', 1e9)  # the shoulder caps (deltoids) above this height stay collision: a cape rounds over them instead of a cut cliff
    return [cfg.get('arms_not_collision', False) and v.co.z < keep_above and sum(w.weight for w in v.groups if gname[w.group].startswith(ARM[1:])) >= 0.5
            for v in body.data.vertices]


def without_arms(bm, mask):
    bm = bm.copy(); bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.verts[i] for i, f in enumerate(mask) if f], context='VERTS')
    bm.normal_update()
    return bm


# ---------- measurement of body anchor rings (all slots) ----------

def ring_at(bm, centre, normal, reach):
    """Cross-section of the body by the plane (centre, normal): centre and the two radii (along the plane's x-ish and y-ish axes) of the cut within `reach` cm."""
    c = bm.copy()
    r = bmesh.ops.bisect_plane(c, geom=c.verts[:] + c.edges[:] + c.faces[:], plane_co=centre, plane_no=normal)
    pts = np.array([v.co for v in r['geom_cut'] if isinstance(v, bmesh.types.BMVert) and (v.co - centre).length < reach])
    c.free()
    ctr = (pts.min(0) + pts.max(0)) / 2
    return [round(float(x), 1) for x in ctr], [round(float(x), 1) for x in (pts.max(0) - pts.min(0)) / 2]


def measure(a):
    body, arm = load_body(a)
    hb = load_head(a.head)
    bm = merged(body_bm(body), hb)  # the neck lives on the head mesh
    for an in a.cfg['anchors']:
        if 'height_cm' in an:  # a horizontal slice of the collision body (without the arms when the slot says so), e.g. the shoulders under a cape
            c, r = ring_at(merged(without_arms(body_bm(body), arm_mask(body, a.cfg)), hb), Vector((0, 0, an['height_cm'])), Vector((0, 0, 1)), an.get('reach_cm', 40))
        elif 'bone' in an:
            b = arm.data.bones[an['bone']]
            nxt = arm.data.bones[an['toward']] if 'toward' in an else b.children[0]  # imported tails point along +Y, not down the limb: aim at the child joint
            n = Vector(an['normal']) if 'normal' in an else (nxt.head_local - b.head_local).normalized()
            c, r = ring_at(bm, b.head_local + n * an.get('along_cm', 0.0), n, an.get('reach_cm', 25))
        else:
            continue
        an['centre'], an['radius'] = c, r
        print(f"MEASURE {an['name']}: centre {c} radius {r}")
    val = lambda v: '[\n' + ',\n'.join('    ' + json.dumps(x) for x in v) + '\n  ]' if isinstance(v, list) and v and isinstance(v[0], dict) else json.dumps(v)
    open(a.cfg_path, 'w').write('{\n' + ',\n'.join(f'  {json.dumps(k)}: {val(v)}' for k, v in a.cfg.items()) + '\n}\n')  # one anchor / region per line


# ---------- garment analysis ----------

def drop_specks(bm, frac=0.02):
    """Delete loose parts under frac of the largest (the Forge mesh carries a few floating specks). Returns how many were dropped."""
    seen, parts = set(), []
    for f in bm.faces:
        if f in seen:
            continue
        st, comp = [f], []
        seen.add(f)
        while st:
            g = st.pop(); comp.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h not in seen:
                        seen.add(h); st.append(h)
        parts.append(comp)
    big = max(len(c) for c in parts)
    small = [c for c in parts if len(c) < big * frac]
    bmesh.ops.delete(bm, geom=[f for c in small for f in c], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    return len(small)


def pca_align(bm, axis, max_tilt=30.0):
    """PCA pre-align check: the main principal axis must lie along the slot axis. Forge GLBs come in Z up, facing -Y; the measured tilt (7 deg on hood v4) is the
    garment's own asymmetry, so it is reported, not rotated away (that tipped the hood sideways). ponytail: a garment that comes in on its side fails; add the rotation then."""
    P = np.array([v.co for v in bm.verts]); c = P.mean(0)
    w, V = np.linalg.eigh(np.cov((P - c).T))
    tilt = math.degrees(math.acos(min(1.0, abs(V[AX[axis], int(np.argmax(w))]))))
    print(f'PCA main axis {np.round(V[:, int(np.argmax(w))], 3).tolist()}: {tilt:.1f} deg off the slot axis {axis}')
    if tilt > max_tilt:
        raise SystemExit(f'PCA: garment main axis {tilt:.0f} deg off {axis}')
    return c


def inner_faces(bm, axis, c):
    """A face is on the inner layer when the segment from it to the slot axis (a line through the garment centre) leaves along its normal and does not cross the
    garment: the outer layer always has its own inner wall in the way, fold walls included. ponytail: one axis line, so two-tube garments (legs) need one line per tube."""
    tree = BVHTree.FromBMesh(bm)
    k = AX[axis]; lo = min(v.co[k] for v in bm.verts); hi = max(v.co[k] for v in bm.verts)
    inner = []
    for f in bm.faces:
        p = f.calc_center_median()
        q = Vector(c); q[k] = min(max(p[k], lo + 0.05 * (hi - lo)), hi - 0.12 * (hi - lo))
        d = q - p; L = d.length
        if L < 1e-9 or f.normal.dot(d) <= 0:
            inner.append(False); continue
        d /= L
        inner.append(tree.ray_cast(p + d * 1e-5, d, L)[0] is None)
    for _ in range(2):  # neighbour majority: rim faces settle on one side
        inner = [sum(inner[g.index] for e in f.edges for g in e.link_faces if g is not f) * 2 > sum(1 for e in f.edges for g in e.link_faces if g is not f)
                 for f in bm.faces]
    return inner


def boundary_loops(bm, inner):
    """Boundary loops of the inner-layer sheet (its openings: face opening, hem, cuffs), largest first, as vertex index lists."""
    fin = [f for f, i in zip(bm.faces, inner) if i]
    fset = set(fin)
    edges = [e for f in fin for e in f.edges if sum(g in fset for g in e.link_faces) == 1]
    adj = {}
    for e in edges:
        for v in e.verts:
            adj.setdefault(v.index, []).append(e.other_vert(v).index)
    seen, loops = set(), []
    for s in adj:
        if s in seen:
            continue
        st, comp = [s], []
        seen.add(s)
        while st:
            x = st.pop(); comp.append(x)
            for y in adj[x]:
                if y not in seen:
                    seen.add(y); st.append(y)
        loops.append(comp)
    return sorted(loops, key=len, reverse=True)


def feature(P, kind, axis, T=None, an=None, loops=None, loop_rank=None):
    """A garment feature on the inner-layer vertices P (garment units): 'top', 'bottom', '+y'/'-y'/'+x'/'-x' (extreme above the waist), 'waist' (narrowest slice in
    the middle band), 'slice' (the slice at the height the current fit T maps onto the anchor), 'loop' (the loop_rank-th boundary loop along the axis).
    Returns (point, half-extents)."""
    k = AX[axis]; lo, hi = P[:, k].min(), P[:, k].max(); H = hi - lo
    def sl(h):
        s = P[np.abs(P[:, k] - h) < H / 150]
        return (s.min(0) + s.max(0)) / 2, (s.max(0) - s.min(0)) / 2
    if kind == 'top':
        return P[np.argmax(P[:, k])], None
    if kind == 'bottom':
        return P[np.argmin(P[:, k])], None
    if kind == 'waist':
        hs = np.linspace(lo + 0.30 * H, lo + 0.62 * H, 40)
        h = min(hs, key=lambda h: sl(h)[1][0])
        c, r = sl(h); c[k] = h
        return c, r
    if kind == 'slice':
        s, t = T
        h = (an['centre'][k] - t[k]) / s[k]
        c, r = sl(h); c[k] = h
        return c, r
    if kind == 'loop':
        L = sorted(loops, key=lambda l: -P[l][:, k].mean())  # top to bottom
        Q = P[L[loop_rank]]
        return Q.mean(0), (Q.max(0) - Q.min(0)) / 2
    sign, j = (1 if kind[0] == '+' else -1), AX[kind[1]]
    w = feature(P, 'waist', axis)[0][k]
    up = P[P[:, k] > w]
    return up[np.argmax(sign * up[:, j])], None


def solve_fit(P, cfg, loops, offset_at):
    """Least squares per-axis scale s and translation t (body cm = s * garment + t) over the anchors' matched components. A ring matches its centre (x, y, z) and its
    radii (rx, ry, rz, offset added); a point matches its coordinates (offset added along `dir`). Slices depend on the fit, so it is iterated."""
    axis = cfg['axis']
    T = None
    loop_anchors = sorted([an for an in cfg['anchors'] if an['garment'] == 'loop'], key=lambda an: -an['centre'][AX[axis]])
    for it in range(4):
        rows = {0: [], 1: [], 2: []}; names = []
        for an in cfg['anchors']:
            if an['garment'] == 'slice' and T is None:
                continue
            rank = loop_anchors.index(an) if an in loop_anchors else None
            g, gr = feature(P, an['garment'], axis, T, an, loops, rank)
            if an['kind'] == 'point':
                b = np.array(an['body']) + np.array(an.get('dir', (0, 0, 0))) * offset_at(an['body'][AX[axis]], an)
            else:
                b = np.array(an['centre']); off = offset_at(an['centre'][AX[axis]], an)
            for m in an['match']:
                j = AX[m[-1]]
                if m.startswith('r'):
                    rows[j].append((gr[j], 0.0, an['radius'][j] + off, an['name'] + '.' + m))
                else:
                    rows[j].append((g[j], 1.0, b[j], an['name'] + '.' + m))
        s, t = np.zeros(3), np.zeros(3)
        for j in range(3):
            A = np.array([[r[0], r[1]] for r in rows[j]]); y = np.array([r[2] for r in rows[j]])
            if len(rows[j]) and np.linalg.matrix_rank(A) == 2:
                s[j], t[j] = np.linalg.lstsq(A, y, rcond=None)[0]
        for j in range(3):  # an axis without scale information takes the mean scale of the others (uniform), then its translation
            if s[j] == 0:
                s[j] = s[s != 0].mean() if (s != 0).any() else 100.0
                pts = [r for r in rows[j] if r[1] == 1.0]
                t[j] = np.mean([r[2] - r[0] * s[j] for r in pts]) if pts else 0.0
        T = (s, t)
    res = [(r[3], (r[0] * s[j] + r[1] * t[j] - r[2]) * 10) for j in range(3) for r in rows[j]]
    return s, t, res


# ---------- push-out ----------

def box_blur(G, r, axes=(0, 1, 2)):
    """Three box passes of half-width r cells along each axis (about a Gaussian of sigma r)."""
    for ax in axes:
        for _ in range(3):
            c = np.cumsum(np.pad(G, [(r + 1, r) if i == ax else (0, 0) for i in range(G.ndim)]), axis=ax)
            G = (np.take(c, range(2 * r + 1, c.shape[ax]), axis=ax) - np.take(c, range(0, c.shape[ax] - 2 * r - 1), axis=ax)) / (2 * r + 1)
    return G


def smooth_field(P, V, X, cell, radius):
    """Spatially smooth vector field: the vectors V at points P are averaged in a voxel grid (normalised convolution, about Gaussian with sigma `radius` cm) and read
    back trilinearly at X. Both cloth layers at one place get the same move, so the wall keeps its thickness and nothing crumples."""
    lo = np.minimum(P.min(0), X.min(0)) - radius * 3 - cell; dims = np.ceil((np.maximum(P.max(0), X.max(0)) + radius * 3 + cell - lo) / cell).astype(int) + 1
    ij = np.round((P - lo) / cell).astype(int)
    S = np.zeros(tuple(dims) + (4,))
    np.add.at(S, (ij[:, 0], ij[:, 1], ij[:, 2]), np.c_[V, np.ones(len(V))])
    S = box_blur(S, max(1, int(round(radius / cell))))
    F = S[..., :3] / np.maximum(S[..., 3:], 1e-9)
    q = (X - lo) / cell; i0 = np.floor(q).astype(int); t = q - i0
    out = np.zeros((len(X), 3))
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (t[:, 0] if dx else 1 - t[:, 0]) * (t[:, 1] if dy else 1 - t[:, 1]) * (t[:, 2] if dz else 1 - t[:, 2])
                out += w[:, None] * F[i0[:, 0] + dx, i0[:, 1] + dy, i0[:, 2] + dz]
    return out


def level_hem(bm, axis_c, z_top, target=None, blend=0.0, dth=3.0, sigma=8.0, hem=None):
    """Round 4: a level hem. Per azimuth bin the hem height h is the lowest vertex; h is blurred over azimuth (sigma deg) and every vertex below z_top is
    stretched along z about z_top so the blurred hem lands on H (the lowest one, or `target` cm: round 5 lengthens the cape to it). The stretch of a vertex d cm
    below z_top is 1 + (K - 1) * smoothstep(d / blend), K per azimuth so the hem lands on H: the shoulders just under the neck band keep the raw slope and the bell
    below takes the length. Fine bins and a narrow blur (round 5) close the raw hem's notches. A smooth vertical scale per azimuth (both layers alike), so the
    bell and its folds are kept. Returns (hem range before, after) in cm."""
    X = np.array([v.co for v in bm.verts])
    nt = int(360 / dth)
    th = np.degrees(np.arctan2(X[:, 1] - axis_c[1], X[:, 0] - axis_c[0])) % 360
    ti = (th / dth).astype(int) % nt
    h = np.full(nt, np.inf)
    np.minimum.at(h, ti, X[:, 2])
    h = np.where(np.isfinite(h), h, np.nanmin(np.where(np.isfinite(h), h, np.nan)))
    k = max(1, int(round(sigma / dth)))
    hb = box_blur(np.concatenate([h[-nt // 2:], h, h[:nt // 2]]), k, axes=(0,))[nt // 2:nt // 2 + nt]
    H = h.min() if target is None else target
    qt = th / dth - 0.5; t0 = np.floor(qt).astype(int); b = qt - t0
    hv = hb[t0 % nt] * (1 - b) + hb[(t0 + 1) % nt] * b
    if hem is not None:  # round 5: the hem height under each vertex is read off the hem loop itself (inverse-distance mean of the 6 nearest loop points):
        L = X[hem]  # the raw centre front and back notches sit on radial pleats, where one azimuth holds both a low pleat wall and the raised notch
        kd = KDTree(len(L))
        for n, p in enumerate(L):
            kd.insert(p, n)
        kd.balance()
        for i in np.where(X[:, 2] < z_top)[0]:
            nb = kd.find_n(X[i], 6)
            wts = np.array([1.0 / (d + 0.1) ** 3 for _c, _n, d in nb])
            hv[i] = float(np.dot(wts, [L[n_, 2] for _c, n_, _d in nb]) / wts.sum())
    below = X[:, 2] < z_top
    d = z_top - X[below, 2]; dh = np.maximum(z_top - hv[below], 1e-3)
    ss = (lambda t: t * t * (3 - 2 * t))(np.clip(d / blend, 0, 1)) if blend > 0 else np.ones_like(d)
    ssh = (lambda t: t * t * (3 - 2 * t))(np.clip(dh / blend, 0, 1)) if blend > 0 else np.ones_like(d)
    K = 1 + ((z_top - H) / dh - 1) / np.maximum(ssh, 1e-3)
    X[below, 2] = z_top - d * (1 + (K - 1) * ss)
    for v, x in zip(bm.verts, X):
        v.co = Vector(x)
    h2 = np.full(nt, np.inf); np.minimum.at(h2, ti, X[:, 2])
    return (float(h.min()), float(h.max())), (float(h2[np.isfinite(h2)].min()), float(h2[np.isfinite(h2)].max()))


def circ_blur(G, r):
    """box_blur along axis 1 (azimuth bins), wrapping around."""
    nt = G.shape[1]
    return box_blur(np.concatenate([G[:, -nt // 2:], G, G[:, :nt // 2]], axis=1), max(1, r), axes=(1,))[:, nt // 2:nt // 2 + nt]


def bin_lerp(T, z, th, z0, dz, dth):
    """Bilinear read of the (height, azimuth) bin table T at the vertices (z, th deg), between bin centres; azimuth wraps."""
    nz, nt = T.shape
    qz = np.clip((z - z0) / dz - 0.5, 0, nz - 1.001); qt = th / dth - 0.5
    z_, t_ = np.floor(qz).astype(int), np.floor(qt).astype(int); a_, b_ = qz - z_, qt - t_
    return sum(T[z_ + i, (t_ + j) % nt] * (a_ if i else 1 - a_) * (b_ if j else 1 - b_) for i in (0, 1) for j in (0, 1))


def drape_scale(bm, inner, skin, offset_at, margin, axis_c, z_join, ramp_cm, front_gap, front_deg, k_max, fit_below, dth=3.0):
    """Round 5 (replaces round 4's additive drape, whose constant outward shift + convex hull turned the bell into a wide, flat yoke): each azimuth column of the
    cape below the neck band is scaled about the slot axis, r' = r * (1 + (k - 1) * ramp(z)), ramp = smoothstep((z_join - z) / ramp_cm), so the column keeps the
    raw bell's own slope and flare, only larger or smaller. k is the smallest factor that puts every inner vertex of the column (where ramp >= 0.6) at the skin
    (outermost hit on the horizontal ray) + offset + margin, and at least 1 (never tucked in), except in the front sector (front_deg: full, fade-out deg off -y)
    where it is fitted to skin + front_gap cm both ways: the front panel hangs that far off the chest instead of tenting. The column factors are dilated +-6 deg
    and blurred over azimuth; both layers move alike. Returns the per-vertex move (cm, numpy) and the factor range."""
    X = np.array([v.co for v in bm.verts]); X0 = X.copy()
    vin = np.array(sorted({v.index for f, i in zip(bm.faces, inner) if i for v in f.verts}))
    th = np.degrees(np.arctan2(X[:, 1] - axis_c[1], X[:, 0] - axis_c[0])) % 360
    r = np.hypot(X[:, 0] - axis_c[0], X[:, 1] - axis_c[1])
    ramp = np.array([smoothstep((z_join - z) / ramp_cm) for z in X[:, 2]])
    nt = int(360 / dth); ti = (th / dth).astype(int) % nt
    kb = np.ones(nt); kf = np.full(nt, -np.inf)
    for i in vin[(ramp[vin] >= 0.6) & (X[vin, 2] <= z_join - fit_below)]:
        d = Vector((X[i, 0] - axis_c[0], X[i, 1] - axis_c[1], 0.0))
        if d.length < 1e-6:
            continue
        d.normalize()
        h = skin.ray_cast(Vector(X[i]) + d * 80, -d, 80 + 30)
        if h[0] is None:
            continue
        rs = Vector((h[0].x - axis_c[0], h[0].y - axis_c[1], 0.0)).length
        kb[ti[i]] = max(kb[ti[i]], 1 + ((rs + offset_at(X[i, 2]) + margin) / r[i] - 1) / ramp[i])
        kf[ti[i]] = max(kf[ti[i]], 1 + ((rs + max(front_gap, offset_at(X[i, 2]) + margin)) / r[i] - 1) / ramp[i])
    kf = np.where(np.isfinite(kf), kf, kb)
    ang = (np.arange(nt) + 0.5) * dth
    off = np.abs((ang - 270 + 180) % 360 - 180)  # degrees off the front (-y)
    w = np.array([1 - smoothstep((o - front_deg[0]) / max(1e-6, front_deg[1] - front_deg[0])) for o in off])
    s = int(round(6 / dth))  # dilate + blur the clearance factors; the front fit is only blurred, so its neighbours do not push the front panel out
    kb = circ_blur(np.max([np.roll(kb, j) for j in range(-s, s + 1)], axis=0)[None, :], s)[0]
    k = np.minimum(w * circ_blur(kf[None, :], s)[0] + (1 - w) * kb, k_max)
    kv = np.interp(th, ang, k, period=360)
    X[:, :2] = axis_c[:2] + (X[:, :2] - axis_c[:2]) * (1 + (kv - 1) * ramp)[:, None]
    for v, x in zip(bm.verts, X):
        v.co = Vector(x)
    return X - X0, (float(k.min()), float(k.max()), float(k[int(270 / dth)]))


def fold_limit(bm, axis_c, z_join, ramp_cm, amp, dth=2.0, dz=0.5, sigma_deg=20.0):
    """Round 5: soft, even flutes instead of a few deep box folds. Per (height, azimuth) bin the outer radius is split into a smooth bell (blurred over sigma_deg
    of azimuth) and the fold depth around it; the depth is compressed as amp * tanh(depth / amp), so folds well under amp keep their shape and the deepest ones
    (centre front and back) come down to about amp. Applied as a blurred radial move to both layers below the neck band. Returns the per-vertex move (cm)."""
    X = np.array([v.co for v in bm.verts]); X0 = X.copy()
    th = np.degrees(np.arctan2(X[:, 1] - axis_c[1], X[:, 0] - axis_c[0])) % 360
    r = np.hypot(X[:, 0] - axis_c[0], X[:, 1] - axis_c[1])
    z0 = X[:, 2].min(); nz = int((X[:, 2].max() - z0) / dz) + 2; nt = int(360 / dth)
    zi = ((X[:, 2] - z0) / dz).astype(int); ti = (th / dth).astype(int) % nt
    R = np.full((nz, nt), -np.inf)
    np.maximum.at(R, (zi, ti), r)
    ang = np.arange(nt)
    delta = np.zeros((nz, nt))
    for zb in range(nz):
        ok = np.isfinite(R[zb])
        if ok.sum() < nt // 3:
            continue
        row = np.interp(ang, ang[ok], R[zb][ok], period=nt)
        dev = row - circ_blur(row[None, :], int(round(sigma_deg / dth)))[0]
        delta[zb] = amp * np.tanh(dev / amp) - dev
    delta = box_blur(np.pad(circ_blur(delta, 1), [(4, 4), (0, 0)], mode='edge'), 2, axes=(0,))[4:-4]
    d = bin_lerp(delta, X[:, 2], th, z0, dz, dth) * np.array([smoothstep((z_join - z) / ramp_cm) for z in X[:, 2]])
    u = (X[:, :2] - axis_c[:2]) / np.maximum(r, 1e-6)[:, None]
    X[:, :2] += u * d[:, None]
    for v, x in zip(bm.verts, X):
        v.co = Vector(x)
    return X - X0


def neck_band(bm, inner, axis_c, zb, height, width):
    """Round 5: a raised band where the hood meets the cape (round 4's join was invisible in game): the outer layer bulges out by `height` cm in a Gaussian
    `width` cm tall around z = zb, all the way round. Returns the per-vertex move (cm)."""
    X = np.array([v.co for v in bm.verts]); X0 = X.copy()
    vin = {v.index for f, i in zip(bm.faces, inner) if i for v in f.verts}
    outer = np.array([v.index not in vin for v in bm.verts])
    r = np.hypot(X[:, 0] - axis_c[0], X[:, 1] - axis_c[1])
    d = height * np.exp(-((X[:, 2] - zb) / width) ** 2) * outer
    X[:, :2] += (X[:, :2] - axis_c[:2]) / np.maximum(r, 1e-6)[:, None] * d[:, None]
    for v, x in zip(bm.verts, X):
        v.co = Vector(x)
    return X - X0


def push_inner(bm, inner, skin, offset_at, fade_cm, loops, margin, radii, radial_at, axis_c):
    """Shrinkwrap with outside semantics on the inner layer: an inner vertex needs what it cuts into the skin plus what it lacks of the region offset (the offset part
    fades to 0 within fade_cm of an opening). The needs (zero where the cloth already clears) become a spatially smoothed move field for ALL vertices, one pass per
    entry of `radii` (cm, wide first: a cape is lifted over a shoulder as one drape; then narrower passes push the residual). Where radial_at(z) is true (a loose cape)
    the wide passes measure along the horizontal ray from the slot axis instead of the skin normal, so the cape widens like cloth hung from the neck, folds kept.
    Returns the per-vertex move vectors (cm, numpy) and the number of inner verts inside the skin before the push."""
    bm.verts.ensure_lookup_table()
    vin = np.array(sorted({v.index for f, i in zip(bm.faces, inner) if i for v in f.verts}))
    rim = KDTree(sum(len(l) for l in loops))
    for l in loops:
        for i in l:
            rim.insert(bm.verts[i].co, i)
    rim.balance()
    fade = np.array([smoothstep(rim.find(bm.verts[i].co)[2] / fade_cm) if fade_cm > 0 else 1.0 for i in vin])
    X = np.array([v.co for v in bm.verts]); X0 = X.copy(); cut = None
    for it, rad in enumerate(radii):
        need = np.zeros((len(vin), 3)); k_cut = 0
        for k, (f_, i) in enumerate(zip(fade, vin)):
            p = Vector(X[i])
            if it < len(radii) - 2 and radial_at(p.z):
                nr = Vector((p.x - axis_c[0], p.y - axis_c[1], 0.0))
                if nr.length < 1e-6:
                    continue
                nr.normalize()
                h = skin.ray_cast(p + nr * 80, -nr, 80 + 30)  # the outermost skin on the horizontal ray through the vertex
                if h[0] is None:
                    continue
                sd = (p - h[0]).dot(nr)
            else:
                loc, nr, _f, d = skin.find_nearest(p)
                if loc is None or d > 30:
                    continue
                sd = (p - loc).dot(nr)
            k_cut += sd < 0
            need[k] = np.array(nr) * (max(0.0, -sd) + f_ * max(0.0, offset_at(p.z) + margin - max(sd, 0.0)))  # margin: faces between pushed vertices sag toward a convex skin
        cut = k_cut if cut is None else cut
        if np.linalg.norm(need, axis=1).max() < 0.02:
            break
        X += smooth_field(X[vin], need, X, max(0.5, rad / 2), rad)
    for v, x in zip(bm.verts, X):
        v.co = Vector(x)
    return X - X0, cut


# ---------- checks ----------

def hits(tree, p, d):
    n = 0; p = p.copy()
    while n < 50:
        h = tree.ray_cast(p, d)
        if h[0] is None:
            return n
        n += 1; p = h[0] + d * 1e-4
    return n


def clearance(tree, skin_bm, rest, region, need):
    """Per region: min and median gap (cm) from the skin to the cloth, skin verts closer than their need (cm, per vertex), skin verts inside the cloth solid (odd ray
    parity along three directions). Regions and needs come from the rest-pose skin (`rest`, same vertex order), so a posed body keeps its regions."""
    res = {}
    for name in set(region):
        gaps, short, inside = [], [], []
        for v, p0, rg, nd in zip(skin_bm.verts, rest, region, need):
            if rg != name:
                continue
            loc, _n, _i, d = tree.find_nearest(v.co)
            if loc is None or d > 15:
                continue
            gaps.append(d)
            if d < nd:
                short.append(p0)
            if all(hits(tree, v.co, Vector(dv)) % 2 for dv in ((0, 0, 1), (0.6, 0.8, 0), (-0.6, 0.8, 0))):
                inside.append(p0)
        for what, pts in (('closer than its need', short), ('inside the cloth', inside)):
            if pts:
                W_ = np.array(pts)
                print(f'  {name}: {len(pts)} skin verts {what}, rest x {W_[:, 0].min():.0f}..{W_[:, 0].max():.0f} y {W_[:, 1].min():.0f}..{W_[:, 1].max():.0f} z {W_[:, 2].min():.0f}..{W_[:, 2].max():.0f}')
        g = sorted(gaps)
        res[name] = (g[0], g[len(g) // 2], len(short), len(inside)) if g else (float('nan'), float('nan'), 0, 0)
    return res


def pose(arm, name):
    """A-pose is the rest pose. arms_down: upper arms straight down. crouch: spine bent 30 deg forward, thighs up 70 deg, knees 110 deg (world-space rotations)."""
    for pb in arm.pose.bones:
        pb.matrix_basis.identity()
    bpy.context.view_layer.update()
    def rot(bone, R):
        pb = arm.pose.bones[bone]; h = pb.head.copy()
        pb.matrix = Matrix.Translation(h) @ R.to_4x4() @ Matrix.Translation(-h) @ pb.matrix
        bpy.context.view_layer.update()
    if name == 'arms_down':
        for s in 'lr':  # aim at the elbow: imported bone tails point along +Y, not down the arm
            up = arm.pose.bones['upperarm_' + s]
            rot(up.name, (arm.pose.bones['lowerarm_' + s].head - up.head).normalized().rotation_difference(Vector((0, 0, -1))).to_matrix())
    elif name == 'crouch':
        for nm in ('spine_01', 'spine_03', 'spine_05'):
            rot(nm, Matrix.Rotation(math.radians(10), 3, 'X'))
        for s in 'lr':
            rot('thigh_' + s, Matrix.Rotation(math.radians(-70), 3, 'X'))
            rot('calf_' + s, Matrix.Rotation(math.radians(110), 3, 'X'))


def main():
    a = parse()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if a.measure:
        return measure(a)
    cfg = a.cfg; lim = cfg['limits']; axis = cfg['axis']; k = AX[axis]
    body, arm = load_body(a)
    bb = body_bm(body)
    amask = arm_mask(body, cfg)
    bt = without_arms(bb, amask)
    hb = load_head(a.head)
    real = merged(bt, hb)  # clearance checks: body + the real head
    hh = hb.copy()  # push collision: the head mesh, above hull_above_cm its convex hull (ears and nose are detail a hood drapes over; the hull keeps the cloth from
    if 'hull_above_cm' in cfg:  # dimpling). Only the head: a hull of the whole head mesh takes in its shoulder bib, a cone that pushed the neck band out by 9 cm
        hull = hb.copy()
        bmesh.ops.delete(hull, geom=[v for v in hull.verts if v.co.z < cfg['hull_above_cm']], context='VERTS')
        rh = bmesh.ops.convex_hull(hull, input=hull.verts[:], use_existing_faces=False)
        bmesh.ops.delete(hull, geom=rh['geom_interior'] + rh['geom_unused'], context='VERTS')
        hh = merged(hb, hull)
    if cfg.get('collision_pose'):  # round 5: the cape hangs from the clavicles, not the arms, so it is fitted to the body as it stands in game (arms down, arms
        pose(arm, cfg['collision_pose'])  # included); the raised A-pose deltoids pushed round 4's shoulders 11 cm out into a flat yoke
        skin = BVHTree.FromBMesh(merged(body_bm(body, bpy.context.evaluated_depsgraph_get()), hh))
        pose(arm, 'a_pose')
    else:
        skin = BVHTree.FromBMesh(merged(bt, hh))
    head_c = sum((v.co for v in hb.verts), Vector()) / len(hb.verts)
    regs = sorted(cfg['regions'], key=lambda r: r.get('from', -1e9))
    blend = cfg['region_blend_cm']
    def offset_at(h, an=None):  # cm: region offset along the slot axis, blended over region_blend_cm across each region start
        if an and 'region' in an:
            return next(r for r in regs if r['name'] == an['region'])['offset_mm'] / 10
        off = regs[0]['offset_mm'] / 10
        for r in regs[1:]:
            off += (r['offset_mm'] / 10 - off) * smoothstep((h - r['from'] + blend / 2) / blend)
        return off

    # 1 import, weld, PCA pre-align
    t0 = time.time()
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.abspath(a.garment))
    new = [o for o in set(bpy.data.objects) - before if o.type == 'MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for o in new:
        o.parent = None; o.select_set(True)
    bpy.context.view_layer.objects.active = new[0]
    bpy.ops.object.join()
    hi = bpy.context.view_layer.objects.active; hi.name = 'hi'
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in [o for o in bpy.data.objects if o.type == 'EMPTY']:
        bpy.data.objects.remove(o)
    bm = bmesh.new(); bm.from_mesh(hi.data)
    nraw = len(bm.faces)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=a.weld)
    specks = drop_specks(bm)
    bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=12)  # pinholes in the closed solid (slits in round 2)
    bm.normal_update(); bm.verts.ensure_lookup_table(); bm.faces.ensure_lookup_table()
    print(f'WELD {len(bm.verts)} verts; dropped {specks} loose specks; boundary edges {sum(e.is_boundary for e in bm.edges)}')
    c = pca_align(bm, axis)

    # 2 inner layer and openings
    inner = inner_faces(bm, axis, c)
    loops = boundary_loops(bm, inner)
    P = np.array([v.co for v in bm.verts])
    iv = sorted({v.index for f, i in zip(bm.faces, inner) if i for v in f.verts})
    print(f'INNER {sum(inner)} of {len(inner)} faces; openings (boundary loops, verts) {[len(l) for l in loops[:6]]}')

    # 3 fit
    remap = {v: n for n, v in enumerate(iv)}
    s, t, res = solve_fit(P[iv], cfg, [[remap[i] for i in l if i in remap] for l in loops[:cfg.get('loops', 2)]], offset_at)
    for v in bm.verts:
        v.co = Vector(np.array(v.co) * s + t)
    bm.normal_update()
    worst = max(abs(r) for _n, r in res)
    print(f"FIT scale x{s[0]:.2f} y{s[1]:.2f} z{s[2]:.2f} cm/unit (anisotropy {max(s) / min(s):.3f}); residual mm " + ' '.join(f'{n} {r:+.1f}' for n, r in res))
    lo_h = min(v.co[k] for v in bm.verts)
    print(f'FIT hem at {axis} {lo_h:.1f} cm, top {max(v.co[k] for v in bm.verts):.1f} cm')
    fitted = bm.copy()  # the raw shape in the body frame: the reference for the drift check
    dump(a, '1_fit', bm)

    # 4 push-out of the inner layer
    axis_c = np.array(next((an['centre'] for an in cfg['anchors'] if an['kind'] == 'ring' and an['garment'] == 'waist'), [0, 0, 0]), dtype=float)  # the slot axis passes the neck
    radial_at = lambda z: next(r for r in reversed(regs) if z >= r.get('from', -1e9)).get('push') == 'radial'
    X_lv = np.array([v.co for v in bm.verts])
    z_join = cfg.get('level_hem_below_cm', 1e9)  # the hood-to-cape join (neck band): the cape stages work below it
    if 'level_hem_below_cm' in cfg:
        before_h, after_h = level_hem(bm, axis_c, z_join, cfg.get('hem_cm'), cfg.get('hem_stretch_blend_cm', 0.0), cfg.get('hem_bin_deg', 3.0), cfg.get('hem_blur_deg', 8.0),
                                     loops[0] if cfg.get('hem_from_loop') else None)
        print(f'HEM level: lowest point per azimuth {before_h[0]:.1f}..{before_h[1]:.1f} cm -> {after_h[0]:.1f}..{after_h[1]:.1f} cm')
        bm.normal_update()
        dump(a, '1b_level', bm)
    level = np.array([v.co for v in bm.verts]) - X_lv
    margin = cfg.get('push_margin_mm', 0) / 10
    dr = cfg['drape']
    drape, kr = drape_scale(bm, inner, skin, offset_at, margin, axis_c, z_join, dr['ramp_cm'], dr['front_gap_cm'], dr['front_deg'], dr['k_max'], dr['fit_below_cm'])
    dmv = np.linalg.norm(drape, axis=1)
    print(f'DRAPE column scale {kr[0]:.2f}..{kr[1]:.2f} (front {kr[2]:.2f}); moved {int((dmv > 0.1).sum())} verts, median {np.median(dmv[dmv > 0.1]) * 10 if (dmv > 0.1).any() else 0:.1f} mm, max {dmv.max() * 10:.1f} mm')
    dump(a, '2a_drape', bm)
    folds = fold_limit(bm, axis_c, z_join, dr['ramp_cm'] * 2, cfg['fold_amp_cm']) if 'fold_amp_cm' in cfg else np.zeros_like(drape)
    fmv = np.linalg.norm(folds, axis=1)
    print(f'FOLDS depth limited to ~{cfg.get("fold_amp_cm", 0)} cm: max move {fmv.max() * 10:.1f} mm, {int((fmv > 0.5).sum())} verts over 5 mm')
    dump(a, '2b_folds', bm)
    acc, cut = push_inner(bm, inner, skin, offset_at, cfg['opening_fade_cm'], loops[:cfg.get('loops', 2)], margin,
                          cfg['push_radii_cm'], radial_at, axis_c)
    nb = cfg.get('neck_band')
    band = neck_band(bm, inner, axis_c, nb['z_cm'], nb['height_mm'] / 10, nb['width_cm']) if nb else np.zeros_like(drape)
    acc = acc + drape + level + folds + band
    lo_h = min(v.co[k] for v in bm.verts)
    Xg = np.array([v.co for v in bm.verts]); rg = np.hypot(Xg[:, 0] - axis_c[0], Xg[:, 1] - axis_c[1])
    fr = np.abs((np.degrees(np.arctan2(Xg[:, 1] - axis_c[1], Xg[:, 0] - axis_c[0])) - 270 + 180) % 360 - 180) < 8
    gaps = []
    for zz in (126, 136, 146):  # front panel gap to the body at three heights (cloth inner radius minus the body's on the -y ray)
        hit = skin.ray_cast(Vector((axis_c[0], axis_c[1] - 80, zz)), Vector((0, 1, 0)), 80)[0]
        sel = fr & (np.abs(Xg[:, 2] - zz) < 1)
        if hit is not None and sel.any():
            gaps.append(f'z{zz} {rg[sel].min() - (axis_c[1] - hit.y):.1f}')
    print(f'FRONT gap to the body, cm: {", ".join(gaps)}')
    print(f'HEM final lowest point {lo_h:.1f} cm; push moved max {np.linalg.norm(acc - drape - level - folds - band, axis=1).max() * 10:.1f} mm')
    mv = np.linalg.norm(acc, axis=1).tolist()
    bm.normal_update()
    moved = sorted(x for x in mv if x > 0.1)  # moved: more than 1 mm
    med = moved[len(moved) // 2] * 10 if moved else 0.0
    t_fit = time.time() - t0
    print(f'PUSH inner verts inside the skin before the push {cut}; moved {len(moved)} of {len(mv)} verts, median move {med:.1f} mm, max {max(mv) * 10:.1f} mm at {tuple(round(x) for x in bm.verts[int(np.argmax(mv))].co)}; fit {t_fit:.1f} s')
    if med > lim['median_move_mm']:
        print(f"FLAG median move {med:.1f} mm above {lim['median_move_mm']} mm")
    dump(a, '2_push', bm)
    bm.to_mesh(hi.data); hi.data.update()
    pushed_kd = KDTree(len(bm.verts))
    for v in bm.verts:
        pushed_kd.insert(v.co, v.index)
    pushed_kd.balance()
    pushed = bm

    # 5 decimate only if the budget needs it (collapse, triangulated: keeps both layers of a closed solid)
    low = hi.copy(); low.data = hi.data.copy(); low.name = 'low'
    bpy.context.scene.collection.objects.link(low)
    bpy.ops.object.select_all(action='DESELECT'); bpy.context.view_layer.objects.active = low; low.select_set(True)
    low.data.materials.clear()
    ntri = sum(len(p.vertices) - 2 for p in low.data.polygons)
    if ntri > cfg['tris']:
        src = low.data.copy(); ratio = cfg['tris'] / ntri
        for _ in range(3):
            low.data = src.copy()
            m = low.modifiers.new('dec', 'DECIMATE'); m.ratio = ratio; m.use_collapse_triangulate = True
            bpy.ops.object.modifier_apply(modifier='dec')
            n = len(low.data.polygons)
            if abs(n - cfg['tris']) < 0.03 * cfg['tris']:
                break
            ratio = min(1.0, ratio * cfg['tris'] / n)
    if low.data.has_custom_normals:  # the glTF's split normals ride along through push and decimation and no longer match the surface: shading dents and lumps
        bpy.ops.mesh.customdata_custom_splitnormals_clear()
    bml = bmesh.new(); bml.from_mesh(low.data)
    bmesh.ops.recalc_face_normals(bml, faces=bml.faces[:])  # outward for the closed double wall (round 3's centre heuristic cancelled out over the two walls: inside out)
    bml.normal_update()
    # round 5: the clay specks were faces facing against all their neighbours (recalc cannot orient across the 22 non-manifold edges): flip them back
    flip = [f for f in bml.faces if sum((g.normal for e in f.edges for g in e.link_faces if g is not f), Vector()).dot(f.normal) < 0]
    bmesh.ops.reverse_faces(bml, faces=flip)
    bml.normal_update()
    sm = cfg.get('inner_smooth')
    if sm:  # round 5: the inner cape surface carried the raw mesh's small bumps; a few Laplacian passes on the inner-layer cape faces only (openings stay put)
        inl = inner_faces(bml, axis, Vector((axis_c[0], axis_c[1], 0.0)))
        bml.faces.index_update()
        sv = list({v for f, i in zip(bml.faces, inl) if i for v in f.verts if v.co.z < z_join - 2 and not v.is_boundary})
        sv = [v for v in sv if all(inl[g.index] for g in v.link_faces)]  # not the rim verts shared with the outer layer
        c0 = [v.co.copy() for v in sv]
        for _ in range(sm['iters']):
            bmesh.ops.smooth_vert(bml, verts=sv, factor=sm['factor'], use_axis_x=True, use_axis_y=True, use_axis_z=True)
        for v, c in zip(sv, c0):  # bumps only: no vertex moves more than max_mm
            d = v.co - c
            if d.length > sm['max_mm'] / 10:
                v.co = c + d * (sm['max_mm'] / 10 / d.length)
        bml.normal_update()
        print(f'INNER SMOOTH {len(sv)} inner cape verts x{sm["iters"]} (factor {sm["factor"]}); flipped back {len(flip)} faces facing against their neighbours')
    print(f'LOW tris {len(bml.faces)} (raw {nraw}); boundary edges {sum(e.is_boundary for e in bml.edges)}, non-manifold {sum(not e.is_manifold and not e.is_boundary for e in bml.edges)}')
    inner_low = np.array(inner_faces(bml, axis, Vector((axis_c[0], axis_c[1], 0.0))), dtype=np.int32)  # for the inner albedo lift (step 6)
    bml.verts.index_update(); bml.verts.ensure_lookup_table()
    face_loop = max(boundary_loops(bml, inner_low), key=lambda l: np.mean([bml.verts[i].co.z for i in l]))  # the face opening: the highest opening
    kd_rim = KDTree(len(face_loop))
    for i in face_loop:
        kd_rim.insert(bml.verts[i].co, i)
    kd_rim.balance()
    rim_low = np.array([1 - smoothstep(kd_rim.find(v.co)[2] / cfg.get('rim_cm', 4.0)) for v in bml.verts])  # 1 at the face-opening rim, 0 from rim_cm away
    bml.to_mesh(low.data); low.data.update()

    # shape drift: (a) where the push moved the cloth less than 1 mm, the final surface against the raw (fitted) one; (b) everywhere, the final surface against the
    # pushed one (what decimation and cleanup changed beyond the intended push). Both ways (final->ref and ref->final), mm.
    def dist(a_bm, b_bm, keep=lambda i: True):
        tb = BVHTree.FromBMesh(b_bm)
        return np.array([tb.find_nearest(v.co)[3] * 10 for i, v in enumerate(a_bm.verts) if keep(i)])
    bml.verts.ensure_lookup_table()
    ra = dist(bml, fitted, lambda i: mv[pushed_kd.find(bml.verts[i].co)[1]] < 0.1)
    ra2 = dist(fitted, bml, lambda i: mv[i] < 0.1 and i % 2 == 0)
    rb = dist(bml, pushed); rb2 = dist(pushed, bml, lambda i: i % 2 == 0)
    drift = (float(max(ra.mean(), rb.mean())), float(max(ra.max(), ra2.max(), rb.max(), rb2.max())))
    print(f'DRIFT unmoved region ({len(ra)} final verts): mean {ra.mean():.2f} max {max(ra.max(), ra2.max()):.2f} mm; whole garment vs the pushed raw mesh: mean {rb.mean():.2f} max {max(rb.max(), rb2.max()):.2f} mm')
    bml.free(); pushed.free()

    fails = []
    if worst > lim['ring_residual_mm']:
        fails.append(f'ring residual {worst:.1f} mm')
    if drift[0] > lim['drift_mean_mm'] or drift[1] > lim['drift_max_mm']:
        fails.append('shape drift')

    # 6 unwrap + bake the Forge colour (hi keeps its glTF material and UVs, pushed to the same place), grade, twill weave
    while low.data.uv_layers:
        low.data.uv_layers.remove(low.data.uv_layers[0])
    low.data.uv_layers.new(name='UVMap')
    bpy.ops.object.select_all(action='DESELECT'); bpy.context.view_layer.objects.active = low; low.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(75), island_margin=0.004, area_weight=0.7)
    bpy.ops.object.mode_set(mode='OBJECT')
    name = cfg['item'] + a.tag
    img = bpy.data.images.new(f'T_{name}_BaseColor', a.tex, a.tex)
    mat = bpy.data.materials.new(f'M_{name}'); mat.use_nodes = True
    node = mat.node_tree.nodes.new('ShaderNodeTexImage'); node.image = img
    mat.node_tree.nodes.active = node
    low.data.materials.append(mat)
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 4
    sc.render.bake.use_pass_direct = sc.render.bake.use_pass_indirect = False
    sc.render.bake.use_selected_to_active = True
    sc.render.bake.cage_extrusion = 0.15; sc.render.bake.max_ray_distance = 0.3; sc.render.bake.margin = 16
    bpy.ops.object.select_all(action='DESELECT'); hi.select_set(True); low.select_set(True); bpy.context.view_layer.objects.active = low
    bpy.ops.object.bake(type='DIFFUSE')
    out = os.path.abspath(a.out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    px = np.array(img.pixels[:], dtype=np.float32).reshape(-1, 4)
    lum = px[:, :3].mean(axis=1)
    empty = lum < 0.4 * np.median(lum[lum > 1e-6])  # unbaked texels and the dark 1 px rim the bake leaves on every island
    mean = px[~empty, :3].mean(axis=0)
    px[:, :3] = np.clip(px[:, :3] * (np.array(WOOL_SRGB) / mean), 0, 1)  # the mean becomes the wool colour, the generated mottling is kept
    px[empty, :3] = np.array(WOOL_SRGB)
    n_ = a.tex
    yy, xx = np.mgrid[0:n_, 0:n_]
    # round 4: a fine, low-contrast slub weave (round 3's 24 px twill at +-28% read as wide corduroy stripes): a 4 px tabby (about 3 mm a repeat at the
    # hood's texel density) at +-3%, and slub = uneven yarn thickness, noise stretched along the weft (31 px) and across it (3 px), at +-6%
    tabby = np.sin(2 * np.pi * xx / 4.0) * np.sin(2 * np.pi * yy / 4.0)
    rng = np.random.default_rng(7)
    slub = box_blur(box_blur(rng.normal(0, 1, (n_, n_)), 5, axes=(1,)), 1, axes=(0,))
    slub = np.clip(slub / slub.std(), -2.5, 2.5)
    wv = cfg.get('weave', {'tabby': 0.03, 'slub': 0.06})  # round 5: the tabby a little stronger (round 4's +-3% read as felt at play distance)
    px[:, :3] = np.clip(px[:, :3] * (1.0 + (wv['tabby'] * tabby + wv['slub'] * slub).reshape(-1, 1)), 0, 1)
    # masks baked onto the unwrap in one emission bake: R inner-layer faces, G face-opening rim (rim_low), B height about the neck band (+-4 cm -> 0..1)
    mask = bpy.data.images.new('masks', n_, n_, float_buffer=True)
    ca = low.data.color_attributes.new('mask', 'FLOAT_COLOR', 'CORNER')
    li = np.zeros(len(low.data.loops), np.int32); low.data.loops.foreach_get('vertex_index', li)
    lp = np.zeros(len(low.data.loops), np.int32)
    for p_ in low.data.polygons:
        lp[p_.loop_start:p_.loop_start + p_.loop_total] = p_.index
    cols = np.c_[inner_low[lp], rim_low[li], np.zeros(len(li)), np.ones(len(li))].astype(np.float32)
    ca.data.foreach_set('color', cols.ravel())
    zb = cfg['neck_band']['z_cm'] if 'neck_band' in cfg else 0.0
    m = bpy.data.materials.new('masks'); m.use_nodes = True; nt = m.node_tree
    at = nt.nodes.new('ShaderNodeAttribute'); at.attribute_name = 'mask'
    sep = nt.nodes.new('ShaderNodeSeparateColor'); nt.links.new(at.outputs['Color'], sep.inputs[0])
    tc = nt.nodes.new('ShaderNodeTexCoord'); xyz = nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(tc.outputs['Object'], xyz.inputs[0])
    mp = nt.nodes.new('ShaderNodeMapRange'); mp.inputs['From Min'].default_value = zb - 4; mp.inputs['From Max'].default_value = zb + 4
    nt.links.new(xyz.outputs['Z'], mp.inputs['Value'])
    cmb = nt.nodes.new('ShaderNodeCombineColor')
    nt.links.new(sep.outputs[0], cmb.inputs[0]); nt.links.new(sep.outputs[1], cmb.inputs[1]); nt.links.new(mp.outputs['Result'], cmb.inputs[2])
    em = nt.nodes.new('ShaderNodeEmission'); nt.links.new(cmb.outputs[0], em.inputs['Color'])
    nt.links.new(em.outputs[0], nt.nodes['Material Output'].inputs[0])
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = mask; nt.nodes.active = t
    low.data.materials.clear(); low.data.materials.append(m)
    sc.render.bake.use_selected_to_active = False
    bpy.ops.object.select_all(action='DESELECT'); low.select_set(True); bpy.context.view_layer.objects.active = low
    bpy.ops.object.bake(type='EMIT')
    mk = np.array(mask.pixels[:], dtype=np.float32).reshape(-1, 4)
    low.data.materials.clear(); low.data.materials.append(mat)
    low.data.color_attributes.remove(low.data.color_attributes['mask'])
    lift = cfg.get('inner_albedo_lift', 1.0)  # round 4: the hood interior only gets bounce light in game and read black; a lighter inner wall reads
    px[:, :3] = np.clip(px[:, :3] * (1.0 + (lift - 1.0) * mk[:, :1]), 0, 1)
    rl = cfg.get('rim_albedo_lift', 1.0)  # round 5: the face-opening rim read as a plain dark hole
    px[:, :3] = np.clip(px[:, :3] * (1.0 + (rl - 1.0) * mk[:, 1:2]), 0, 1)
    print(f'INNER LIFT x{lift} on {int(inner_low.sum())} faces, {float(mk[:, 0].mean()) * 100:.0f}% of texels; RIM LIFT x{rl} on {float((mk[:, 1] > 0.5).mean()) * 100:.1f}% of texels')
    height = wv.get('tabby_px', 0.0) * tabby + wv.get('slub_px', 0.0) * slub  # texels, for the normal map
    if 'neck_band' in cfg:  # round 5: the band in the colour and normal map too, outer layer only: a raised ridge with a dark stitch line under it
        nb = cfg['neck_band']; dzb = (mk[:, 2] - 0.5) * 8.0; out_ = 1.0 - mk[:, 0]
        ridge = np.exp(-(dzb / nb['width_cm']) ** 2) * out_
        seam = np.exp(-((dzb + 1.3 * nb['width_cm']) / 0.25) ** 2) * out_
        px[:, :3] = np.clip(px[:, :3] * (1.0 + nb.get('ridge_lift', 0.08) * ridge - nb.get('seam_dark', 0.35) * seam)[:, None], 0, 1)
        height = height + (nb.get('ridge_px', 3.0) * ridge - nb.get('seam_px', 1.5) * seam).reshape(n_, n_)
    gy, gx = np.gradient(height)  # rows run up the image (v), columns along u
    nrm = np.stack([-gx, gy, np.ones_like(gx)], -1)  # DirectX green (Unreal): +dH/dv
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    nimg = bpy.data.images.new(f'T_{name}_Normal', n_, n_); nimg.colorspace_settings.name = 'Non-Color'
    nimg.pixels.foreach_set(np.c_[(nrm.reshape(-1, 3) + 1) / 2, np.ones(n_ * n_)].astype(np.float32).ravel())
    nimg.filepath_raw = os.path.join(os.path.dirname(out), f'T_{name}_Normal.png'); nimg.file_format = 'PNG'; nimg.save()
    print(f'NORMAL {nimg.filepath_raw}: tabby {wv.get("tabby_px", 0)} px, slub {wv.get("slub_px", 0)} px, mean tilt {math.degrees(float(np.arccos(nrm[..., 2]).mean())):.1f} deg')
    px[:, 3] = 1.0
    img.pixels.foreach_set(px.ravel())
    img.filepath_raw = os.path.join(os.path.dirname(out), f'T_{name}_BaseColor.png'); img.file_format = 'PNG'; img.save()
    print(f'BAKED {img.filepath_raw} mean {tuple(round(float(x), 3) for x in mean)} -> {WOOL_SRGB}')
    bpy.data.objects.remove(hi)
    low.name = f'SK_{name}'
    for poly in low.data.polygons:
        poly.use_smooth = True
    tris = sum(len(p.vertices) - 2 for p in low.data.polygons)

    # see-through: rays from the head centre through the back half-space must all hit the cloth
    tree = BVHTree.FromObject(low, bpy.context.evaluated_depsgraph_get())
    miss = total = 0
    for az in range(-40, 41, 4):
        for el in range(-30, 41, 4):
            d = Vector((math.sin(math.radians(az)) * math.cos(math.radians(el)), math.cos(math.radians(az)) * math.cos(math.radians(el)), math.sin(math.radians(el))))
            total += 1; miss += tree.ray_cast(head_c, d)[0] is None
    print(f'SEETHROUGH {miss} of {total} back rays from the head centre pass through the garment')
    if miss:
        fails.append('see-through')
    fv = [v.co for v in hb.verts if head_c.z - 12 < v.co.z < head_c.z + 8 and v.co.y < head_c.y - 3]
    vis = sum(1 for q in fv if tree.ray_cast(q + Vector((0, -80, 0)), Vector((0, 1, 0)), 80 - 0.3)[0] is None)
    print(f'FACE visible from the front: {100 * vis / len(fv):.0f}% of brow-to-chin verts')

    # 7 skin weights from the nearest body triangle (barycentric), smoothed over the cloth, neck ramp to neck_02/head
    bw = {g.index: g.name for g in body.vertex_groups}
    bvh = BVHTree.FromBMesh(bb)
    bb.faces.ensure_lookup_table()
    bones = arm.data.bones
    zseam = bones['neck_01'].head_local.z; z_head = bones['head'].head_local.z
    keep_arm = cfg.get('arm_weight_keep', 1.0)
    W = {}
    for v in low.data.vertices:
        loc, _n, fi, _d = bvh.find_nearest(v.co)
        f = bb.faces[fi]
        ws = [dict((bw[g.group], g.weight) for g in body.data.vertices[vv.index].groups) for vv in f.verts]
        p0, p1, p2 = (vv.co for vv in f.verts)
        v0, v1, v2 = p1 - p0, p2 - p0, loc - p0
        d00, d01, d11, d20, d21 = v0.dot(v0), v0.dot(v1), v1.dot(v1), v2.dot(v0), v2.dot(v1)
        den = d00 * d11 - d01 * d01 or 1e-12
        bv = (d11 * d20 - d01 * d21) / den; bwt = (d00 * d21 - d01 * d20) / den
        w = {}
        for kk, x in zip(ws, (1 - bv - bwt, bv, bwt)):
            for nm, val in kk.items():
                w[nm] = w.get(nm, 0.0) + val * x
        if keep_arm < 1.0:  # a cape hangs from the shoulders: the swinging arm keeps only part of its pull, the rest goes to that side's clavicle (round 3: the sides opened over the arms when walking)
            for nm in [n for n in w if n.startswith(ARM[1:]) and n[-2:] in ('_l', '_r')]:
                x = w[nm]; w[nm] = x * keep_arm
                w['clavicle' + nm[-2:]] = w.get('clavicle' + nm[-2:], 0.0) + x * (1 - keep_arm)
        root = cfg.get('cape_root_bone')
        if root:  # round 5: the cape hangs from the upper chest; the lower spine, pelvis and legs it lies near must not swing its hem (sides split when walking)
            for nm in [n for n in w if not n.startswith(('clavicle', 'neck', 'head', root) + ARM[1:])]:
                w[root] = w.get(root, 0.0) + w.pop(nm)
            cf = cfg.get('clavicle_fade_cm')  # [top, length]: below the shoulder the clavicle share fades into the root bone, so a shrug or arm swing does not lift the hem (round 5 walk_back)
            if cf:
                f = smoothstep((cf[0] - v.co.z) / cf[1])
                for nm in [n for n in w if n.startswith('clavicle')]:
                    x = w[nm] * f; w[nm] -= x; w[root] = w.get(root, 0.0) + x
        tt =max(0.0, min(1.0, (v.co.z - zseam) / max(1e-6, z_head - zseam)))
        if tt > 0:
            w = {nm: x * (1 - tt) for nm, x in w.items()}
            w['neck_02'] = w.get('neck_02', 0.0) + tt * (1 - tt) * 2.0
            w['head'] = w.get('head', 0.0) + tt * tt
        W[v.index] = w
    nbrs = {v.index: [] for v in low.data.vertices}
    for e in low.data.edges:
        nbrs[e.vertices[0]].append(e.vertices[1]); nbrs[e.vertices[1]].append(e.vertices[0])
    for _ in range(6):  # weights average with the neighbours: panels do not swing on their own
        nxt = {}
        for i, w in W.items():
            acc = dict(w)
            for j in nbrs[i]:
                for nm, x in W[j].items():
                    acc[nm] = acc.get(nm, 0.0) + x / max(1, len(nbrs[i]))
            nxt[i] = {nm: x / 2 for nm, x in acc.items()}
        W = nxt
    vg = {}
    for i, w in W.items():
        top8 = sorted(w.items(), key=lambda kv: -kv[1])[:8]
        tot = sum(x for _, x in top8) or 1.0
        for nm, x in top8:
            if x / tot > 1e-3:
                if nm not in vg:
                    vg[nm] = low.vertex_groups.new(name=nm)
                vg[nm].add([i], x / tot, 'REPLACE')
    low.parent = arm
    mod = low.modifiers.new('Armature', 'ARMATURE'); mod.object = arm

    # clearance and overlaps in each pose (body and cloth deform together)
    fac = lim['min_clearance_factor']
    arm_poses = cfg.get('arm_collision_poses')  # round 5: the whole arm is checked in these poses and left out of the others (the cape does not follow the arms)
    armv = arm_mask(body, dict(cfg, arm_caps_above_cm=1e9, arms_not_collision=True)) + [False] * len(hb.verts) if arm_poses is not None else None
    for ps in cfg['poses']:
        pose(arm, ps)
        dg = bpy.context.evaluated_depsgraph_get()
        hp = hb.copy()  # the head mesh is not skinned: it moves rigidly with neck_01 (the poses do not turn the neck or head bones themselves)
        pb = arm.pose.bones['neck_01']
        hp.transform(arm.matrix_world @ pb.matrix @ pb.bone.matrix_local.inverted() @ arm.matrix_world.inverted())
        sk = merged(body_bm(body, dg) if armv else without_arms(body_bm(body, dg), amask), hp)
        cm = bmesh.new(); cm.from_mesh(low.evaluated_get(dg).to_mesh()); cm.transform(low.matrix_world)
        ct = BVHTree.FromBMesh(cm)
        if ps == cfg['poses'][0]:  # rest: regions (by height; the face itself is open) and needs (min_clearance_factor x the blended offset) per skin vertex
            rest = [v.co.copy() for v in sk.verts]
            skip = lambda p: any(r.get('skip_face') for r in regs) and p.y < head_c.y - 6 and head_c.z - 14 < p.z < head_c.z + 14
            region = [None if skip(p) or p.z < lo_h + 6 else next(r['name'] for r in reversed(regs) if p.z >= r.get('from', -1e9)) for p in rest]
            need0 = [fac * offset_at(p.z) for p in rest]
        reg = region if not armv or ps in arm_poses else [None if am else rg for rg, am in zip(region, armv)]
        cl = clearance(ct, sk, rest, reg, need0 if ps == cfg['poses'][0] else [0.0] * len(rest))  # posed: no skin inside the cloth or touching it
        for r in regs:
            if r['name'] not in cl:
                continue
            mn, md, nshort, ins = cl[r['name']]
            ok = nshort == 0 and ins == 0
            print(f"CLEARANCE {ps} {r['name']}: min {mn * 10:.1f} mm, median {md * 10:.1f} mm, {nshort} skin verts closer than {'0.8 x offset' if ps == cfg['poses'][0] else 'zero'}, {ins} inside the cloth -> {'ok' if ok else 'FAIL'}")
            if not ok:
                fails.append(f"clearance {ps} {r['name']}")
        cm.free(); sk.free()
    pose(arm, 'a_pose')
    print(f'FINAL tris {tris}; fit {t_fit:.1f} s; checks: {"FAIL " + "; ".join(fails) if fails else "all ok"}')
    if fails and not a.skip_checks:
        raise SystemExit('CHECKS FAILED: ' + '; '.join(fails))

    # 8 export (the skeleton is the UE one: no Empty above it, cm)
    unit = sc.unit_settings
    unit.system, unit.length_unit, unit.scale_length = 'METRIC', 'CENTIMETERS', 0.01
    bpy.ops.object.select_all(action='DESELECT')
    bpy.data.objects.remove(body)
    for o in (arm, low):
        o.select_set(True)
    bpy.ops.export_scene.fbx(filepath=out, use_selection=True, object_types={'ARMATURE', 'MESH'}, use_armature_deform_only=False,
                             add_leaf_bones=False, bake_anim=False, mesh_smooth_type='OFF', path_mode='STRIP')
    bpy.ops.wm.save_as_mainfile(filepath=os.path.splitext(out)[0] + '.blend')
    print(f'CLEAN_DONE tris={tris} out={out}')


main()
