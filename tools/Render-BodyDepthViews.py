"""Orthographic depth + silhouette maps of the reference body (Orlando body FBX + head OBJ, as Render-BodyReference.py loads it)
from the Asset Forge's item views (front, left, right: 1024 px squares, ortho, camera on -Y / +X / -X), framed on one slot
from tools/body-slots.json. Output per view: depth_<slot>_<view>.png (near white, background black, depth stretched over the
slot region's own range), mask_<slot>_<view>.png (body silhouette), plus <slot>_meta.json (frame, and the slot region's
pixel bbox per view, for layout prompts).
  blender --factory-startup -b --python tools/Render-BodyDepthViews.py -- --slot head [--out DIR]
"""
import json, math, os, sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
opt = lambda k, d: args[args.index(k) + 1] if k in args else d
slot, out = opt('--slot', 'head'), opt('--out', os.path.join(ROOT, 'Artifacts/GEAR02/depth'))
cfg = json.load(open(os.path.join(ROOT, 'tools/body-slots.json')))
if not cfg.get(slot, {}).get('z'):
    sys.exit(f'slot {slot!r} has no z range in tools/body-slots.json yet')
S = 1024
os.makedirs(out, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=os.path.join(ROOT, 'Artifacts/Characters/PlayerBase/OrlandoBare_body.fbx'))
body = next(o for o in bpy.data.objects if o.type == 'MESH')
lines = open(os.path.join(ROOT, 'Artifacts/Characters/PlayerBase/OrlandoBare_head.obj')).read().splitlines()  # Unreal cm, y flipped to the FBX frame
hv = [(float(x) * .01, -float(y) * .01, float(z) * .01) for _, x, y, z in (l.split() for l in lines if l.startswith('v '))]
hf = [[int(i.split('/')[0]) - 1 for i in l.split()[1:4]] for l in lines if l.startswith('f ')]
bv = [tuple(body.matrix_world @ v.co) for v in body.data.vertices]
bf = [tuple(p.vertices) for p in body.data.polygons]
verts = bv + hv
faces = bf + [[i + len(bv) for i in f] for f in hf]
pts = np.array(verts)
lo, hi = pts.min(0), pts.max(0)
H = hi[2] - lo[2]
z0, z1 = (lo[2] + H * f for f in cfg[slot]['z'])
size = (z1 - z0) * cfg[slot]['margin']
ctr = Vector(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (z0 + z1) / 2))
# arms below the shoulders are not part of the slot: drop faces touching vertices whose heaviest bone is an arm/hand bone
# (shoulder caps above arm_cut_z stay, the garment covers them); the slot's lower edge z0 is a hard cut applied to the rays
names = {g.index: g.name for g in body.vertex_groups}
arm = lambda n: n.startswith(('upperarm', 'lowerarm', 'hand', 'wrist', 'pinky', 'ring', 'thumb', 'middle', 'index'))
cut = lo[2] + H * cfg[slot].get('arm_cut_z', 0)
drop = {i for i, v in enumerate(body.data.vertices)
        if v.groups and arm(names[max(v.groups, key=lambda g: g.weight).group]) and bv[i][2] < cut}
bvh = BVHTree.FromPolygons(verts, [f for f in faces if not drop.intersection(f)])
meta = {'slot': slot, 'size_m': size, 'center': list(ctr), 'z_range_m': [z0, z1], 'views': {}}

def soften(d, grow, smooth):
    """Garment, not skin: grow the silhouette by `grow` px (nearest depth, as cloth thickness) and box-blur the depth by
    `smooth` px so ears, lips and fingers do not survive into the garment."""
    for _ in range(grow):
        n = np.stack([np.roll(d, (a, b), (0, 1)) for a in (-1, 0, 1) for b in (-1, 0, 1)])
        near = np.nanmin(np.where(np.isnan(n), np.inf, n), axis=0)
        d = np.where(np.isnan(d) & np.isfinite(near), near, d)
    if smooth:
        m = (~np.isnan(d)).astype(np.float32); v = np.where(np.isnan(d), 0, d).astype(np.float32)
        def blur(a):
            for ax in (0, 1):
                a = np.moveaxis(a, ax, 0); c = np.cumsum(np.pad(a, ((smooth + 1, smooth), (0, 0))), 0)
                a = np.moveaxis((c[2 * smooth + 1:] - c[:-2 * smooth - 1]) / (2 * smooth + 1), 0, ax)
            return a
        bm = blur(m); bv_ = blur(v)
        d = np.where(m > 0, bv_ / np.maximum(bm, 1e-6), np.nan).astype(np.float32)
    return d

def save(g, path):  # g: HxW uint8, row 0 on top; Blender wants bottom-up RGBA floats
    im = bpy.data.images.new('t', S, S, alpha=False)
    f = np.repeat(g[::-1, :, None] / 255.0, 4, axis=2).astype(np.float32); f[..., 3] = 1
    im.pixels.foreach_set(f.ravel()); im.filepath_raw = path; im.file_format = 'PNG'; im.save(); bpy.data.images.remove(im)

for name, az in (('front', 0), ('left', 90), ('right', 270)):
    a = math.radians(az)
    d = Vector((math.sin(a), -math.cos(a), 0))
    right = Vector((math.cos(a), math.sin(a), 0))
    depth = np.full((S, S), np.nan, np.float32)
    for j in range(S):
        v = (0.5 - (j + 0.5) / S) * size
        for i in range(S):
            o = ctr + right * ((i + 0.5) / S - 0.5) * size + Vector((0, 0, v)) + d * 5
            hit = bvh.ray_cast(o, -d)
            if hit[0] is not None and hit[0].z >= z0:
                depth[j, i] = hit[3]  # distance from the camera plane origin
    depth = soften(depth, cfg[slot].get('grow_px', 0), cfg[slot].get('smooth_px', 0))
    m = ~np.isnan(depth)
    dn, df = np.nanmin(depth), np.nanmax(depth)
    img = np.where(m, 40 + 215 * (df - depth) / (df - dn), 0).astype(np.uint8)  # near white, far dark grey, background black
    save(img, os.path.join(out, f'depth_{slot}_{name}.png'))
    save((m * 255).astype(np.uint8), os.path.join(out, f'mask_{slot}_{name}.png'))
    ys, xs = np.nonzero(m)
    meta['views'][name] = {'body_bbox_px': [int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())]}
    print('VIEW', name, meta['views'][name])
json.dump(meta, open(os.path.join(out, f'{slot}_meta.json'), 'w'), indent=1)
