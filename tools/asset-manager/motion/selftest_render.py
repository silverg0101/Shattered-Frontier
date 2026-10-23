"""Blender: a known motion for testing track_solve.py without a video model.

    blender -b -P selftest_render.py -- MODEL OUT_DIR [FRAMES] [SWING]

Renders the rest pose like render_views.py (views.png, joints.json), then swings every bone a few degrees
on a sine, renders each frame's two views as OUT_DIR/frames/NNNN.png and writes the true point positions
to OUT_DIR/truth.json. Stitch the frames into a video and run track_solve.py on it to measure its error.
"""
import json, math, sys
from pathlib import Path

import bpy
from mathutils import Euler

sys.path.insert(0, str(Path(__file__).parent))
import render_views as rv


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    model, out = argv[0], Path(argv[1])
    frames = int(argv[2]) if len(argv) > 2 else 48
    amp = float(argv[3]) if len(argv) > 3 else 1.0  # 1 = up to 12° per bone, compounding down each chain
    out.mkdir(parents=True, exist_ok=True)
    arm = rv.load(model)
    lo, hi = rv.bounds(arm)
    pts, bones = rv.points(arm)
    cams, calib = rv.setup(lo, hi)
    image = rv.render(cams, out / "views.png")
    uv = rv.project(cams, pts)
    (out / "joints.json").write_text(json.dumps({
        "calib": calib, "turn": 0.0, "points": [list(p) for p in pts], "bones": bones, "uv": uv, "seen": rv.seen(image, uv)}))

    bpy.context.scene.cycles.samples = 16
    (out / "frames").mkdir(exist_ok=True)
    truth = []
    base = {pb.name: pb.matrix_basis.copy() for pb in arm.pose.bones}  # the swing rides on the imported pose
    arm.animation_data_clear()  # an imported pose often comes as an action, which would reset the swing on render
    for f in range(frames):
        ph = 2 * math.pi * f / frames
        for i, pb in enumerate(arm.pose.bones):
            if not pb.parent:  # a root bone spins the whole model about the floor: no video shows that
                continue
            swing = Euler((amp * math.radians(12) * (math.sin(ph + i) - math.sin(i)), 0,
                           amp * math.radians(8) * (math.cos(ph + 0.7 * i) - math.cos(0.7 * i))))
            pb.matrix_basis = base[pb.name] @ swing.to_matrix().to_4x4()
        bpy.context.view_layer.update()
        truth.append([list(arm.matrix_world @ p) for b in arm.pose.bones for p in (b.head, b.tail)])
        rv.render(cams, out / "frames" / f"{f:04d}.png")
    # truth is per bone (head, tail); map it onto joints.json's deduplicated points
    order = {}
    for k, b in enumerate(bones):
        order.setdefault(b["head"], 2 * k)
        order.setdefault(b["tail"], 2 * k + 1)
    (out / "truth.json").write_text(json.dumps([[t[order[i]] for i in range(len(pts))] for t in truth]))
    print(f"selftest: {frames} frames, {len(pts)} points")


if __name__ == "__main__":
    main()
