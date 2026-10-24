"""Track a two-view video's joints and rebuild them in 3D.

    python track_solve.py VIDEO JOINTS_JSON OUT_DIR
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


def run(video, joints, out, detector=None):
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
    (out / "motion.json").write_text(json.dumps({
        "fps": fps, "frames": len(pos), "score": round(s, 3), "height_error_pct": round(err_pct, 2),
        "seen": seen, "positions": np.round(pos, 5).tolist()}))
    full = np.concatenate([front / [2, 1], side / [2, 1] + [0.5, 0]], 1)  # back to whole-frame coordinates
    overlay(frames, full, np.concatenate([err, err], 1), seen + seen, j["calib"]["model_height"], out / "overlay.mp4", fps)
    print(f"motion: {len(pos)} frames, score {s:.2f}, height error {err_pct:.1f}% of model height")


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
    print("check ok")


if __name__ == "__main__":
    if sys.argv[1:] == ["--check"]:
        check()
    else:
        argv = sys.argv[1:]
        run(argv[0], argv[1], Path(argv[2]), argv[argv.index("--detector") + 1] if "--detector" in argv else None)
