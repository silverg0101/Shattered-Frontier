"""Blender: a known motion for testing track_solve.py without a video model.

    blender -b -P selftest_render.py -- MODEL OUT_DIR [FRAMES] [SWING] [--seed S] [--turn DEG]

Renders the rest pose like render_views.py (views.png, joints.json), then swings every bone on a sine up to
SWING times its joint range (synth_render.joint_ranges, the same limits the detector trains within; default
1 = the full range; with --seed each bone peaks at a random 40-100 % of that, as no real motion bends every
joint to its limit at once), renders each frame's two views as OUT_DIR/frames/NNNN.png and writes the true point
positions to OUT_DIR/truth.json and, per frame, which points a camera sees at rest but neither sees now to
OUT_DIR/hidden.json. Stitch the frames into a video and run track_solve.py on it to measure its error.
"""
import json, math, random, sys
from pathlib import Path

import bpy
from mathutils import Euler

sys.path.insert(0, str(Path(__file__).parent))
import render_views as rv
import synth_render as sr


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    model, out = argv[0], Path(argv[1])
    frames = int(argv[2]) if len(argv) > 2 else 48
    amp = float(argv[3]) if len(argv) > 3 and not argv[3].startswith("--") else 1.0  # fraction of each joint's range
    # --seed: random phase per bone, so an exam is not the motion a detector was trained on
    rng = random.Random(int(argv[argv.index("--seed") + 1])) if "--seed" in argv else None
    out.mkdir(parents=True, exist_ok=True)
    turn = float(argv[argv.index("--turn") + 1]) if "--turn" in argv else 0.0
    arm = rv.load(model, turn)
    lo, hi = rv.bounds(arm)
    for o in bpy.context.scene.objects:  # widgets hidden from the render must not block the visibility rays either
        if o.type == "MESH" and o.hide_render:
            o.hide_viewport = True
    pts, bones = rv.points(arm)
    cams, calib = rv.setup(lo, hi)
    image = rv.render(cams, out / "views.png")
    uv = rv.project(cams, pts)
    (out / "joints.json").write_text(json.dumps({
        "calib": calib, "turn": turn, "points": [list(p) for p in pts], "bones": bones, "uv": uv, "seen": rv.seen(image, uv)}))

    bpy.context.scene.cycles.samples = 16
    (out / "frames").mkdir(exist_ok=True)
    truth, hidden = [], []
    scene, height = bpy.context.scene, calib["model_height"]
    base = {pb.name: pb.matrix_basis.copy() for pb in arm.pose.bones}  # the swing rides on the imported pose
    arm.animation_data_clear()  # an imported pose often comes as an action, which would reset the swing on render
    phases = [(rng.uniform(0, 2 * math.pi), rng.uniform(0, 2 * math.pi), rng.uniform(0.4, 1.0)) if rng
              else (i, 0.7 * i, 1.0) for i in range(len(arm.pose.bones))]
    ranges = sr.joint_ranges(arm, sr.bone_lengths(arm, calib["model_height"]), calib["model_height"])
    in_view = lambda: [any(sr.visible(scene, cams[v], arm.matrix_world @ p, height) for v in ("front", "side"))
                       for b in arm.pose.bones for p in (b.head, b.tail)]
    # A joint counts as hidden when a camera sees it at rest but neither does now (a foot behind the body);
    # joints buried in the model's own bulk (pelvis, spine) are never seen and are graded as usual.
    at_rest = in_view()
    for f in range(frames):
        ph = 2 * math.pi * f / frames
        for i, pb in enumerate(arm.pose.bones):
            if not pb.parent:  # a root bone spins the whole model about the floor: no video shows that
                continue
            a, b, k = phases[i]
            r = k * amp * math.radians(ranges[pb.name]) / 2  # the bracket below spans -2..2: peaks at the range
            swing = Euler((r * (math.sin(ph + a) - math.sin(a)), 0, r * (math.cos(ph + b) - math.cos(b))))
            pb.matrix_basis = base[pb.name] @ swing.to_matrix().to_4x4()
        bpy.context.view_layer.update()
        truth.append([list(arm.matrix_world @ p) for b in arm.pose.bones for p in (b.head, b.tail)])
        hidden.append([rest and not now for rest, now in zip(at_rest, in_view())])
        rv.render(cams, out / "frames" / f"{f:04d}.png")
    # truth is per bone (head, tail); map it onto joints.json's deduplicated points
    order = {}
    for k, b in enumerate(bones):
        order.setdefault(b["head"], 2 * k)
        order.setdefault(b["tail"], 2 * k + 1)
    (out / "truth.json").write_text(json.dumps([[t[order[i]] for i in range(len(pts))] for t in truth]))
    (out / "hidden.json").write_text(json.dumps([[t[order[i]] for i in range(len(pts))] for t in hidden]))
    print(f"selftest: {frames} frames, {len(pts)} points")


if __name__ == "__main__":
    main()
