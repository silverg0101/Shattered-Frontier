"""Build a garment's body-removal mesh: the patch of the bare MetaHuman body the garment fully covers, for Mutable's Remove Mesh modifier.

Blender 5.1, headless:
  "C:\\Program Files\\Blender Foundation\\Blender 5.1\\blender.exe" --factory-startup --background --python tools/Build-GarmentBodyClip.py -- \
      --body Artifacts/Characters/PlayerBase/OrlandoBare_body.fbx --garment Artifacts/Characters/PlayerBase/garment_tunic_leggings.fbx \
      --out Artifacts/Characters/PlayerBase/garment_tunic_leggings_bodyclip.fbx
A body vertex is covered when a ray along its normal hits the garment within --reach cm (the skirt's thighs are covered by the leggings,
so the tube's larger gap does not matter). Faces whose three vertices are covered are kept, then the patch is shrunk by --margin rings so
skin still meets every hem, cuff and neck edge. Mutable removes base-mesh vertices at the same positions, so the patch is the body's own
geometry, exported skinned to the same armature (Unreal imports it on metahuman_base_skel).
"""
import argparse
import os
import sys

import bmesh
import bpy
from mathutils.bvhtree import BVHTree


def parse():
    p = argparse.ArgumentParser()
    p.add_argument('--body', required=True)
    p.add_argument('--garment', required=True)
    p.add_argument('--out', required=True)
    p.add_argument('--reach', type=float, default=4.0, help='cm along the skin normal to look for cloth')
    p.add_argument('--margin', type=int, default=2, help='rings of covered skin kept next to garment edges')
    p.add_argument('--mask', help='also write the patch as a body-UV PNG (black = hidden, white = kept): a MetaHuman hidden face map')
    p.add_argument('--mask-size', type=int, default=1024)
    return p.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])


def write_mask(bm, path, size):
    """Rasterize the patch's faces in UV channel 0: black where the garment hides the skin, white elsewhere."""
    import numpy as np
    img = np.ones((size, size), np.float32)
    uv = bm.loops.layers.uv[0]
    ys, xs = np.mgrid[0:size, 0:size]
    for f in bm.faces:
        pts = [(l[uv].uv.x % 1.0 * size, l[uv].uv.y % 1.0 * size) for l in f.loops]  # MetaHuman body UVs sit in UDIM 1002
        for k in range(1, len(pts) - 1):  # fan triangles
            (ax, ay), (bx, by), (cx, cy) = pts[0], pts[k], pts[k + 1]
            x0, x1 = int(max(min(ax, bx, cx) - 1, 0)), int(min(max(ax, bx, cx) + 2, size))
            y0, y1 = int(max(min(ay, by, cy) - 1, 0)), int(min(max(ay, by, cy) + 2, size))
            if x0 >= x1 or y0 >= y1:
                continue
            px, py = xs[y0:y1, x0:x1] + 0.5, ys[y0:y1, x0:x1] + 0.5
            e0 = (bx - ax) * (py - ay) - (by - ay) * (px - ax)
            e1 = (cx - bx) * (py - by) - (cy - by) * (px - bx)
            e2 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx)
            inside = ((e0 >= -1) & (e1 >= -1) & (e2 >= -1)) | ((e0 <= 1) & (e1 <= 1) & (e2 <= 1))  # either winding, ~1 px dilation
            img[y0:y1, x0:x1][inside] = 0.0
    rgba = np.repeat(img.reshape(-1, 1), 4, axis=1); rgba[:, 3] = 1.0  # Blender image rows run bottom-up, like UV v
    im = bpy.data.images.new('mask', size, size, alpha=False)
    im.pixels.foreach_set(rgba.ravel())
    im.filepath_raw = os.path.abspath(path); im.file_format = 'PNG'
    os.makedirs(os.path.dirname(im.filepath_raw), exist_ok=True)
    im.save()
    print(f'BODYCLIP_MASK hidden_px={int((img == 0).sum())} out={im.filepath_raw}')


def main():
    args = parse()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.abspath(args.body))
    body = next(o for o in bpy.data.objects if o.type == 'MESH')
    arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=os.path.abspath(args.garment))
    new = set(bpy.data.objects) - before
    garment = next(o for o in new if o.type == 'MESH')

    deps = bpy.context.evaluated_depsgraph_get()
    gm = bmesh.new()
    gm.from_object(garment, deps)
    gm.transform(garment.matrix_world)
    tree = BVHTree.FromBMesh(gm)
    reach = args.reach * (0.01 if max(body.dimensions) < 10 else 1.0)  # FBX from Unreal may arrive in metres or centimetres

    bm = bmesh.new()
    bm.from_mesh(body.data)
    mw = body.matrix_world
    nm = mw.to_3x3().inverted().transposed()
    covered = [tree.ray_cast(mw @ v.co, (nm @ v.normal).normalized(), reach)[0] is not None for v in bm.verts]
    for _ in range(args.margin):  # shrink: a vertex next to an uncovered one becomes uncovered
        covered = [c and all(covered[e.other_vert(v).index] for e in v.link_edges) for v, c in zip(bm.verts, covered)]
    doomed = [f for f in bm.faces if not all(covered[v.index] for v in f.verts)]
    bmesh.ops.delete(bm, geom=doomed, context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    kept = len(bm.faces)
    if args.mask:
        write_mask(bm, args.mask, args.mask_size)
    bm.to_mesh(body.data)
    body.data.update()
    body.name = os.path.splitext(os.path.basename(args.out))[0]
    for o in new:
        bpy.data.objects.remove(o)
    bpy.ops.object.select_all(action='DESELECT')
    for o in (arm, body):
        o.select_set(True)
    out = os.path.abspath(args.out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=out, use_selection=True, object_types={'ARMATURE', 'MESH'},
                             use_armature_deform_only=False, add_leaf_bones=False, bake_anim=False,
                             mesh_smooth_type='FACE', path_mode='STRIP')
    print(f'BODYCLIP faces={kept} covered_verts={sum(covered)} out={out}')


main()
