"""A trained detector's exam: a motion it has not seen, every bone swinging through its full joint range,
filmed and compressed like a real video, tracked, baked onto the skeleton and compared with the truth. Every
new detector, and every change to the tools, has to pass it.

    python exam.py MODEL DETECTOR_DIR OUT_DIR [--seed S] [--turn DEG] [--blender PATH]
                   [--limits '{"median": 2, "p95": 5, "worst_joint": 6, "pair_spacing": 3}'] [--usable 2]

Writes OUT_DIR/exam.json with a band: "good" within the limits, "usable" within usable x the limits (the
detector is still far better than a point tracker there), "failed" beyond. Exits 1 unless good (a developer gate).
"""
import json, subprocess, sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
SIZE = (1344, 768)  # the video model's frame: our panels are stretched to it, as in a real job
FRAMES, SWING = 48, 1.0  # every bone swings up to its full joint range (synth_render.joint_ranges)
LIMITS = {"median": 2.0, "p95": 5.0, "worst_joint": 6.0, "pair_spacing": 3.0}  # % of model height


def stitch(frame_dir, path):
    """Rendered two-view frames -> an mp4 at the video model's size and compression."""
    import av, cv2
    with av.open(str(path), "w") as c:
        s = c.add_stream("libx264", rate=24, options={"crf": "28"})
        s.width, s.height, s.pix_fmt = *SIZE, "yuv420p"
        for f in sorted(Path(frame_dir).glob("*.png")):
            img = cv2.resize(cv2.cvtColor(cv2.imread(str(f)), cv2.COLOR_BGR2RGB), SIZE, interpolation=cv2.INTER_AREA)
            for p in s.encode(av.VideoFrame.from_ndarray(img, format="rgb24")):
                c.mux(p)
        for p in s.encode():
            c.mux(p)


def grade(joints, truth, motion, limits=LIMITS, usable=2.0):
    """Errors on the points the detector steers (seen in motion.json), in % of model height, and the band."""
    j, m = joints, motion
    h = j["calib"]["model_height"]
    pos, true = np.array(m["positions"]), np.array(truth)
    on = np.array(m["seen"])
    err = np.linalg.norm(pos[:, on] - true[:, on], axis=-1) / h * 100
    names = {b["head"]: b["name"] for b in j["bones"]}
    pairs = []
    for i, n in names.items():
        if on[i] and n.endswith("_l"):
            r = next((k for k, v in names.items() if v == n[:-2] + "_r" and on[k]), None)
            if r is not None:  # left/right kept apart: their spacing over time matches the truth
                d = np.linalg.norm(pos[:, i] - pos[:, r], axis=-1) - np.linalg.norm(true[:, i] - true[:, r], axis=-1)
                pairs.append((n[:-2], float(np.abs(d).mean() / h * 100)))
    # Per joint, the error along x (seen by the front view only), y (the side view only: depth) and z (both).
    axes = np.abs(pos[:, on] - true[:, on]).mean(0) / h * 100
    idx = np.where(on)[0]
    per_joint = sorted(({"joint": names.get(int(i), str(i)), "mean": float(err[:, k].mean()),
                         "x": float(axes[k, 0]), "y": float(axes[k, 1]), "z": float(axes[k, 2])}
                        for k, i in enumerate(idx)), key=lambda r: -r["mean"])
    res = {"median": float(np.median(err)), "p95": float(np.percentile(err, 95)),
           "worst_joint": float(err.mean(0).max()), "pair_spacing": max((d for _, d in pairs), default=0.0),
           "worst_pair": max(pairs, key=lambda p: p[1])[0] if pairs else None, "joints": per_joint}
    res["state"] = ("good" if all(res[k] <= v for k, v in limits.items()) else
                    "usable" if all(res[k] <= v * usable for k, v in limits.items()) else "failed")
    res["passed"] = res["state"] == "good"
    res["limits"], res["usable_x"] = dict(limits), usable
    return res


def main():
    argv = sys.argv[1:]
    model, detector, out = argv[0], argv[1], Path(argv[2])
    seed = argv[argv.index("--seed") + 1] if "--seed" in argv else "7"
    turn = argv[argv.index("--turn") + 1] if "--turn" in argv else "0"
    blender = argv[argv.index("--blender") + 1] if "--blender" in argv else str(Path.home() / "blender/blender-4.2.3-linux-x64/blender")
    limits = {**LIMITS, **json.loads(argv[argv.index("--limits") + 1])} if "--limits" in argv else LIMITS
    usable = float(argv[argv.index("--usable") + 1]) if "--usable" in argv else 2.0
    film = out / "film"
    subprocess.run([blender, "-b", "-P", str(HERE / "selftest_render.py"), "--", model, str(film), str(FRAMES), str(SWING),
                    "--seed", seed, "--turn", turn], check=True, capture_output=True)
    stitch(film / "frames", out / "exam.mp4")
    sys.path.insert(0, str(HERE))
    import track_solve
    track_solve.run(out / "exam.mp4", film / "joints.json", out / "track", detector)
    # Grade the baked animation, which is what a job delivers: the bake keeps bone lengths, so a joint the
    # video can't pin (inside the torso, behind it) is held by its neighbours.
    subprocess.run([blender, "-b", "-P", str(HERE / "bake.py"), "--", model, str(film / "joints.json"),
                    str(out / "track" / "motion.json"), str(out / "exam.glb"), "--positions", str(out / "baked.json")],
                   check=True, capture_output=True)
    res = grade(json.loads((film / "joints.json").read_text()), json.loads((film / "truth.json").read_text()),
                json.loads((out / "baked.json").read_text()), limits, usable)
    (out / "exam.json").write_text(json.dumps(res, indent=1))
    w = res["joints"][0]
    print(f"exam {res['state'].upper()}: median {res['median']:.2f}%, p95 {res['p95']:.2f}%, "
          f"worst joint {res['worst_joint']:.2f}% ({w['joint']}: x {w['x']:.1f} y {w['y']:.1f} z {w['z']:.1f}), "
          f"left/right spacing {res['pair_spacing']:.2f}% ({res['worst_pair']})")
    sys.exit(0 if res["passed"] else 1)


if __name__ == "__main__":
    main()
