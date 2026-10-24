"""Blender: draw a BVH skeleton as a picture.

    blender -b -P skeleton_sheet.py -- CLIP.bvh OUT.png                 first frame, side view over top view, labelled
    blender -b -P skeleton_sheet.py -- CLIP.bvh OUT.png --frames 8      a strip of N side views across the clip

The first frame, not the BVH rest pose: Zoo-300K rest poses have tails and End Sites pointing anywhere. Bones are tubes, joints balls; blue = left, red = right, grey = centre (by joint name). End Sites are labelled
<joint>_end, as zoo_curate.py names them. Workbench render, white background.
"""
import math, re, sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], Path(args[1])
frames = int(args[args.index("--frames") + 1]) if "--frames" in args else 0
W, H = (480, 360) if frames else (1400, 760)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_anim.bvh(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
sc = bpy.context.scene
f0, f1 = map(int, arm.animation_data.action.frame_range)


def side_of(name):
    n = name.lower()
    return "L" if re.search(r"left|(^|[_.])l([_.]|$)", n) else "R" if re.search(r"right|(^|[_.])r([_.]|$)", n) else "C"


COLOR = {"L": (0.1, 0.3, 0.95, 1), "R": (0.9, 0.1, 0.1, 1), "C": (0.35, 0.35, 0.35, 1)}


def points():
    """{label: (world position, side)} for every joint and End Site, plus the bone segments between them."""
    pts, segs = {}, []
    for pb in arm.pose.bones:
        h, t = arm.matrix_world @ pb.head, arm.matrix_world @ pb.tail
        pts[pb.name] = (h, side_of(pb.name))
        if pb.parent:
            segs.append((pb.parent.name, pb.name))
        if not pb.children:
            pts[f"{pb.name}_end"] = (t, side_of(pb.name))
            segs.append((pb.name, f"{pb.name}_end"))
    return pts, segs


def draw(pts, segs, r):
    made = []
    for side in "LRC":
        cu = bpy.data.curves.new(f"bones{side}", "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = r * 0.45
        for a, b in segs:
            if pts[b][1] == side:
                sp = cu.splines.new("POLY"); sp.points.add(1)
                for p, k in zip(sp.points, (a, b)):
                    p.co = (*pts[k][0], 1)
        o = bpy.data.objects.new(f"bones{side}", cu); o.color = COLOR[side]; sc.collection.objects.link(o); made.append(o)
    ball = bpy.data.meshes.new("ball")
    import bmesh
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=r * 1.3); bm.to_mesh(ball); bm.free()
    for k, (p, side) in pts.items():
        o = bpy.data.objects.new(k, ball); o.location = p; o.color = COLOR[side]; sc.collection.objects.link(o); made.append(o)
    return made


def label(pts, rot, up, size):
    made = []
    for k, (p, side) in pts.items():
        cu = bpy.data.curves.new(f"t_{k}", "FONT"); cu.body = k; cu.size = size
        o = bpy.data.objects.new(f"t_{k}", cu); o.rotation_euler = rot
        o.location = p + up * (size * (0.6 if side != "R" else -1.4)); o.color = COLOR[side]
        sc.collection.objects.link(o); made.append(o)
    return made


def camera(loc, rot, scale):
    c = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); c.data.type = "ORTHO"; c.data.ortho_scale = scale
    c.location, c.rotation_euler = loc, rot; c.data.clip_end = 1e5; sc.collection.objects.link(c); sc.camera = c
    return c


def shot():
    sc.render.filepath = str(out.with_suffix(".tmp.png"))
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(sc.render.filepath); w, h = img.size
    px = np.empty(w * h * 4, np.float32); img.pixels.foreach_get(px); bpy.data.images.remove(img)
    Path(sc.render.filepath).unlink()
    return px.reshape(h, w, 4)


sc.render.engine = "BLENDER_WORKBENCH"
sc.display.shading.light, sc.display.shading.color_type = "FLAT", "OBJECT"
sc.render.resolution_x, sc.render.resolution_y = W, H
sc.view_settings.view_transform = "Standard"
sc.world = bpy.data.worlds.new("w"); sc.world.color = (1, 1, 1)

# framing from every sampled pose: the long horizontal axis is the body's length
sample = [f0 + round(i * (f1 - f0) / frames) for i in range(frames)] if frames else [f0]
allp = []
for fr in sample:
    sc.frame_set(fr)
    allp += [p for p, _ in points()[0].values()]
lo = Vector([min(p[i] for p in allp) for i in range(3)]); hi = Vector([max(p[i] for p in allp) for i in range(3)])
c, size = (lo + hi) / 2, hi - lo
along_x = size.x >= size.y
L = max(size.x, size.y)
r = L * 0.006
far = L * 10
if along_x:  # side camera looks along +Y, top camera down with X across the image
    side_loc, side_rot, side_up = (c.x, c.y - far, c.z), (math.radians(90), 0, 0), Vector((0, 0, 1))
    top_rot = (0, 0, 0)
else:
    side_loc, side_rot, side_up = (c.x + far, c.y, c.z), (math.radians(90), 0, math.radians(90)), Vector((0, 0, 1))
    top_rot = (0, 0, math.radians(90))
side_scale = max(L, size.z * W / H) * 1.25

if frames:
    tiles = []
    for fr in sample:
        sc.frame_set(fr)
        pts, segs = points()
        made = draw(pts, segs, r * 1.5)
        camera(side_loc, side_rot, side_scale); tiles.append(shot())
        for o in made:
            bpy.data.objects.remove(o)
    rows = [np.concatenate(tiles[i:i + 4], axis=1) for i in range(0, len(tiles), 4)]
    if rows[-1].shape != rows[0].shape:
        rows[-1] = np.concatenate([rows[-1], np.ones((H, rows[0].shape[1] - rows[-1].shape[1], 4), np.float32)], axis=1)
    sheet = np.concatenate(rows[::-1], axis=0)  # pixel rows go bottom-up
else:
    sc.frame_set(f0)
    pts, segs = points()
    draw(pts, segs, r)
    fs = L * 0.022
    side_labels = label(pts, side_rot, side_up, fs)
    camera(side_loc, side_rot, side_scale); side = shot()
    for o in side_labels:
        bpy.data.objects.remove(o)
    top_up = Vector((0, 1, 0)) if along_x else Vector((-1, 0, 0))
    label(pts, top_rot, top_up, fs)
    camera((c.x, c.y, c.z + far), top_rot, max(L, (size.y if along_x else size.x) * W / H) * 1.25); top = shot()
    sheet = np.concatenate([top, side], axis=0)  # side on top of the picture
out.parent.mkdir(parents=True, exist_ok=True)
img = bpy.data.images.new("sheet", sheet.shape[1], sheet.shape[0]); img.pixels.foreach_set(sheet.ravel())
img.filepath_raw, img.file_format = str(out), "PNG"; img.save()
print("SHEET", out, "frames", sample)
