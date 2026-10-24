"""blender -b -P wfix.py -- IN.glb OUT.glb : drop skin weights that bleed into a different limb (v100 rat: claws weighted to the other paw, hind toes to the tail)."""
import bpy, sys
src, out = sys.argv[sys.argv.index("--") + 1:]
bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.import_scene.gltf(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE"); o = max((o for o in bpy.data.objects if o.type == "MESH"), key=lambda m: len(m.data.vertices))
roots = {"bone_15": "LF", "bone_27": "RF", "bone_39": "LH", "bone_46": "RH", "bone_53": "T", "bone_6": "H"}
import json  # hierarchy from the annotation: the animated GLB may not keep bone parenting
par = {j["raw"]: j["parent"] for j in json.load(open("/home/raadmin/unimate-eval/UniMate/outputs/rig/ratv100_annotation.json"))["joints"]}
def limb_of(b):
    while b is not None and b not in roots: b = par.get(b)
    return roots.get(b)
limb = {b: limb_of(b) for b in par if limb_of(b)}
gn = {g.index: g.name for g in o.vertex_groups}; n = 0
for v in o.data.vertices:
    tot = {}
    for g in v.groups:
        L = limb.get(gn[g.group]);
        if L: tot[L] = tot.get(L, 0) + g.weight
    if len(tot) < 2: continue
    keep = max(tot, key=tot.get)
    # limb roots (shoulder, thigh, tail base, neck) may blend with neighbours; distal bones may not
    drop = [g.group for g in v.groups if limb.get(gn[g.group]) not in (None, keep) and gn[g.group] not in roots and g.weight > 0]
    for gi in drop: o.vertex_groups[gi].remove([v.index])
    n += bool(drop)
    s = sum(g.weight for g in v.groups)
    if drop and s > 0:
        for g in v.groups: g.weight /= s
print("WFIX vertices cleaned", n)
# pass 2: a vertex bound wholly to the wrong limb (a hind claw on a front finger) -> rebind to its nearest bone
import numpy as np
mw = arm.matrix_world
seg = [(b.name, np.array(mw @ b.head_local), np.array(mw @ b.tail_local)) for b in arm.data.bones if b.name in limb or b.name in par]
def dist(p, a, b):
    ab = b - a; t = np.clip(np.dot(p - a, ab) / max(np.dot(ab, ab), 1e-12), 0, 1); return np.linalg.norm(p - (a + t * ab))
m2 = 0
for v in o.data.vertices:
    tot = {}
    for g in v.groups:
        L = limb.get(gn[g.group])
        if L: tot[L] = tot.get(L, 0) + g.weight
    if not tot: continue
    keep = max(tot, key=tot.get)
    if keep not in ("LF", "RF", "LH", "RH"): continue
    p = np.array(o.matrix_world @ v.co)
    d = sorted((dist(p, a, b), n) for n, a, b in seg)
    near = d[0][1]; Ln = limb.get(near)
    if Ln in (None, keep) or near in roots: continue
    mine = min(x for x, n in d if limb.get(n) == keep)
    if mine < 2.5 * d[0][0]: continue
    for g in list(v.groups): o.vertex_groups[g.group].remove([v.index])
    (o.vertex_groups.get(near) or o.vertex_groups.new(name=near)).add([v.index], 1.0, "REPLACE"); m2 += 1
print("WFIX vertices rebound to nearest bone", m2)
anim = arm.animation_data is not None and arm.animation_data.action is not None
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_animations=anim, export_animation_mode="ACTIVE_ACTIONS", export_force_sampling=True)
print("WFIX wrote", out)
