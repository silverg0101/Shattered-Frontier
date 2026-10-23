"""Blender: bake solved joint positions onto the model's skeleton and export it animated.

    blender -b -P bake.py -- MODEL JOINTS_JSON MOTION_JSON OUT.glb

Per frame, parents first, each bone is turned so it points at its solved tail. A bone with a child off its
axis (pelvis to thighs, chest to clavicles, hand to fingers) also turns about that axis so the child lands
where it was tracked; other bones keep the twist their parent gives them. Connected bones stay where their
parent puts them, so bone lengths never change; free bones (the pelvis) and the root follow their tracked
head. Points that land on empty background (joints.json "seen") steer nothing. The model is exported as GLB
with that one animation.
"""
import json, math, sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).parent))
from render_views import load  # same import and turn, so world positions line up

MIN_OFFSET = 0.1  # an off-axis child counts for twist when it sits this far (x bone length) from the axis


def frame_of(a, b):
    """Orthonormal basis from a main direction a and a helper direction b (only its part across a counts)."""
    x = a.normalized()
    y = (b - b.project(x)).normalized()
    return Matrix((x, y, x.cross(y))).transposed()


def main():
    model, joints, motion, out = sys.argv[sys.argv.index("--") + 1:][:4]
    j = json.loads(Path(joints).read_text())
    m = json.loads(Path(motion).read_text())
    arm = load(model, j["turn"])
    scene = bpy.context.scene
    scene.render.fps = round(m["fps"])
    scene.frame_start, scene.frame_end = 1, m["frames"]

    to_arm = arm.matrix_world.inverted()
    bones = {b["name"]: b for b in j["bones"]}
    rest = {pb.name: pb.matrix.copy() for pb in arm.pose.bones}  # frame 0 = the rendered pose, armature space
    pts0 = [to_arm @ Vector(p) for p in j["points"]]
    seen = j.get("seen") or [True] * len(pts0)  # points on empty background can't steer anything
    twist = {}  # bone -> point index of the off-axis child that fixes its twist
    for pb in arm.pose.bones:
        b, h0 = bones[pb.name], pts0[bones[pb.name]["head"]]
        axis = pts0[b["tail"]] - h0
        best = max(((c - c.project(axis)).length, bones[ch.name]["head"]) for ch in pb.children
                   for c in [pts0[bones[ch.name]["head"]] - h0]) if pb.children and axis.length else (0, None)
        if axis.length and best[0] > MIN_OFFSET * axis.length:
            twist[pb.name] = best[1]

    arm.animation_data_clear()  # an imported pose action would fight the keys
    order = [pb for pb in arm.pose.bones]
    order.sort(key=lambda pb: len(pb.parent_recursive))
    for f, frame in enumerate(m["positions"], 1):
        pts = [to_arm @ Vector(p) for p in frame]
        want = {}  # bone -> desired armature-space matrix this frame
        for pb in order:
            b = bones[pb.name]
            h0, t0 = pts0[b["head"]], pts0[b["tail"]]
            if pb.parent:  # where the parent now puts this bone's head
                base = want[pb.parent.name] @ rest[pb.parent.name].inverted() @ rest[pb.name]
                head = base.to_translation()
                spin = (want[pb.parent.name].to_3x3() @ rest[pb.parent.name].to_3x3().inverted())
                if not pb.bone.use_connect and seen[b["head"]]:  # a free bone (pelvis) goes where it was tracked
                    head = pts[b["head"]]
            else:  # the root: tracked if it's on the model, else it stays put (a floor root between the feet)
                head, spin = (pts[b["head"]] if seen[b["head"]] else h0), Matrix.Identity(3)
            # Aim from where the head really is, so one bone's error is corrected by the next, not passed on.
            a0, a = t0 - h0, pts[b["tail"]] - head
            if not a0.length or not a.length or not seen[b["tail"]]:
                rot = spin
            elif pb.name in twist and seen[twist[pb.name]]:
                c = twist[pb.name]
                rot = frame_of(a, pts[c] - head) @ frame_of(a0, pts0[c] - h0).transposed()
            else:
                rot = (spin @ a0).rotation_difference(a).to_matrix() @ spin
            want[pb.name] = Matrix.Translation(head) @ rot.to_4x4() @ Matrix.Translation(-h0) @ rest[pb.name]
        for pb in order:
            if pb.parent:
                parent = want[pb.parent.name] @ pb.parent.bone.matrix_local.inverted() @ pb.bone.matrix_local
            else:
                parent = pb.bone.matrix_local
            pb.matrix_basis = parent.inverted() @ want[pb.name]
            pb.rotation_mode = "QUATERNION"
            pb.keyframe_insert("rotation_quaternion", frame=f)
            if not pb.bone.use_connect:  # the root, and posed offsets such as an imported pelvis drop
                pb.keyframe_insert("location", frame=f)
    baked = arm.animation_data.action
    baked.name = Path(out).stem
    for a in [a for a in bpy.data.actions if a != baked]:  # a model's own clips (idle, walk) would be exported instead
        bpy.data.actions.remove(a)
    if j["turn"]:  # undo the render turn; the keys are relative to the armature, so they turn with it
        for o in [o for o in scene.objects if o.parent is None]:
            o.rotation_euler.z -= math.radians(j["turn"])
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_animations=True,
                              export_animation_mode="ACTIVE_ACTIONS", export_force_sampling=True,
                              export_frame_range=True)
    print(f"baked {m['frames']} frames to {out}")


if __name__ == "__main__":
    main()
