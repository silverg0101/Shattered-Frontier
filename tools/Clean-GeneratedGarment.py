"""GEAR-02: clean an Asset Forge garment mesh (GLB) into a skinned game garment on Orlando's body. Deterministic, no hand edits.

Blender 5.1, headless (all lengths in cm, the body FBX's own units):
  blender --factory-startup -b --python tools/Clean-GeneratedGarment.py -- --garment GameClient/ArtSource/Characters/Player/outfit-asset/hood/hood.glb --out Artifacts/Characters/PlayerBase/hood/hood.fbx
Steps: 1 align the mesh to the body (uniform scale + shift so its height spans --bottom..--top and it sits on the head, x centred);
2 delete faces inside the body/head meshes or within --offset of them (opens the face and neck); 3 decimate a copy to a shell of ~--tris/2
faces, unwrap it (smart UV), bake the generated texture onto it (Cycles, selected to active); 4 solidify --thickness outward
(rim filled; the inner skin stays --offset off the body); 5 skin weights from the nearest body triangle (barycentric), verts above the
neck seam ramp to neck_02/head; 6 export FBX + armature (Unreal skeleton, as Build-PlayerGarment.py) + PNG texture + .blend.
"""
import argparse
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.join(ROOT, 'Artifacts/Characters/PlayerBase')


def parse():
    p = argparse.ArgumentParser()
    p.add_argument('--garment', required=True)
    p.add_argument('--out', required=True)
    p.add_argument('--body', default=os.path.join(BASE, 'Orlando_body.fbx'))
    p.add_argument('--head', default=os.path.join(BASE, 'Orlando_head.obj'))
    p.add_argument('--top', type=float, default=206.0, help='cm, top of the garment (the hood is ~16 cm above the head: it must clear the skull)')
    p.add_argument('--bottom', type=float, default=117.0, help='cm, lowest point of the cape')
    p.add_argument('--yshift', type=float, default=10.0, help='cm, extra shift toward the face (-y)')
    p.add_argument('--offset', type=float, default=0.8, help='cm: faces closer than this to the skin (or inside it) are deleted')
    p.add_argument('--thickness', type=float, default=0.4, help='cm of cloth (3-5 mm)')
    p.add_argument('--tris', type=int, default=12000, help='final triangle count, about')
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
    from mathutils import Matrix
    me.transform(Matrix.Rotation(math.radians(a.yaw), 4, 'Z'))
    zs = [v.co.z for v in me.vertices]
    s = 100.0 * (top - a.bottom) / 100.0 / (max(zs) - min(zs)) * 1.0  # glTF metres -> cm, height fitted
    s = (top - a.bottom) / (max(zs) - min(zs))
    xs = [v.co.x for v in me.vertices]; ys = [v.co.y for v in me.vertices]
    cx, cy = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2
    for v in me.vertices:
        v.co = Vector(((v.co.x - cx) * s, (v.co.y - cy) * s + head_c.y + a.yshift, (v.co.z - min(zs)) * s + a.bottom))
    print(f'ALIGN scale={s:.2f} (m->cm x{s:.1f}) size_cm={(max(xs)-min(xs))*s:.1f} x {(max(ys)-min(ys))*s:.1f} x {(top-a.bottom):.1f}')

    # 2 cape below the head mesh: pushed out of the body; face opening and neck: faces inside / near the skin deleted
    bm = bmesh.new(); bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=0.002)  # glTF splits vertices along UV seams and hard edges: weld, or decimation shreds the cuts
    bm.faces.ensure_lookup_table()
    # fit report on the aligned, still uncut mesh: does the head fit inside the hood?
    tree0 = BVHTree.FromBMesh(bm)
    hv_ = [v.co for v in hb.verts if v.co.z > head_z - 30]
    pen = []
    for v in hv_:  # from the head centre to each head vertex: a hood wall hit before the vertex is a wall inside the head
        d = v - head_c; L = d.length
        hit = tree0.ray_cast(head_c, d.normalized(), L)
        pen.append(L - hit[3] if hit[0] is not None else 0.0)
    inside = [x for x in pen if x > 0.3]
    print(f'FITREPORT head verts with the hood wall inside them (>3 mm): {100 * len(inside) / len(pen):.1f}% of {len(pen)}, max {max(pen):.1f} cm, mean of those {sum(inside) / max(1, len(inside)):.1f} cm')
    zeye = head_c.z + 4
    hits, o_, dd = [], Vector((0, -60, zeye)), Vector((0, 1, 0))
    for _ in range(6):
        h = tree0.ray_cast(o_, dd, 200)
        if h[0] is None:
            break
        hits.append(h[0].y); o_ = h[0] + dd * 0.05
    hy = [v.co.y for v in hb.verts if abs(v.co.z - zeye) < 1.5]
    print(f'FITREPORT along +y at eye height (x=0): hood wall hits y={[round(y, 1) for y in hits]}; Orlando head y {min(hy):.1f}..{max(hy):.1f} (depth {max(hy) - min(hy):.1f} cm); hood front rim y {min(v.co.y for v in bm.verts if abs(v.co.z - zeye) < 12):.1f}')

    zpush = min(v.co.z for v in hb.verts) + 4.0  # below the head mesh the cape rests on the shoulders: push it out instead of cutting holes
    pushed = 0
    for v in bm.verts:
        if v.co.z < zpush:
            loc, nrm, _i, _d = skin.find_nearest(v.co)
            if loc is not None and (v.co - loc).dot(nrm) < a.offset:
                v.co = loc + nrm * a.offset; pushed += 1
    print(f'PUSHED {pushed} cape verts out of the body')
    doomed = []
    for f in bm.faces:
        c = f.calc_center_median()
        if c.z < zpush:
            continue
        loc, nrm, _i, dist = skin.find_nearest(c)
        if loc is not None and (c - loc).dot(nrm) < a.offset:
            doomed.append(f)
    print(f'INSIDE deleted {len(doomed)} of {len(bm.faces)} faces')
    bmesh.ops.delete(bm, geom=doomed, context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(me); bm.free(); me.update()

    # 3 low-poly shell copy, new UVs, bake the texture
    low = hi.copy(); low.data = hi.data.copy(); low.name = 'low'
    bpy.context.scene.collection.objects.link(low)
    bpy.ops.object.select_all(action='DESELECT'); bpy.context.view_layer.objects.active = low; low.select_set(True)
    if any(len(p.vertices) > 3 for p in low.data.polygons):
        bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.quads_convert_to_tris(); bpy.ops.object.mode_set(mode='OBJECT')
    src_mesh = low.data.copy()
    ratio = min(1.0, a.tris * 0.4 / max(1, len(src_mesh.polygons)))
    for _ in range(3):  # solidify's doubled shell plus rim depends on the border length: converge on the final count
        low.data = src_mesh.copy()
        m = low.modifiers.new('dec', 'DECIMATE'); m.ratio = ratio
        bpy.ops.object.modifier_apply(modifier='dec')
        m = low.modifiers.new('sol', 'SOLIDIFY'); m.thickness = a.thickness; m.use_rim = True
        ev = low.evaluated_get(bpy.context.evaluated_depsgraph_get())
        n = sum(len(p.vertices) - 2 for p in ev.data.polygons)
        low.modifiers.remove(m)
        print(f'RATIO {ratio:.4f} -> final tris {n}')
        if abs(n - a.tris) < 0.08 * a.tris:
            break
        ratio = min(1.0, ratio * a.tris / n)
    # drop loose specks
    bm = bmesh.new(); bm.from_mesh(low.data)
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
    big = max(len(c) for c in parts)
    kill = [f for c in parts if len(c) < big * 0.05 for f in c]
    bmesh.ops.delete(bm, geom=kill, context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    # outward normals: majority of faces should point away from the head centre
    bm.normal_update()
    if sum((f.calc_center_median() - head_c).dot(f.normal) for f in bm.faces) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    bm.to_mesh(low.data); bm.free(); low.data.update()
    print(f'LOW shell tris={len(low.data.polygons)}')
    while low.data.uv_layers:
        low.data.uv_layers.remove(low.data.uv_layers[0])
    low.data.uv_layers.new(name='UVMap')
    # ponytail: smart project fragments the decimated, crumpled shell into hundreds of islands (a one-seam unwrap overlapped and stretched worse); lower --tris or smooth the shell to improve
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(89), island_margin=0.003, area_weight=0.6)
    bpy.ops.object.mode_set(mode='OBJECT')
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
    sc.render.bake.cage_extrusion = 1.0; sc.render.bake.max_ray_distance = 3.0; sc.render.bake.margin = 12
    bpy.ops.object.select_all(action='DESELECT'); hi.select_set(True); low.select_set(True); bpy.context.view_layer.objects.active = low
    bpy.ops.object.bake(type='DIFFUSE')
    out = os.path.abspath(a.out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    import numpy as np  # empty texels become the garment's mean colour, or the mips of so many small islands darken the cloth in game
    px = np.array(img.pixels[:], dtype=np.float32).reshape(-1, 4)
    empty = px[:, :3].sum(axis=1) < 1e-6
    px[empty, :3] = px[~empty, :3].mean(axis=0); px[:, 3] = 1.0
    img.pixels.foreach_set(px.ravel())
    img.filepath_raw = os.path.join(os.path.dirname(out), f'T_{name}_BaseColor.png'); img.file_format = 'PNG'; img.save()
    print(f'BAKED {img.filepath_raw}')
    bpy.data.objects.remove(hi)

    # 4 thickness, outward (inner skin stays at the offset)
    bpy.ops.object.select_all(action='DESELECT'); low.select_set(True); bpy.context.view_layer.objects.active = low
    m = low.modifiers.new('sol', 'SOLIDIFY'); m.thickness = a.thickness; m.offset = 1.0; m.use_rim = True
    bpy.ops.object.modifier_apply(modifier='sol')
    low.name = f'SK_{name}'
    for poly in low.data.polygons:
        poly.use_smooth = True  # the decimated shell shows its facets under flat shading
    tris = sum(len(p.vertices) - 2 for p in low.data.polygons)
    print(f'FINAL tris={tris}')

    # tunnel check: rays from the head centre through the back half-space (+-40 deg, -45 to +40 deg elevation, 3 deg steps: also finds a slit down the centre back) must all hit the hood; a hole the size
    # of the face in its back means the input view showed background through the face opening
    tree = BVHTree.FromObject(low, bpy.context.evaluated_depsgraph_get())
    miss = 0; total = 0
    for az in range(-40, 41, 3):
        for el in range(-45, 41, 3):
            d = Vector((math.sin(math.radians(az)) * math.cos(math.radians(el)), math.cos(math.radians(az)) * math.cos(math.radians(el)), math.sin(math.radians(el))))
            total += 1
            miss += tree.ray_cast(head_c, d)[0] is None
    lb = [low.matrix_world @ v.co for v in low.data.vertices]
    print(f'TUNNELCHECK missed {miss} of {total} rays from the head centre {tuple(round(c, 1) for c in head_c)} to the back; garment bbox {tuple(round(min(p[i] for p in lb), 1) for i in range(3))} {tuple(round(max(p[i] for p in lb), 1) for i in range(3))}')
    if miss and not a.skip_checks:
        raise SystemExit(f'TUNNEL: {miss} of {total} back rays pass through the garment (hole behind the face opening)')

    # 5 skin weights
    bw = {g.index: g.name for g in body.vertex_groups}
    bvh = BVHTree.FromBMesh(bb)
    bb.faces.ensure_lookup_table()
    wts = lambda v: {bw[g.group]: g.weight for g in body.data.vertices[v.index].groups}
    body_idx = body.data.vertices
    # bb was built from the body mesh and triangulated in place: vertex i of bb is body vertex i
    bb.verts.ensure_lookup_table()
    bones = arm.data.bones
    zseam = bones['neck_01'].head_local.z
    z_head = bones['head'].head_local.z
    groups = {}
    for v in low.data.vertices:
        loc, nrm, fi, dist = bvh.find_nearest(v.co)
        f = bb.faces[fi]
        w = {}
        ws = [dict((bw[g.group], g.weight) for g in body_idx[vv.index].groups) for vv in f.verts]
        # barycentric weights of loc on the triangle
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
        top8 = sorted(w.items(), key=lambda kv: -kv[1])[:8]
        tot = sum(x for _, x in top8) or 1.0
        groups[v.index] = [(n, x / tot) for n, x in top8 if x / tot > 1e-3]
    vg = {}
    for i, lst in groups.items():
        for n, x in lst:
            if n not in vg:
                vg[n] = low.vertex_groups.new(name=n)
            vg[n].add([i], x, 'REPLACE')
    low.parent = arm
    mod = low.modifiers.new('Armature', 'ARMATURE'); mod.object = arm

    # 6 export (the skeleton is the UE one: no Empty above it, cm)
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
