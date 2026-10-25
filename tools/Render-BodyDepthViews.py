"""Orthographic depth + silhouette maps of a clearance-expanded dress form over OrlandoBare.
from the Asset Forge's item views (front, left, right: 1024 px squares, ortho, camera on -Y / +X / -X), framed on one slot
from tools/body-slots.json. Output per view: depth_<slot>_<view>.png (near white, background black, depth stretched over the
slot region's own range), mask_<slot>_<view>.png (proxy silhouette), body_mask_<slot>_<view>.png (bare body),
plus <slot>_meta.json (frame, and the slot region's
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
slot, out = opt('--slot', 'head'), os.path.abspath(opt('--out', os.path.join(ROOT, 'Artifacts/GEAR02/depth')))
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
bare_verts = bv + hv
bare_faces = bf + [[i + len(bv) for i in f] for f in hf]
pts = np.array(bare_verts)
lo, hi = pts.min(0), pts.max(0)
H = hi[2] - lo[2]
z0, z1 = (lo[2] + H * f for f in cfg[slot]['z'])
size = (z1 - z0) * cfg[slot]['margin']
ctr = Vector(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (z0 + z1) / 2))
if slot == 'head':
    ctr.y = (min(v[1] for v in hv) + max(v[1] for v in hv)) / 2
# Arms are not under a hood/cape; omit their weighted faces while retaining the torso shoulder.
# The slot's lower edge z0 is a hard cut applied to the rays.
names = {g.index: g.name for g in body.vertex_groups}
arm = lambda n: n.startswith(('upperarm', 'lowerarm', 'hand', 'wrist', 'pinky', 'ring', 'thumb', 'middle', 'index'))
drop = {i for i, v in enumerate(body.data.vertices)
        if v.groups and arm(names[max(v.groups, key=lambda g: g.weight).group])}
bare_bvh = BVHTree.FromPolygons(bare_verts, [f for f in bare_faces if not drop.intersection(f)])

c = cfg[slot]
clearance = float(c['clearance_cm']) / 100
loose = float(c.get('loose_clearance_cm', c['clearance_cm'])) / 100
loose_z = float(c.get('loose_below_z', c['z'][0]))
if not (0 <= clearance <= .1 and clearance <= loose <= .15 and 0 <= loose_z <= 1):
    sys.exit('invalid slot clearance; use 0-10 cm minimum and minimum-15 cm loose clearance')

def offset(z):
    f = max(0., min(1., (lo[2] + H * (loose_z + .1) - z) / (H * .1)))
    return clearance + (loose - clearance) * f

cx, cy = ctr.x, ctr.y
proxy_verts = []
proxy_faces = []
head = np.asarray(hv)
hz0, hz1 = float(head[:, 2].min()), float(head[:, 2].max())
if slot != 'head':
    for x, y, z in bv:
        dx, dy = x - cx, y - cy
        r = math.hypot(dx, dy)
        dr = offset(z)
        proxy_verts.append((x + dx / r * dr, y + dy / r * dr, z) if r > 1e-5 else (x, y, z))
    proxy_faces = [f for f in bf if not drop.intersection(f)]
else:
    # A continuous bell form replaces the open arm-cut torso mesh. Each ring envelopes nearby
    # torso vertices with the configured cape clearance, then widens toward the hem.
    torso = np.asarray([bv[i] for i in range(len(bv)) if i not in drop and z0 <= bv[i][2] <= hz0 + .11])
    mx = float((torso[:, 0].min() + torso[:, 0].max()) / 2)
    my = float((torso[:, 1].min() + torso[:, 1].max()) / 2)
    lower = torso[torso[:, 2] <= z0 + .07]
    bottom_x = float(np.max(np.abs(lower[:, 0] - mx))) + loose
    bottom_y = float(np.max(np.abs(lower[:, 1] - my))) + loose
    top_z = hz0 + .16
    levels = list(np.linspace(z0, hz0 + .10, 9)) + [top_z]
    radii = []
    for z in levels[:-1]:
        nearby = torso[np.abs(torso[:, 2] - z) <= .03]
        shape = 1 - (z - z0) / (top_z - z0)
        x = max(.105 + (bottom_x - .105) * shape,
                float(np.max(np.abs(nearby[:, 0] - mx))) + offset(z) if len(nearby) else 0)
        y = max(.105 + (bottom_y - .105) * shape,
                float(np.max(np.abs(nearby[:, 1] - my))) + offset(z) if len(nearby) else 0)
        radii.append([x, y])
    radii.append([.105 + clearance, .105 + clearance])
    for j in range(len(radii) - 2, -1, -1):
        radii[j][0] = max(radii[j][0], radii[j + 1][0])
        radii[j][1] = max(radii[j][1], radii[j + 1][1])
    segs = 48
    for z, (rx_m, ry_m) in zip(levels, radii):
        for k in range(segs):
            angle = 2 * math.pi * k / segs
            proxy_verts.append((mx + math.cos(angle) * rx_m, my + math.sin(angle) * ry_m, z))
    for j in range(len(levels) - 1):
        for k in range(segs):
            q = j * segs + k; nxt = j * segs + (k + 1) % segs
            proxy_faces.append((q, nxt, nxt + segs, q + segs))
    proxy_faces.extend([tuple(reversed(range(segs))),
                        tuple((len(levels) - 1) * segs + k for k in range(segs))])

# Replace the scanned head with a smooth closed form. The depth radius encloses the nose and
# the ring profile removes its relief; the lower mantle encloses the wide ear/neck vertices.
hx = float(np.median(head[:, 0]))
hy = float((head[:, 1].min() + head[:, 1].max()) / 2)
rx = float(np.quantile(np.abs(head[:, 0] - hx), .97))
ry = float(np.max(np.abs(head[:, 1] - hy)))
ring = ((0., .75), (.1, .95), (.2, 1.), (.77, 1.), (.85, .92), (.94, .55), (1., .01))
segs = 48; start = len(proxy_verts)
for t, scale in ring:
    z = hz0 + t * (hz1 - hz0) + clearance * t
    for k in range(segs):
        a = 2 * math.pi * k / segs
        proxy_verts.append((hx + math.cos(a) * (rx * scale + clearance),
                            hy + math.sin(a) * (ry * scale + clearance), z))
for j in range(len(ring) - 1):
    for k in range(segs):
        q = start + j * segs + k; nxt = start + j * segs + (k + 1) % segs
        proxy_faces.append((q, nxt, nxt + segs, q + segs))
proxy_faces.extend([tuple(start + k for k in reversed(range(segs))),
                    tuple(start + (len(ring) - 1) * segs + k for k in range(segs))])
proxy_bvh = BVHTree.FromPolygons(proxy_verts, proxy_faces)
meta = {'slot': slot, 'size_m': size, 'center': list(ctr), 'z_range_m': [z0, z1],
        'clearance_cm': c['clearance_cm'], 'loose_clearance_cm': c.get('loose_clearance_cm', c['clearance_cm']), 'views': {}}

def save(g, path):  # g: HxW uint8, row 0 on top; Blender wants bottom-up RGBA floats
    im = bpy.data.images.new('t', S, S, alpha=False)
    f = np.repeat(g[::-1, :, None] / 255.0, 4, axis=2).astype(np.float32); f[..., 3] = 1
    im.pixels.foreach_set(f.ravel()); im.filepath_raw = path; im.file_format = 'PNG'; im.save(); bpy.data.images.remove(im)

for name, az in (('front', 0), ('left', 90), ('right', 270)):
    a = math.radians(az)
    d = Vector((math.sin(a), -math.cos(a), 0))
    right = Vector((math.cos(a), math.sin(a), 0))
    depth = np.full((S, S), np.nan, np.float32)
    bare = np.zeros((S, S), bool)
    for j in range(S):
        v = (0.5 - (j + 0.5) / S) * size
        for i in range(S):
            o = ctr + right * ((i + 0.5) / S - 0.5) * size + Vector((0, 0, v)) + d * 5
            hit = proxy_bvh.ray_cast(o, -d)
            if hit[0] is not None and hit[0].z >= z0:
                depth[j, i] = hit[3]  # distance from the camera plane origin
            hit = bare_bvh.ray_cast(o, -d)
            bare[j, i] = hit[0] is not None and hit[0].z >= z0
    m = ~np.isnan(depth)
    dn, df = np.nanmin(depth), np.nanmax(depth)
    img = np.where(m, 40 + 215 * (df - depth) / max(df - dn, 1e-6), 0).astype(np.uint8)
    save(img, os.path.join(out, f'depth_{slot}_{name}.png'))
    save((m * 255).astype(np.uint8), os.path.join(out, f'mask_{slot}_{name}.png'))
    save((bare * 255).astype(np.uint8), os.path.join(out, f'body_mask_{slot}_{name}.png'))
    ys, xs = np.nonzero(m)
    meta['views'][name] = {'body_bbox_px': [int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())]}
    print('VIEW', name, meta['views'][name])
json.dump(meta, open(os.path.join(out, f'{slot}_meta.json'), 'w'), indent=1)
