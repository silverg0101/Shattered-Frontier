"""Blender: bake solved joint positions onto the model's skeleton and export it animated.

    blender -b -P bake.py -- MODEL JOINTS_JSON MOTION_JSON OUT.glb [--positions BAKED_MOTION.json]

Per frame, parents first, each bone is turned so it points at its solved tail, or at the nearest child's
head when the tail isn't tracked. A bone with another child off its axis (pelvis to thighs, chest to
clavicles) also turns about that axis so that child lands where it was tracked; other bones keep the twist
their parent gives them. Only the hips (the root's children) follow their tracked head; every other bone
stays where its parent puts it, so bone lengths never change. Points that are not tracked (motion.json
"seen": empty background, or not covered by the detector) steer nothing. A looping action (motion.json "loop")
has its feet planted on the floor (track_solve.feet finds them). The model is exported as GLB with that one
animation.
"""
import json, math, sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).parent))
from render_views import load  # same import and turn, so world positions line up
from track_solve import fabrik, feet

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
    # points on empty background, or ones the detector does not cover (fingers), steer nothing
    seen = m.get("seen") or j.get("seen") or [True] * len(pts0)
    # Each bone aims at its tail when that is a tracked joint, else at the tracked head of the child nearest its
    # tail: glTF stores no tails, so an imported thigh's tail is a guess but the knee (the calf's head) is real.
    aim, twist = {}, {}  # bone -> point it points at; point of the off-axis child that fixes its twist
    for pb in arm.pose.bones:
        b, h0 = bones[pb.name], pts0[bones[pb.name]["head"]]
        kids = [bones[ch.name]["head"] for ch in pb.children if seen[bones[ch.name]["head"]]]
        if seen[b["tail"]] and b["tail"] != b["head"]:
            aim[pb.name] = b["tail"]
        elif kids:
            aim[pb.name] = min(kids, key=lambda i: (pts0[i] - pts0[b["tail"]]).length)
        if pb.name not in aim or not (pts0[aim[pb.name]] - h0).length:
            aim.pop(pb.name, None)
            continue
        axis = pts0[aim[pb.name]] - h0
        best = max(((pts0[i] - h0 - (pts0[i] - h0).project(axis)).length, i) for i in kids if i != aim[pb.name]) \
            if len(kids) > 1 else (0, None)
        if best[0] > MIN_OFFSET * axis.length:
            twist[pb.name] = best[1]

    legs = feet(j) if m.get("loop") else []  # a looping action (walk, run, idle) stands on the floor
    for chain, _ in legs:  # a leg aims joint to joint, so its joints land exactly where planting puts them
        for c, nxt in zip(chain, chain[1:]):
            aim[c["name"]] = nxt["head"]
            twist.pop(c["name"], None)

    arm.animation_data_clear()  # an imported pose action would fight the keys
    order = [pb for pb in arm.pose.bones]
    order.sort(key=lambda pb: len(pb.parent_recursive))

    def pose(pts):
        want = {}  # bone -> desired armature-space matrix this frame
        for pb in order:
            b = bones[pb.name]
            h0 = pts0[b["head"]]
            if pb.parent:  # where the parent now puts this bone's head
                base = want[pb.parent.name] @ rest[pb.parent.name].inverted() @ rest[pb.name]
                head = base.to_translation()
                spin = (want[pb.parent.name].to_3x3() @ rest[pb.parent.name].to_3x3().inverted())
                # Only the hips (the root's children) move freely, as in motion capture; every other bone stays
                # where its parent puts it, so bone lengths never change and the skin never stretches.
                if pb.parent.parent is None and seen[b["head"]]:
                    head = pts[b["head"]]
            else:  # the root: tracked if it's on the model, else it stays put (a floor root between the feet)
                head, spin = (pts[b["head"]] if seen[b["head"]] else h0), Matrix.Identity(3)
            # Aim from where the head really is, so one bone's error is corrected by the next, not passed on.
            a0 = pts0[aim[pb.name]] - h0 if pb.name in aim else Vector()
            a = pts[aim[pb.name]] - head if pb.name in aim else Vector()
            if not a0.length or not a.length:
                rot = spin
            elif pb.name in twist:
                c = twist[pb.name]
                rot = frame_of(a, pts[c] - head) @ frame_of(a0, pts0[c] - h0).transposed()
            else:
                rot = (spin @ a0).rotation_difference(a).to_matrix() @ spin
            want[pb.name] = Matrix.Translation(head) @ rot.to_4x4() @ Matrix.Translation(-h0) @ rest[pb.name]
        return want

    frames = [[to_arm @ Vector(p) for p in frame] for frame in m["positions"]]
    wants = [pose(pts) for pts in frames]
    # Feet on the floor: a foot that never comes down (a height error) is lowered by its lowest tenth of heights,
    # one that sinks through the floor is lifted onto it, and its leg bends to follow, hip or shoulder fixed.
    # ponytail: the lowest tenth counts as standing; a motion mostly in mid-air (a leap) would be pulled down
    up = np.array(to_arm.to_3x3() @ Vector((0, 0, 1)))  # a world metre up, in armature space
    for chain, sub in legs:
        lengths = [(pts0[n["head"]] - pts0[c["head"]]).length for c, n in zip(chain, chain[1:])]
        foot = [(c["name"], i) for c in sub for i in (c["head"], c["tail"])]
        low = lambda w: min((arm.matrix_world @ (w[n] @ rest[n].inverted() @ pts0[i])).z for n, i in foot)
        d = np.array([low(w) for w in wants]) - min((arm.matrix_world @ pts0[i]).z for _, i in foot)
        dz = np.maximum(d - np.percentile(d, 10), 0) - d
        print(f"planting {chain[-1]['name']}: moved up to {np.abs(dz).max() / j['calib']['model_height'] * 100:.1f}% of height")
        for t, w in enumerate(wants):
            old = np.array([w[c["name"]].to_translation() for c in chain])
            new = fabrik(old, lengths, old[-1] + up * dz[t])
            for k, c in enumerate(chain[1:], 1):
                frames[t][c["head"]] = Vector(new[k])
            for i in {i for _, i in foot} - {chain[-1]["head"]}:  # the foot moves with its ankle
                frames[t][i] = frames[t][i] + Vector(new[-1] - old[-1])
    if legs:
        wants = [pose(pts) for pts in frames]
    for f, want in enumerate(wants, 1):
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
    argv = sys.argv[sys.argv.index("--") + 1:]
    if "--positions" in argv:  # the baked joints, for exam.py to grade what a job actually delivers
        heads = []
        for f in range(1, m["frames"] + 1):
            scene.frame_set(f)
            frame = [list(p) for p in j["points"]]
            for b in j["bones"]:
                frame[b["head"]] = list(arm.matrix_world @ arm.pose.bones[b["name"]].head)
            heads.append(frame)
        Path(argv[argv.index("--positions") + 1]).write_text(json.dumps({**m, "positions": heads}))
    if j["turn"]:  # undo the render turn; the keys are relative to the armature, so they turn with it
        for o in [o for o in scene.objects if o.parent is None]:
            o.rotation_euler.z -= math.radians(j["turn"])
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_animations=True,
                              export_animation_mode="ACTIVE_ACTIONS", export_force_sampling=True,
                              export_frame_range=True)
    print(f"baked {m['frames']} frames to {out}")


if __name__ == "__main__":
    main()
