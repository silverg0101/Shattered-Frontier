"""Track a two-view video's joints and rebuild them in 3D.

    python track_solve.py VIDEO JOINTS_JSON OUT_DIR [--detector DIR] [--loop]
    python track_solve.py --check          # self-check, no GPU or video needed

The left half of every frame is the front view (world x right, z up), the right half the left side (world y
right, z up), both orthographic and framed as render_views.py recorded in joints.json. CoTracker3 follows
each bone point from its frame-0 position in both halves. Front gives x and z, side gives y and z; z comes
from both, and how well those two heights agree is the take's score.

Writes OUT_DIR/motion.json (per-frame world positions, fps, score) and OUT_DIR/overlay.mp4.
"""
import json, sys
from pathlib import Path

import numpy as np

COTRACKER = "facebookresearch/co-tracker:82e02e8029753ad4ef13cf06be7f4fc5facdda4d"
SMOOTH = 5          # frames in the moving average over solved positions
GOOD_ERR = 0.05     # height disagreement (fraction of model height) that scores 0
HOLD = 0.2          # detector confidence below which a joint keeps its last place
LOOP_MIN = 0.8      # seconds: shortest loop a looping action is cut to (a walk's two steps)
VEL_WEIGHT = 4      # frames: how much a velocity mismatch at the loop's seam counts against a pose mismatch


def read_video(path):
    import av
    with av.open(str(path)) as c:
        s = c.streams.video[0]
        fps = float(s.average_rate or 24)
        frames = np.stack([f.to_ndarray(format="rgb24") for f in c.decode(s)])
    return frames, fps


def track(model, frames, uv):
    """frames (T,H,W,3) uint8, uv (N,2) normalized to the frames -> tracks (T,N,2) normalized, visible (T,N)."""
    import torch
    T, h, w, _ = frames.shape
    video = torch.from_numpy(frames).permute(0, 3, 1, 2)[None].float().cuda()
    q = torch.zeros(1, len(uv), 3, device="cuda")
    q[0, :, 1], q[0, :, 2] = torch.tensor(uv[:, 0] * w), torch.tensor(uv[:, 1] * h)  # (frame, x, y)
    with torch.no_grad():
        tr, vis = model(video, queries=q)
    tr = tr[0].cpu().numpy() / np.array([w, h])
    del video
    torch.cuda.empty_cache()
    return tr, vis[0].cpu().numpy().astype(np.float32)


def track_views(frames, j):
    """Front and side halves tracked one at a time: CoTracker scales its input to 512 px wide, so a half gets
    twice the detail of the whole frame, and only one half is on the GPU at once. Tracks come back in each
    half's own 0..1 coordinates."""
    import torch
    model = torch.hub.load(COTRACKER, "cotracker3_offline", trust_repo=True).cuda().eval()  # no GPU: fails here
    print(f"track device: {torch.cuda.get_device_name()}")
    half = frames.shape[2] // 2
    out = []
    for panel, key in enumerate(("front", "side")):
        uv = (np.array(j["uv"][key]) - [0.5 * panel, 0]) * [2, 1]
        out.append(track(model, np.ascontiguousarray(frames[:, :, panel * half:(panel + 1) * half]), uv))
    print(f"track peak GPU memory: {torch.cuda.max_memory_allocated() / 2**30:.1f} GB")
    return out


def solve(front, side, vis_f, vis_s, calib):
    """Normalized tracks per half (T,N,2, each in 0..1 of its own panel) -> world positions (T,N,3), height error (T,N)."""
    cx, cy, cz = calib["center"]
    pw, ph = calib["width"], calib["height"]
    x = cx + (front[..., 0] - 0.5) * pw
    y = cy + (side[..., 0] - 0.5) * pw
    zf = cz + (0.5 - front[..., 1]) * ph
    zs = cz + (0.5 - side[..., 1]) * ph
    wf, ws = vis_f + 1e-3, vis_s + 1e-3   # a point hidden in one view leans on the other
    z = (wf * zf + ws * zs) / (wf + ws)
    return np.stack([x, y, z], -1), np.abs(zf - zs)


def smooth(pos, k=SMOOTH):
    if len(pos) < k:
        return pos
    pad = np.concatenate([pos[:1].repeat(k // 2, 0), pos, pos[-1:].repeat(k // 2, 0)])
    kern = np.ones(k) / k
    return np.apply_along_axis(lambda s: np.convolve(s, kern, "valid"), 0, pad)


def score(err, vis_f, vis_s, model_height):
    both = (vis_f > 0.5) & (vis_s > 0.5)
    med = float(np.median(err[both])) if both.any() else float(np.median(err))
    rel = med / model_height
    return max(0.0, 1 - rel / GOOD_ERR), 100 * rel


def overlay(frames, tracks, err, seen, model_height, path, fps):
    import av
    T, h, w, _ = frames.shape
    with av.open(str(path), "w") as c:
        s = c.add_stream("libx264", rate=round(fps))
        s.width, s.height, s.pix_fmt = w - w % 2, h - h % 2, "yuv420p"
        for t in range(T):
            img = frames[t, :s.height, :s.width].copy()
            for (u, v), e in [(p, e) for p, e, on in zip(tracks[t], err[t], seen) if on]:
                px, py = int(u * w), int(v * h)
                bad = e / model_height > GOOD_ERR
                img[max(py - 3, 0):py + 4, max(px - 3, 0):px + 4] = (255, 60, 60) if bad else (60, 220, 255)
            for p in s.encode(av.VideoFrame.from_ndarray(img, format="rgb24")):
                c.mux(p)
        for p in s.encode():
            c.mux(p)


def hold(tracks, vis, floor=HOLD):
    """A joint the detector is unsure of keeps its last confident place (its low confidence stays, so solve()
    leans on the other view)."""
    tracks = tracks.copy()
    for t in range(1, len(tracks)):
        weak = vis[t] < floor
        tracks[t, weak] = tracks[t - 1, weak]
    return tracks


def median3(pos):
    """3-frame median: removes one-frame flips (left and right foot swapped for a frame) before smoothing."""
    if len(pos) < 3:
        return pos
    pad = np.concatenate([pos[:1], pos, pos[-1:]])
    return np.median(np.stack([pad[:-2], pad[1:-1], pad[2:]]), 0)


def loop_cut(pos, fps, seen):
    """The stretch of a looping action that loops best -> (start, end), end excluded. Skips the opening fifth
    (the video starts on the rest pose, then sets off) and ends where pose and velocity come back to the
    start's, at least LOOP_MIN seconds later."""
    T = len(pos)
    p = pos[:, np.array(seen)]
    p = p - p.mean(1, keepdims=True)  # the pose about its centre: drift across the floor doesn't count
    v = np.diff(p, axis=0)
    cost = lambda a, b: np.abs(p[a] - p[b]).mean() + VEL_WEIGHT * np.abs(v[a] - v[b]).mean()
    pairs = [(a, b) for a in range(T // 5, T // 2) for b in range(a + int(LOOP_MIN * fps), T - 1)]
    return min(pairs, key=lambda ab: cost(*ab)) if pairs else (0, T)


def close_loop(pos):
    """Spreads the mismatch between the frame after the clip and its first frame over the clip, so the last
    frame leads into the first without a hitch. pos holds the clip plus that one frame after it."""
    d = pos[-1] - pos[0]
    return pos[:-1] - d * (np.arange(len(pos) - 1) / (len(pos) - 1))[:, None, None]


def feet(j):
    """Per foot touching the floor at rest: (leg chain bones from the limb's root down to the ankle, the bones
    of the ankle's subtree). The limb's root is the top bone off the centre line (a hip, a shoulder); the ankle
    is the first bone down the chain that branches (into toes), else the leaf itself. bake.py plants them."""
    P, h = np.array(j["points"]), j["calib"]["model_height"]
    floor = P[:, 2].min()
    by = {b["name"]: b for b in j["bones"]}
    kids = {n: [b for b in j["bones"] if b["parent"] == n] for n in by}
    out = {}
    for b in j["bones"]:
        if kids[b["name"]] or P[b["tail"], 2] - floor > 0.08 * h:
            continue
        chain = [b]  # leaf up to the limb's root
        while chain[-1]["parent"] and abs(P[by[chain[-1]["parent"]]["head"], 0]) > 0.01 * h:
            chain.append(by[chain[-1]["parent"]])
        chain = chain[::-1]
        if abs(P[chain[0]["head"], 0]) <= 0.01 * h:  # a tail or a snout tip, not a leg
            continue
        k = next((i for i, c in enumerate(chain) if len(kids[c["name"]]) > 1), len(chain) - 1)
        sub, stack = [], [chain[k]]
        while stack:
            sub.append(stack.pop())
            stack += kids[sub[-1]["name"]]
        out[chain[k]["name"]] = (chain[:k + 1], sub)
    return list(out.values())


def fabrik(p, lengths, target, rounds=12):
    """Joint chain p (n,3) with its root fixed, bones kept at their lengths, its end pulled towards target."""
    p = p.copy()
    for _ in range(rounds):
        root, p[-1] = p[0].copy(), target
        for i in range(len(p) - 2, -1, -1):
            p[i] = p[i + 1] + (p[i] - p[i + 1]) / max(np.linalg.norm(p[i] - p[i + 1]), 1e-9) * lengths[i]
        p[0] = root
        for i in range(1, len(p)):
            p[i] = p[i - 1] + (p[i] - p[i - 1]) / max(np.linalg.norm(p[i] - p[i - 1]), 1e-9) * lengths[i - 1]
    return p


def run(video, joints, out, detector=None, loop=False):
    out.mkdir(parents=True, exist_ok=True)
    j = json.loads(Path(joints).read_text())
    frames, fps = read_video(video)
    n = len(j["points"])
    seen = j.get("seen") or [True] * n  # points on empty background steer nothing: don't score or draw them
    if detector:  # a detector trained on this model: finds each joint by what it looks like, left and right apart
        import detector as det
        ((front, vis_f), (side, vis_s)), found = det.detect(detector, frames, n)
        front, side = hold(front, vis_f), hold(side, vis_s)
        seen = [bool(a and b) for a, b in zip(seen, found)]  # undetected points (fingers) follow their parent
    else:
        (front, vis_f), (side, vis_s) = track_views(frames, j)
    pos, err = solve(front, side, vis_f, vis_s, j["calib"])
    pos = smooth(median3(pos))
    on = np.array(seen)
    s, err_pct = score(err[:, on], vis_f[:, on], vis_s[:, on], j["calib"]["model_height"])
    cut = None
    if loop:
        a, b = loop_cut(pos, fps, seen)
        cut = {"start": a, "end": b, "seam_pct": round(float(np.abs(pos[b, on] - pos[a, on]).mean()
                                                                / j["calib"]["model_height"] * 100), 2)}
        pos = close_loop(pos[a:b + 1])
    (out / "motion.json").write_text(json.dumps({
        "fps": fps, "frames": len(pos), "score": round(s, 3), "height_error_pct": round(err_pct, 2),
        "loop": cut, "seen": seen, "positions": np.round(pos, 5).tolist()}))
    full = np.concatenate([front / [2, 1], side / [2, 1] + [0.5, 0]], 1)  # back to whole-frame coordinates
    overlay(frames, full, np.concatenate([err, err], 1), seen + seen, j["calib"]["model_height"], out / "overlay.mp4", fps)
    print(f"motion: {len(pos)} frames, score {s:.2f}, height error {err_pct:.1f}% of model height"
          + (f", loop {cut['start']}-{cut['end']} (seam {cut['seam_pct']}%)" if cut else ""))


def check():
    calib = {"center": [0.1, -0.2, 1.0], "width": 2 * 8 / 9, "height": 2.0, "model_height": 1.8}
    rng = np.random.default_rng(1)
    world = rng.uniform(-0.6, 0.6, (10, 7, 3)) + calib["center"]
    cx, cy, cz = calib["center"]
    front = np.stack([0.5 + (world[..., 0] - cx) / calib["width"], 0.5 - (world[..., 2] - cz) / calib["height"]], -1)
    side = np.stack([0.5 + (world[..., 1] - cy) / calib["width"], 0.5 - (world[..., 2] - cz) / calib["height"]], -1)
    ones = np.ones(world.shape[:2], np.float32)
    pos, err = solve(front, side, ones, ones, calib)
    assert np.allclose(pos, world) and err.max() < 1e-9
    side[..., 1] += 0.03                               # side view drifted 3% of frame height
    _, err = solve(front, side, ones, ones, calib)
    s, pct = score(err, ones, ones, calib["model_height"])
    assert abs(pct - 100 * 0.06 / 1.8) < 1e-6 and 0 < s < 1, (s, pct)
    assert np.allclose(smooth(np.ones((9, 2, 3))), 1) and smooth(pos).shape == pos.shape
    flip = np.zeros((7, 1, 3)); flip[3] = 1   # one frame out of place
    assert np.allclose(median3(flip), 0)
    tr, v = np.arange(10, dtype=float).reshape(5, 1, 2), np.array([[1], [0.1], [1], [0.1], [0.1]])
    assert np.allclose(hold(tr, v)[:, 0, 0], [0, 0, 4, 4, 4])
    # A 24-frame cycle after 20 frames of standing still: the cut is whole cycles, past the standing start.
    t = np.arange(120.0)
    walk = np.where(t < 20, 0, np.sin(2 * np.pi * (t - 20) / 24))[:, None, None] * [[[1, 0, 0], [-1, 0, 1]]]
    a, b = loop_cut(walk, 24, [True, True])
    assert a >= 24 and (b - a) % 24 == 0, (a, b)
    clip = close_loop(walk[a:b + 1] + np.linspace(0, 0.1, b - a + 1)[:, None, None])  # plus a slow drift
    seam = np.abs(clip[0] - clip[-1]).max()  # the last frame leads into the first like any other step
    assert seam <= np.abs(np.diff(clip, axis=0)).max() + 1e-9, seam
    # A leg (hip, knee, ankle, toe) off a centre spine, and a tail: one foot, its chain hip to ankle (no branch,
    # so the leaf); lifted 5 cm, the chain keeps its root and bone lengths and reaches the target.
    rest = np.array([[0, 0, 1], [0.2, 0, 1], [0.2, 0.1, 0.5], [0.2, 0, 0.05], [0.2, -0.1, 0], [0, 0.5, 0.02]])
    leg = {"calib": {"model_height": 1.0}, "points": rest.tolist(), "bones": [
        {"name": "spine", "head": 0, "tail": 1, "parent": None}, {"name": "hip", "head": 1, "tail": 2, "parent": "spine"},
        {"name": "shin", "head": 2, "tail": 3, "parent": "hip"}, {"name": "foot", "head": 3, "tail": 4, "parent": "shin"},
        {"name": "tail", "head": 0, "tail": 5, "parent": "spine"}]}
    assert [([c["name"] for c in ch], [c["name"] for c in s]) for ch, s in feet(leg)] == [(["hip", "shin", "foot"], ["foot"])]
    chain = rest[[1, 2, 3]] + [0, 0.02, 0]
    lengths = [np.linalg.norm(rest[2] - rest[1]), np.linalg.norm(rest[3] - rest[2])]
    p = fabrik(chain, lengths, rest[3] + [0, 0, 0.05])
    assert np.allclose(p[0], chain[0]) and np.allclose(p[-1], rest[3] + [0, 0, 0.05], atol=1e-3), p
    assert np.allclose(np.linalg.norm(np.diff(p, axis=0), axis=-1), lengths)
    print("check ok")


if __name__ == "__main__":
    if sys.argv[1:] == ["--check"]:
        check()
    else:
        argv = sys.argv[1:]
        run(argv[0], argv[1], Path(argv[2]), argv[argv.index("--detector") + 1] if "--detector" in argv else None,
            "--loop" in argv)
