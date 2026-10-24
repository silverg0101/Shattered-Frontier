"""GEAR-02: clean an Asset Forge garment mesh (GLB) into a skinned game garment on Orlando's body. Deterministic, no hand edits.

Blender 5.1, headless (all lengths in cm, the body FBX's own units):
  blender --factory-startup -b --python tools/Clean-GeneratedGarment.py -- --garment GameClient/ArtSource/Characters/Player/outfit-asset/hood/hood.glb --out Artifacts/Characters/PlayerBase/hood/hood.fbx
The Forge mesh is a closed, thin (2 to 19 mm) double-layer solid. Decimating that merges the layers and tears the cloth (round 1), so:
1 align (uniform scale + shift so it spans --bottom..--top and sits on the head, --pitch tips it back) and weld;
2 keep only the OUTER layer (a face is outer when the layer behind it is nearer the skin), 3 push cloth that is inside / too close to the skin out along the
skin normal with a smoothed displacement (no faces are deleted: no holes), 4 decimate that single sheet gently to ~--tris/2 faces, smooth the hem edge,
5 one-seam conformal unwrap (hem to face opening over the back), bake the Forge colour onto it, grade it to a dark brown wool and add a coarse weave,
6 solidify --thickness outward (smooth shaded), 7 skin weights from the nearest body triangle (barycentric), smoothed over the cloth, neck ramp to neck_02/head,
8 export FBX + armature (Unreal skeleton, as Build-PlayerGarment.py) + PNG texture + .blend.
"""
import argparse
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.join(ROOT, 'Artifacts/Characters/PlayerBase')
WOOL_SRGB = (0.34, 0.24, 0.155)  # target mean colour of the undyed brown wool (sRGB, 0..1)


def parse():
    p = argparse.ArgumentParser()
    p.add_argument('--garment', required=True)
    p.add_argument('--out', required=True)
    p.add_argument('--body', default=os.path.join(BASE, 'Orlando_body.fbx'))
    p.add_argument('--head', default=os.path.join(BASE, 'Orlando_head.obj'))
    p.add_argument('--top', type=float, default=206.0, help='cm, top of the garment (the hood must clear the skull)')
    p.add_argument('--bottom', type=float, default=128.0, help='cm, lowest point of the cape (mid upper arm)')
    p.add_argument('--yshift', type=float, default=10.0, help='cm, extra shift toward the face (-y)')
    p.add_argument('--pitch', type=float, default=0.0, help='deg about X around the head centre: + tips the hood top forward (-y), - back')
    p.add_argument('--flare', type=float, default=1.15, help='cape radius factor at the hem (1 at the neck): clears the arms instead of pushing the cloth into tubes')
    p.add_argument('--weld', type=float, default=0.08, help='cm: merge vertices closer than this (closes the hairline slit down the back centre; glTF splits UV seams)')
    p.add_argument('--calm', type=float, default=0.8, help='0..1: how far the cape hem flutes and scallops are smoothed toward an even flare')
    p.add_argument('--body-offset', dest='body_offset', type=float, default=2.5, help='cm: clearance of the cape below the neck')
    p.add_argument('--offset', type=float, default=0.8, help='cm: cloth closer than this to the skin (or inside it) is pushed out to it')
    p.add_argument('--thickness', type=float, default=0.4, help='cm of cloth (3-5 mm)')
    p.add_argument('--tris', type=int, default=19000, help='final triangle count, about (item budget up to ~20k)')
    p.add_argument('--tex', type=int, default=2048)
    p.add_argument('--yaw', type=float, default=0.0, help='deg about Z if the garment does not face -Y like the body (Asset Forge GLBs import facing -Y)')
    p.add_argument('--skip-checks', action='store_true', help='diagnose only: export even when the tunnel check fails')
    p.add_argument('--tag', default='')
    return p.parse_args(sys.argv[sys.argv.index('--') + 1:])


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


def body_bm(body):
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bm.transform(body.matrix_world)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.normal_update()
    return bm


def components(bm):
    seen, parts = set(), []
    for f in bm.faces:
        if f in seen:
            continue
        stack, comp = [f], []
        seen.add(f)
        while stack:
            g = stack.pop(); comp.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h not in seen:
                        seen.add(h); stack.append(h)
        parts.append(comp)
    return parts


def drop_specks(bm, frac=0.02):
    parts = components(bm)
    big = max(len(c) for c in parts)
    bmesh.ops.delete(bm, geom=[f for c in parts if len(c) < big * frac for f in c], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    return len(parts)


def calm_hem(bm, ay, z_top, z_hem, strength, radius=9.0):
    """The Forge hem has deep radial flutes and a scalloped edge. Below z_top every vertex moves toward the mean radius (and, in the hem band, mean height) of its
    neighbourhood, more toward the hem. The shift comes from the mean of the 1 cm sandwich around the vertex, so both cloth layers move together and keep their thickness."""
    from mathutils.kdtree import KDTree
    vs = [v for v in bm.verts if v.co.z < z_top]
    kd = KDTree(len(vs))
    pol = []
    for i, v in enumerate(vs):
        kd.insert(v.co, i)
        pol.append((math.hypot(v.co.x, v.co.y - ay), v.co.z))
    kd.balance()
    out = []
    for i, v in enumerate(vs):
        near = [j for _c, j, _d in kd.find_range(v.co, radius)]
        core = [j for _c, j, _d in kd.find_range(v.co, 1.0)]
        mean = lambda ids, k: sum(pol[j][k] for j in ids) / len(ids)
        t = min(1.0, (z_top - v.co.z) / max(1e-6, z_top - z_hem))
        w = strength * t * t * (3 - 2 * t)
        dr = w * (mean(near, 0) - mean(core, 0))
        dz = w * (mean(near, 1) - mean(core, 1)) if v.co.z < z_hem + 10 else 0.0
        r, phi = pol[i][0], math.atan2(v.co.y - ay, v.co.x)
        out.append((r + dr, phi, v.co.z + dz))
    for v, (r, phi, z) in zip(vs, out):
        v.co = Vector((r * math.cos(phi), ay + r * math.sin(phi), z))


def push_out(bm, skin, offset, r_max=1.2, r_avg=3.0):  # offset: cm, or a function of the vertex position
    """Cloth inside or within `offset` of the skin moves out along the skin normal. The displacement is a field in space, not per vertex: every vertex takes the
    largest need within r_max (both cloth layers, 2-19 mm apart, move together and keep their thickness), then the vectors are averaged within r_avg (folds stay folds)."""
    from mathutils.kdtree import KDTree
    vs = list(bm.verts)
    kd = KDTree(len(vs))
    disp = []
    for i, v in enumerate(vs):
        kd.insert(v.co, i)
        loc, n, _i, _d = skin.find_nearest(v.co)
        off = offset(v.co) if callable(offset) else offset
        disp.append(n * max(0.0, off - (v.co - loc).dot(n)) if loc is not None else Vector())
    kd.balance()
    mx = []
    for i, v in enumerate(vs):
        best = max((disp[j] for _c, j, _d in kd.find_range(v.co, r_max)), key=lambda d: d.length)
        mx.append(best)
    moved, top = 0, 0.0
    for i, v in enumerate(vs):
        near = [mx[j] for _c, j, _d in kd.find_range(v.co, r_avg)]
        d = sum(near, Vector()) / len(near)
        if d.length > 1e-4:
            v.co = v.co + d; moved += 1; top = max(top, d.length)
    return moved, top
def main():
    a = parse()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.abspath(a.body))
    body = next(o for o in bpy.data.objects if o.type == 'MESH')
    arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    body.parent = None; arm.parent = None  # the importer's 0.01 Empty: the data itself is cm
    for o in [o for o in bpy.data.objects if o.type == 'EMPTY']:
        bpy.data.objects.remove(o)
    bpy.context.view_layer.update()
    bb = body_bm(body)
    hb = load_head(a.head)
    # one collision BVH: the body mesh (stops at the neck seam) plus the head mesh (neck, bib, face)
    both = bmesh.new()
    for src in (bb, hb):
        tmp = bpy.data.meshes.new('t'); src.to_mesh(tmp); both.from_mesh(tmp); bpy.data.meshes.remove(tmp)
    both.normal_update()
    skin = BVHTree.FromBMesh(both)
    head_z = max(v.co.z for v in hb.verts)
    head_c = sum((v.co for v in hb.verts), Vector()) / len(hb.verts)
    top = a.top

    # 1 import + align
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.abspath(a.garment))
    new = [o for o in set(bpy.data.objects) - before if o.type == 'MESH']
    for o in new:
        o.parent = None
    bpy.ops.object.select_all(action='DESELECT')
    for o in new:
        o.select_set(True)
    bpy.context.view_layer.objects.active = new[0]
    bpy.ops.object.join()
    hi = bpy.context.view_layer.objects.active
    hi.name = 'hi'
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in [o for o in bpy.data.objects if o.type == 'EMPTY']:
        bpy.data.objects.remove(o)
    me = hi.data
    me.transform(Matrix.Rotation(math.radians(a.yaw), 4, 'Z'))
    zs = [v.co.z for v in me.vertices]
    s = (top - a.bottom) / (max(zs) - min(zs))
    xs = [v.co.x for v in me.vertices]; ys = [v.co.y for v in me.vertices]
    cx, cy = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2
    for v in me.vertices:
        v.co = Vector(((v.co.x - cx) * s, (v.co.y - cy) * s + head_c.y + a.yshift, (v.co.z - min(zs)) * s + a.bottom))
    ay = sum(v.co.y for v in bb.verts if 130 < v.co.z < 150) / max(1, sum(1 for v in bb.verts if 130 < v.co.z < 150))  # torso centre
    zn = min(v.co.z for v in hb.verts)  # neck base: no flare above it (the hood), full flare at the hem
    for v in me.vertices:
        k = 1.0 + (a.flare - 1.0) * min(1.0, max(0.0, (zn - v.co.z) / max(1e-6, zn - a.bottom))) ** 1.5
        v.co.x *= k; v.co.y = ay + (v.co.y - ay) * k
    if a.pitch:  # about the x axis through the head centre; + moves the top toward -y (forward)
        rot = Matrix.Translation(head_c) @ Matrix.Rotation(math.radians(-a.pitch), 4, 'X') @ Matrix.Translation(-head_c)
        me.transform(rot)
    print(f'ALIGN scale={s:.2f} size_cm={(max(xs)-min(xs))*s:.1f} x {(max(ys)-min(ys))*s:.1f} x {(top-a.bottom):.1f}')

    bm = bmesh.new(); bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=a.weld)  # glTF splits vertices along UV seams and hard edges
    bm.faces.ensure_lookup_table()
    # fit report on the aligned solid: does the head fit inside the hood?
    tree0 = BVHTree.FromBMesh(bm)
    hv_ = [v.co for v in hb.verts if v.co.z > head_z - 30]
    pen = []
    for v in hv_:
        d = v - head_c; L = d.length
        hit = tree0.ray_cast(head_c, d.normalized(), L)
        pen.append(L - hit[3] if hit[0] is not None else 0.0)
    inside = [x for x in pen if x > 0.3]
    print(f'FITREPORT head verts with the hood wall inside them (>3 mm): {100 * len(inside) / len(pen):.1f}% of {len(pen)}, max {max(pen):.1f} cm')
    zeye = head_c.z + 4
    hy = [v.co.y for v in hb.verts if abs(v.co.z - zeye) < 1.5]
    print(f'FITREPORT Orlando head y {min(hy):.1f}..{max(hy):.1f}; hood front rim y {min(v.co.y for v in bm.verts if abs(v.co.z - zeye) < 12):.1f}')

    # 2 smooth push out of the skin (nothing is deleted: no holes)
    calm_hem(bm, ay, zn - 8, a.bottom, a.calm)
    zn_ = min(v.co.z for v in hb.verts)  # neck base: the hood keeps --offset, the cape below it hangs --body-offset off the body (the walk cycle and the other bodies move the skin by centimetres)
    moved, mx = push_out(bm, skin, lambda p: a.offset + (a.body_offset - a.offset) * min(1.0, max(0.0, (zn_ + 6 - p.z) / 12.0)))
    print(f'PUSHED {moved} cloth verts out of the skin, max {mx:.1f} cm')
    bm.normal_update()
    bm.to_mesh(me); bm.free(); me.update()

    # 3 decimate the closed solid (collapse, triangulated): at ~5x it keeps both layers and the rim, no tears
    low = hi.copy(); low.data = hi.data.copy(); low.name = 'low'
    bpy.context.scene.collection.objects.link(low)
    bpy.ops.object.select_all(action='DESELECT'); bpy.context.view_layer.objects.active = low; low.select_set(True)
    low.data.materials.clear()
    src_mesh = low.data.copy()
    ratio = min(1.0, a.tris / max(1, sum(len(p.vertices) - 2 for p in src_mesh.polygons)))
    for _ in range(3):
        low.data = src_mesh.copy()
        m = low.modifiers.new('dec', 'DECIMATE'); m.ratio = ratio; m.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier='dec')
        n = len(low.data.polygons)
        print(f'RATIO {ratio:.4f} -> tris {n}')
        if abs(n - a.tris) < 0.04 * a.tris:
            break
        ratio = min(1.0, ratio * a.tris / n)
    bm = bmesh.new(); bm.from_mesh(low.data)
    drop_specks(bm)
    bm.normal_update()
    if sum((f.calc_center_median() - head_c).dot(f.normal) for f in bm.faces) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    print(f'LOW tris={len(bm.faces)} boundary edges={sum(e.is_boundary for e in bm.edges)} nonmanifold={sum(not e.is_manifold and not e.is_boundary for e in bm.edges)}')
    bm.to_mesh(low.data); bm.free(); low.data.update()

    # 4 smart-project unwrap (constant texel density; the two cloth layers get their own islands), then bake
    while low.data.uv_layers:
        low.data.uv_layers.remove(low.data.uv_layers[0])
    low.data.uv_layers.new(name='UVMap')
    bpy.ops.object.select_all(action='DESELECT'); bpy.context.view_layer.objects.active = low; low.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(75), island_margin=0.004, area_weight=0.7)
    bpy.ops.object.mode_set(mode='OBJECT')
    # 5b bake the Forge colour (hi keeps its glTF material and UVs) onto the new UVs
    name = 'Hood' + a.tag
    img = bpy.data.images.new(f'T_{name}_BaseColor', a.tex, a.tex)
    mat = bpy.data.materials.new(f'M_{name}'); mat.use_nodes = True
    node = mat.node_tree.nodes.new('ShaderNodeTexImage'); node.image = img
    mat.node_tree.nodes.active = node
    low.data.materials.clear(); low.data.materials.append(mat)
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
    col = px[~empty, :3]
    # grade: the mean colour becomes the dark brown wool (contrast of the generated mottling kept), then a coarse weave (period 12 px) and slub noise
    mean = col.mean(axis=0)
    px[:, :3] = np.clip(px[:, :3] * (np.array(WOOL_SRGB) / mean), 0, 1)
    px[empty, :3] = np.array(WOOL_SRGB)  # empty texels = the mean cloth colour, or the mips darken it
    t = a.tex
    yy, xx = np.mgrid[0:t, 0:t]
    per = 10.0
    weave = 0.5 * np.sin(2 * np.pi * xx / per) * np.sin(2 * np.pi * yy / per) + 0.5 * np.sin(2 * np.pi * (xx + yy) / (per * 2.1)) * 0.0
    rng = np.random.default_rng(7)
    slub = rng.normal(0, 1, (t, t)).astype(np.float32)
    slub = (slub + np.roll(slub, 1, 0) + np.roll(slub, 1, 1) + np.roll(slub, 2, 1)) / 4
    px[:, :3] = np.clip(px[:, :3] * (1.0 + 0.28 * weave.reshape(-1, 1) + 0.18 * slub.reshape(-1, 1)), 0, 1)
    px[:, 3] = 1.0
    img.pixels.foreach_set(px.ravel())
    img.filepath_raw = os.path.join(os.path.dirname(out), f'T_{name}_BaseColor.png'); img.file_format = 'PNG'; img.save()
    print(f'BAKED {img.filepath_raw} mean {tuple(round(float(x), 3) for x in mean)} -> {WOOL_SRGB}')
    bpy.data.objects.remove(hi)

    # 6 the solid already has its 3-19 mm of cloth: just smooth shading
    bpy.ops.object.select_all(action='DESELECT'); low.select_set(True); bpy.context.view_layer.objects.active = low
    low.name = f'SK_{name}'
    for poly in low.data.polygons:
        poly.use_smooth = True
    tris = sum(len(p.vertices) - 2 for p in low.data.polygons)
    print(f'FINAL tris={tris}')

    # tunnel check: rays from the head centre through the back half-space (-30 to +40 deg elevation: below that they pass under the hem) must all hit the cloth
    tree = BVHTree.FromObject(low, bpy.context.evaluated_depsgraph_get())
    miss = 0; total = 0
    for az in range(-40, 41, 3):
        for el in range(-30, 41, 3):
            d = Vector((math.sin(math.radians(az)) * math.cos(math.radians(el)), math.cos(math.radians(az)) * math.cos(math.radians(el)), math.sin(math.radians(el))))
            total += 1
            if tree.ray_cast(head_c, d)[0] is None:
                miss += 1; print(f"MISSRAY az={az} el={el}")
    lb = [low.matrix_world @ v.co for v in low.data.vertices]
    print(f'TUNNELCHECK missed {miss} of {total} rays from the head centre; garment bbox {tuple(round(min(p[i] for p in lb), 1) for i in range(3))} {tuple(round(max(p[i] for p in lb), 1) for i in range(3))}')
    if miss and not a.skip_checks:
        raise SystemExit(f'TUNNEL: {miss} of {total} back rays pass through the garment')

    # 7 skin weights, then smoothed over the cloth so it moves as one draped piece
    bw = {g.index: g.name for g in body.vertex_groups}
    bvh = BVHTree.FromBMesh(bb)
    bb.faces.ensure_lookup_table()
    body_idx = body.data.vertices
    bb.verts.ensure_lookup_table()
    bones = arm.data.bones
    zseam = bones['neck_01'].head_local.z
    z_head = bones['head'].head_local.z
    W = {}
    for v in low.data.vertices:
        loc, nrm, fi, dist = bvh.find_nearest(v.co)
        f = bb.faces[fi]
        w = {}
        ws = [dict((bw[g.group], g.weight) for g in body_idx[vv.index].groups) for vv in f.verts]
        p0, p1, p2 = (vv.co for vv in f.verts)
        v0, v1, v2 = p1 - p0, p2 - p0, loc - p0
        d00, d01, d11, d20, d21 = v0.dot(v0), v0.dot(v1), v1.dot(v1), v2.dot(v0), v2.dot(v1)
        den = d00 * d11 - d01 * d01 or 1e-12
        bv = (d11 * d20 - d01 * d21) / den; bwt = (d00 * d21 - d01 * d20) / den
        for k, x in zip(ws, (1 - bv - bwt, bv, bwt)):
            for n, val in k.items():
                w[n] = w.get(n, 0.0) + val * x
        t = max(0.0, min(1.0, (v.co.z - zseam) / max(1e-6, z_head - zseam)))  # above the seam: follow the neck and head
        if t > 0:
            w = {n: x * (1 - t) for n, x in w.items()}
            w['neck_02'] = w.get('neck_02', 0.0) + t * (1 - t) * 2.0
            w['head'] = w.get('head', 0.0) + t * t
        W[v.index] = w
    nbrs = {v.index: [] for v in low.data.vertices}
    for e in low.data.edges:
        nbrs[e.vertices[0]].append(e.vertices[1]); nbrs[e.vertices[1]].append(e.vertices[0])
    for _ in range(6):  # weights average with the neighbours: panels no longer swing on their own
        nxt = {}
        for i, w in W.items():
            acc = {n: x for n, x in w.items()}
            for j in nbrs[i]:
                for n, x in W[j].items():
                    acc[n] = acc.get(n, 0.0) + x / max(1, len(nbrs[i]))
            nxt[i] = {n: x / 2 for n, x in acc.items()}
        W = nxt
    groups = {}
    for i, w in W.items():
        top8 = sorted(w.items(), key=lambda kv: -kv[1])[:8]
        tot = sum(x for _, x in top8) or 1.0
        groups[i] = [(n, x / tot) for n, x in top8 if x / tot > 1e-3]
    vg = {}
    for i, lst in groups.items():
        for n, x in lst:
            if n not in vg:
                vg[n] = low.vertex_groups.new(name=n)
            vg[n].add([i], x, 'REPLACE')
    low.parent = arm
    mod = low.modifiers.new('Armature', 'ARMATURE'); mod.object = arm

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
