"""Blender: a solid stand-in of a curated Zoo-300K animal, rendered as the depth guide for a creature's concept views.

    blender -b -P standin_depth.py -- ANIMAL.json CLIP.bvh SIZE_CM OUTDIR

The clip's first frame (rest poses have tails and End Sites pointing anywhere) read through the animal's role map: one
tapered capsule per bone, a ball per joint, blended into one body (voxel remesh + smooth), turned to face -Y (the front
camera's side), SIZE_CM from nose to tail tip, standing on z=0. Written to OUTDIR: depth_<view>.png (1024 px, near white,
background black, depth stretched over the body's own range - the conventions of tools/Render-BodyDepthViews.py; cameras
on -Y / +X / -X, one ortho scale for all three), standin_<view>.png (shaded preview) and standin.json (ortho scale, bbox).
"""
import json, math, os, sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

# Radii as fractions of the body length (nose to tail root): the tuning knob. A stocky quadruped: the torso is about a
# third of the body length deep, widest at the chest and hips; the neck is as thick as the head is tall; limbs are thick
# at the top and taper to a paw; the tail's base is a third of the hips. A *_tip entry is the radius of the End Site of
# that role (default: the role's own radius, tails: half).
RADII = {"pelvis": 0.160, "spine_01": 0.165, "spine_02": 0.168, "spine_03": 0.175, "neck": 0.100,
         "head": 0.115, "head_tip": 0.040,
         "tail_01": 0.055, "tail_02": 0.045, "tail_03": 0.036, "tail_04": 0.028, "tail_05": 0.021, "tail_06": 0.015,
         "FL_01": 0.085, "FL_02": 0.060, "FL_03": 0.042, "FL_04": 0.038,
         "HL_01": 0.105, "HL_02": 0.070, "HL_03": 0.045, "HL_04": 0.038}
for _s in ("FR", "HR"):
    for _i in range(1, 5):
        RADII[f"{_s}_{_i:02}"] = RADII[f"{_s[0]}L_{_i:02}"]
SHOULDER = 0.82  # front legs hang this far along the pelvis-to-neck line
HEAD_LEN = 0.20  # the head is this share of the body length (nose to the pelvis)
LEG_FRAC = 0.33  # joint height of every leg, of the body length: the belly sits at about 35-40% of the back height
TAIL_MAX = 0.5  # the tail is at most this many body lengths (pelvis to nose): a Zoo-300K rat's runs 2.3 x and a buffalo's 1 x, which drew stiff long tails on the concept
HEAD_BLOB = 0.4  # a ball of the head's radius this far along the head (0 = at the joint, 1 = at the nose)
VOXEL = 1 / 250  # remesh voxel, of the whole length
S = 1024

args = sys.argv[sys.argv.index("--") + 1:]
animalf, clipf, size_cm, out = args[0], args[1], float(args[2]), args[3]
out = os.path.abspath(out)
os.makedirs(out, exist_ok=True)
animal = json.load(open(animalf))
role = animal["role_map"]
bird = animal.get("skeleton") == "grounded_bird_v1"
if bird:
    from bird_proxy import folded_wings, foot_anchors

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_anim.bvh(filepath=clipf, global_scale=0.01)
arm = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
sc = bpy.context.scene

# Nodes: a joint, or the End Site of a leaf bone; edges join a bone's joint to each mapped child (or its End Site).
# Only the bones' lengths are used: a clip's frames and the BVH rest pose have tails and limbs pointing anywhere, so the
# stand-in is laid out in the stance the concept asks for (spine level, legs straight down, tail straight back).
bones = arm.data.bones
rest = lambda n: bones[n[:-4]].tail_local if n.endswith("_end") else bones[n].head_local
rad_role, edges = {}, []
mapped = lambda b: role.get(b.name, "root") != "root"
for b in bones:
    if not mapped(b):
        continue
    rad_role[b.name] = role[b.name]
    up = b.parent
    while up is not None and not mapped(up):  # a bone the role map skips (a clavicle, a pastern) joins its neighbours
        up = up.parent
    if up is not None:
        edges.append((up.name, b.name))
    if not any(mapped(d) for d in b.children_recursive):
        rad_role[f"{b.name}_end"] = role.get(f"{b.name}_end", role[b.name])
        edges.append((b.name, f"{b.name}_end"))
role_of = lambda n: rad_role[n]
kids = {}
for a, b in edges:
    kids.setdefault(a, []).append(b)
nose = next(n for n in rad_role if n.endswith("_end") and role_of(n) == "head")
pelvis = next(n for n in rad_role if role_of(n) == "pelvis")
root_t = next((n for n in rad_role if role_of(n) == "tail_01"), pelvis)
is_leg = lambda n: role_of(n)[:2] in ("FL", "FR", "HL", "HR")
is_wing = lambda n: role_of(n).startswith(("WL_", "WR_"))
is_tail = lambda n: role_of(n).startswith("tail")

# pass 1: forward (-Y) along the spine and head, +Y along the tail, x=0, legs as the drop below their root joint
lay, drop, leg_root, parent = {pelvis: Vector()}, {}, {}, {}
stack = [pelvis]
while stack:
    a = stack.pop()
    for b in kids.get(a, []):
        ln = (rest(b) - rest(a)).length
        parent[b] = a
        if is_leg(b):
            leg_root[b] = leg_root[a] if is_leg(a) else b
            drop[b] = drop[a] + ln if is_leg(a) else 0.0
            lay[b] = Vector(lay[a])
        else:
            lay[b] = lay[a] + Vector((0, 1 if is_tail(b) or is_wing(b) else -1, 0)) * ln
        stack.append(b)
head_j = next(n for n in lay if role_of(n) == "head" and not n.endswith("_end"))
lay[nose].y = lay[head_j].y - HEAD_LEN / (1 - HEAD_LEN) * abs(lay[head_j].y)  # the Zoo bones' head is too long or short
for n in lay:  # shoulders sit at the chest, just behind the neck: the Zoo bones may hang them from mid-spine
    if is_leg(n) and role_of(leg_root[n])[0] == "F":
        lay[n].y = lay[head_j].y * SHOULDER
lb = abs(lay[nose].y - lay[root_t].y)
roots = {r: max(d for n, d in drop.items() if leg_root[n] == r) for r in set(leg_root.values())}
mean = lambda v: sum(v) / len(v)
h = LEG_FRAC * lb  # level spine and short legs: the Zoo bones' straight legs are stilts, their bends are not drawn
for n in drop:
    drop[n] *= h / roots[leg_root[n]]
zspine = lambda n: h
pos = {}
for n, p in lay.items():
    if is_leg(n):
        pos[n] = Vector((p.x, p.y, zspine(parent[leg_root[n]]) - drop[n]))
    else:
        pos[n] = Vector((p.x, p.y, zspine(n)))

body_len = abs(lay[nose].y)
tail_len = max([lay[n].y for n in lay if is_tail(n)] or [0])
if tail_len > TAIL_MAX * body_len:
    for n in lay:
        if is_tail(n):
            pos[n].y *= TAIL_MAX * body_len / tail_len
if bird:
    for n in pos:
        if role_of(n) in ("neck", "head"):
            pos[n].z += abs(lay[n].y) / max(body_len, 1e-9) * h
    shoulder_y = min(p.y for n,p in pos.items() if role_of(n).startswith("spine"))
    wing_pos, wing_radii = folded_wings(pos,rad_role,(pos[pelvis],RADII["pelvis"]*lb),shoulder_y)
    pos.update({n:Vector(p) for n,p in wing_pos.items()})
scale = size_cm / 100 / (max(p.y for p in pos.values()) - min(p.y for p in pos.values()))  # nose to tail tip, as the creature job sizes the rig
pos = {n: p * scale for n, p in pos.items()}
L = size_cm / 100
Lb = abs(pos[nose].y - pos[root_t].y)  # nose to tail root: the radii's unit


def radius(n):
    r = role_of(n)
    if bird and is_wing(n):
        return wing_radii[n]*scale
    if n.endswith("_end") and f"{r}_tip" in RADII:
        return RADII[f"{r}_tip"] * Lb
    return RADII[r] * Lb * (0.5 if n.endswith("_end") and r.startswith("tail") else 1)


for n in pos:  # left legs on +X (the animal faces -Y), a little apart: shoulders and hips are not on the midline
    if is_leg(n):
        pos[n].x = (1 if role_of(n)[1] == "L" else -1) * 0.7 * radius(parent[leg_root[n]])
floor = min(p.z - radius(n) for n, p in pos.items())
pos = {n: p - Vector((0, 0, floor)) for n, p in pos.items()}
feet = foot_anchors(pos,rad_role,radius) if bird else []


bm = bmesh.new()
for n, p in pos.items():
    bmesh.ops.create_uvsphere(bm, u_segments=16, v_segments=10, radius=radius(n), matrix=Matrix.Translation(p))
for a, b in edges:
    d = pos[b] - pos[a]
    if d.length < 1e-6:
        continue
    m = Matrix.Translation((pos[a] + pos[b]) / 2) @ d.to_track_quat("Z", "Y").to_matrix().to_4x4()
    r1,r2 = radius(a),radius(b)
    if bird and is_wing(b) and not is_wing(a):
        r1 = r2  # no torso-sized cone protruding into a round shoulder lobe
    bmesh.ops.create_cone(bm, cap_ends=True, segments=16, radius1=r1, radius2=r2, depth=d.length, matrix=m)
for foot in feet:
    matrix = Matrix.Translation(Vector(foot["centre"])) @ Matrix.Diagonal((*foot["radii"],1))
    bmesh.ops.create_uvsphere(bm,u_segments=16,v_segments=10,radius=1,matrix=matrix)
for a, b in edges:  # a plump belly: a ball amid each torso segment, a little wider than its ends
    if all(role_of(n)[:2] in ("pe", "sp") for n in (a, b)):
        bmesh.ops.create_uvsphere(bm, u_segments=16, v_segments=10, radius=1.12 * (radius(a) + radius(b)) / 2,
                                  matrix=Matrix.Translation((pos[a] + pos[b]) / 2))
head_n = next(n for n in pos if role_of(n) == "head" and not n.endswith("_end"))
bmesh.ops.create_uvsphere(bm, u_segments=16, v_segments=10, radius=RADII["head"] * Lb,
                          matrix=Matrix.Translation(pos[head_n].lerp(pos[nose], HEAD_BLOB)))
me = bpy.data.meshes.new("standin")
bm.to_mesh(me)
bm.free()
ob = bpy.data.objects.new("standin", me)
sc.collection.objects.link(ob)
mod = ob.modifiers.new("remesh", "REMESH")
mod.mode, mod.voxel_size = "VOXEL", L * VOXEL
sm = ob.modifiers.new("smooth", "SMOOTH")
sm.factor, sm.iterations = 0.8, 12
dg = bpy.context.evaluated_depsgraph_get()
body = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
v = np.empty(len(body.vertices) * 3, np.float32)
body.vertices.foreach_get("co", v)
v = v.reshape(-1, 3)
v[:, 2] -= v[:, 2].min()  # stands on the floor
faces = [tuple(p.vertices) for p in body.polygons]
tree = BVHTree.FromPolygons([tuple(x) for x in v], faces, epsilon=0.0)
lo, hi = v.min(0), v.max(0)
ctr = Vector(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2))
size = float(max(hi - lo)) * 1.12  # one ortho scale for all three views


def save(g, path):  # g: HxW uint8, row 0 on top
    im = bpy.data.images.new("t", S, S, alpha=False)
    f = np.repeat(g[::-1, :, None] / 255.0, 4, axis=2).astype(np.float32)
    f[..., 3] = 1
    im.pixels.foreach_set(f.ravel())
    im.filepath_raw, im.file_format = path, "PNG"
    im.save()
    bpy.data.images.remove(im)


meta = {"ortho_scale_m": size, "size_cm": size_cm, "bbox_m": [list(map(float, lo)), list(map(float, hi))], "views": {}}
if bird:
    meta["proxy"] = {"profile":"folded-bird-v1","feet":feet,
                     "wings":{n:{"role":role_of(n),"point":list(pos[n]),"radius":radius(n)} for n in wing_pos}}
for name, az in (("front", 0), ("left", 90), ("right", 270)):
    a = math.radians(az)
    d = Vector((math.sin(a), -math.cos(a), 0))
    right = Vector((math.cos(a), math.sin(a), 0))
    depth = np.full((S, S), np.nan, np.float32)
    shade = np.zeros((S, S), np.float32)
    # only the rows and columns the body can reach (its bbox corners projected on the view)
    cs = [Vector((x, y, z)) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    us = [((c - ctr).dot(right) / size + 0.5) * S for c in cs]
    vs = [(0.5 - (c.z - ctr.z) / size) * S for c in cs]
    i0, i1, j0, j1 = max(0, int(min(us)) - 2), min(S, int(max(us)) + 3), max(0, int(min(vs)) - 2), min(S, int(max(vs)) + 3)
    for j in range(j0, j1):
        z = ctr.z + (0.5 - (j + 0.5) / S) * size
        for i in range(i0, i1):
            o = ctr + right * ((i + 0.5) / S - 0.5) * size + Vector((0, 0, z - ctr.z)) + d * 5
            hit = tree.ray_cast(o, -d)
            if hit[0] is not None:
                depth[j, i] = hit[3]
                shade[j, i] = 0.3 + 0.7 * abs(hit[1].dot(d))
    m = ~np.isnan(depth)
    dn, df = np.nanmin(depth), np.nanmax(depth)
    save(np.where(m, 40 + 215 * (df - depth) / max(df - dn, 1e-6), 0).astype(np.uint8), os.path.join(out, f"depth_{name}.png"))
    save(np.where(m, 255 * shade, 0).astype(np.uint8), os.path.join(out, f"standin_{name}.png"))
    ys, xs = np.nonzero(m)
    meta["views"][name] = {"bbox_px": [int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())]}
    print("VIEW", name, meta["views"][name])
json.dump(meta, open(os.path.join(out, "standin.json"), "w"), indent=1)
