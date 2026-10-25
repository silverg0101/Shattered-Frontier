"""blender -b -P unimate_bvh.py -- IN.glb OUT.bvh OUT.json
A UniMate clip of a rig on the shared quadruped skeleton (its animated GLB) as a Y-up BVH in centimetres, the form
zoo_retarget.py reads library clips in, so a UniMate clip goes through the same cleanup and checks. OUT.json: frames,
fps, the bones and how far the re-imported first frame lies from the GLB's (round-trip check, metres)."""
import bpy, sys, json, math
from mathutils import Matrix

FPS = 30  # UniMate samples at 30 fps; the glTF importer converts key times at the scene rate

inp, out, outj = sys.argv[sys.argv.index("--") + 1:][:3]
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.render.fps = FPS
bpy.ops.import_scene.gltf(filepath=inp)
arm = next(o for o in sc.objects if o.type == "ARMATURE")
f0, f1 = map(int, arm.animation_data.action.frame_range)
sc.frame_start, sc.frame_end = f0, f1
sc.frame_set(f0)
ref = {b.name: (arm.matrix_world @ b.head).copy() for b in arm.pose.bones}
# The BVH importer turns Y-up into Blender's Z-up; the exporter writes Blender's axes as they are. Stand the armature
# on Y-up first so the round trip keeps the clip upright.
arm.matrix_world = Matrix.Rotation(math.radians(-90), 4, "X") @ arm.matrix_world
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
# Applied into the rest pose: the exporter drops the object's turn for the root's offset, so a GLB whose root bone
# isn't at the origin (a SkinTokens rig, a retargeted clip) came back up to a metre off.
bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
bpy.ops.export_anim.bvh(filepath=out, global_scale=100, frame_start=f0, frame_end=f1, rotate_mode="NATIVE")
bpy.ops.import_anim.bvh(filepath=out, global_scale=0.01)
back = bpy.context.active_object
sc.frame_set(f0)
err = max((back.matrix_world @ back.pose.bones[n].head - p).length for n, p in ref.items() if n in back.pose.bones)
info = {"frames": f1 - f0 + 1, "fps": FPS, "bones": [b.name for b in arm.data.bones], "roundtrip_m": round(err, 5)}
print("UNIMATE_BVH", json.dumps(info))
json.dump(info, open(outj, "w"))
if err > 0.005:
    raise SystemExit(f"BVH round trip moved a bone {err:.4f} m")
