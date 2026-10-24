"""One model's joint detector: DINOv2 ViT-S/14 fine-tuned on synth_render.py's images of that model.

    python detector.py train DATA_DIR OUT_DIR [EPOCHS]     # DATA_DIR from synth_render.py; writes weights.pt
    python detector.py --check                              # heatmap and augmentation self-check, CPU only

Input is one image, the front panel left of the side panel (as in the video), so a joint's place in one view
informs the other. The head outputs one heatmap per keypoint per view; a peak's height doubles as confidence
(trained to 1 where visible, 0.3 where hidden). detect() returns what track_solve.py expects from a tracker.

Training keeps every render on the GPU and augments there: the GPU hosts' CPUs are too small to feed it.
"""
import json, math, random, sys
from pathlib import Path

import numpy as np

DINO = "facebookresearch/dinov2"
PANEL = (392, 448)        # w, h per view: 28 x 32 patches of 14 px; the video's 672x768 panels shrink to this
GRID = (56, 64)           # heatmap cells per view (stride 7)
SIGMA = 1.5               # target Gaussian, in cells
HIDDEN_AMP = 0.3
MEAN, STD = (0.485, 0.456, 0.406), (0.229, 0.224, 0.225)


def build(k):
    import torch
    from torch import nn
    class Net(nn.Module):
        def __init__(self):
            super().__init__()
            self.backbone = torch.hub.load(DINO, "dinov2_vits14", trust_repo=True)
            self.head = nn.Sequential(nn.Conv2d(384, 256, 1), nn.GELU(), nn.Upsample(scale_factor=2, mode="bilinear"),
                                      nn.Conv2d(256, 2 * k, 3, padding=1))
        def forward(self, x):  # x: (B, 3, 448, 784) -> (B, 2k, 64, 112)
            t = self.backbone.forward_features(x)["x_norm_patchtokens"]
            b, n, c = t.shape
            return self.head(t.transpose(1, 2).reshape(b, c, x.shape[2] // 14, x.shape[3] // 14))
    return Net()


def panels(img):
    """A two-view uint8 image (H, W, 3), any size -> (2, 448, 392, 3) uint8 at the network's panel size."""
    import cv2
    half = img.shape[1] // 2
    return np.stack([cv2.resize(p, PANEL, interpolation=cv2.INTER_AREA) for p in (img[:, :half], img[:, half:])])


def to_input(p):
    """(B, 2, 448, 392, 3) float 0..1 tensor -> normalized network input (B, 3, 448, 784)."""
    import torch
    x = torch.cat([p[:, 0], p[:, 1]], 2).permute(0, 3, 1, 2)
    return (x - x.new_tensor(MEAN)[:, None, None]) / x.new_tensor(STD)[:, None, None]


def targets(uv, vis):
    """uv (B, 2, k, 2) in each panel's 0..1, vis (B, 2, k) -> heatmaps (B, 2k, 64, 112): front, then side."""
    import torch
    gw, gh = GRID
    b, _, k, _ = uv.shape
    ys = torch.arange(gh, device=uv.device, dtype=torch.float32)[:, None]
    xs = torch.arange(2 * gw, device=uv.device, dtype=torch.float32)[None]
    cx = (torch.arange(2, device=uv.device)[None, :, None] * gw + uv[..., 0] * gw - 0.5).reshape(b, 2 * k, 1, 1)
    cy = (uv[..., 1] * gh - 0.5).reshape(b, 2 * k, 1, 1)
    amp = torch.where(vis > 0.5, 1.0, HIDDEN_AMP).reshape(b, 2 * k, 1, 1)
    return amp * torch.exp(-((xs - cx) ** 2 + (ys - cy) ** 2) / (2 * SIGMA ** 2))


def soft_uv(maps, k, beta=20.0):
    """Differentiable joint position: softmax over each heatmap's own panel -> uv (B, 2, k, 2)."""
    import torch
    gw, gh = GRID
    b = maps.shape[0]
    m = maps.reshape(b, 2, k, gh, 2, gw)[:, torch.arange(2), :, :, torch.arange(2)].permute(1, 0, 2, 3, 4)
    w = torch.softmax(m.reshape(b, 2, k, -1) * beta, -1).reshape(b, 2, k, gh, gw)
    u = (w.sum(3) * (torch.arange(gw, device=maps.device) + 0.5)).sum(-1) / gw
    v = (w.sum(4) * (torch.arange(gh, device=maps.device) + 0.5)).sum(-1) / gh
    return torch.stack([u, v], -1)


def decode(maps, k):
    """Heatmaps (2k, 64, 112) -> uv per view (each 0..1 of its panel) and peak heights: front first, then side."""
    gw, gh = GRID
    out, conf = np.zeros((2, k, 2), np.float32), np.zeros((2, k), np.float32)
    for c in range(2 * k):
        panel, j = divmod(c, k)
        m = maps[c][:, panel * gw:(panel + 1) * gw]  # a view's joint is looked for in its own panel only
        y, x = np.unravel_index(int(m.argmax()), m.shape)
        y0, y1, x0, x1 = max(y - 2, 0), min(y + 3, gh), max(x - 2, 0), min(x + 3, gw)
        w = np.clip(m[y0:y1, x0:x1], 0, None)
        yy, xx = np.mgrid[y0:y1, x0:x1]
        s = w.sum() or 1
        out[panel, j] = [((xx * w).sum() / s + 0.5) / gw, ((yy * w).sum() / s + 0.5) / gh]
        conf[panel, j] = min(max(float(m[y, x]), 0), 1)
    return out, conf


def augment(p, uv, g):
    """The look of a generated video, per panel: drift in scale and position, light, colour, gamma, blur and
    noise. p (B, 2, H, W, 3) float 0..1, uv (B, 2, k, 2); g a torch.Generator. Returns both changed.
    ponytail: no JPEG or directional motion blur on the GPU; add them if the exam fails on blurry frames."""
    import torch
    import torch.nn.functional as F
    b, _, h, w, _ = p.shape
    n, dev = b * 2, p.device
    r = lambda lo, hi, *shape: lo + (hi - lo) * torch.rand(*(shape or (n,)), generator=g, device=dev)
    x = p.reshape(n, h, w, 3).permute(0, 3, 1, 2)
    s, tx, ty = r(0.97, 1.03), r(-0.04, 0.04), r(-0.04, 0.04)  # output = s * input + t, in -1..1 coordinates
    theta = torch.zeros(n, 2, 3, device=dev)
    theta[:, 0, 0], theta[:, 1, 1], theta[:, 0, 2], theta[:, 1, 2] = 1 / s, 1 / s, -tx / s, -ty / s
    x = F.grid_sample(x, F.affine_grid(theta, x.shape, align_corners=False), padding_mode="border", align_corners=False)
    q = uv.reshape(n, -1, 2) * 2 - 1
    q = q * s[:, None, None] + torch.stack([tx, ty], -1)[:, None]
    uv = ((q + 1) / 2).reshape(uv.shape)
    x = x * r(0.85, 1.15)[:, None, None, None] * r(0.95, 1.05, n, 3)[:, :, None, None]
    x = ((x - 0.5) * r(0.8, 1.2)[:, None, None, None] + 0.5).clamp(0, 1) ** r(0.8, 1.25)[:, None, None, None]
    sig = r(0.3, 1.5) * (torch.rand(n, generator=g, device=dev) < 0.5)  # half get a Gaussian blur
    t = torch.arange(-3, 4, device=dev, dtype=torch.float32)
    k1 = torch.exp(-t[None] ** 2 / (2 * sig[:, None].clamp(min=1e-3) ** 2))
    k1 = torch.where(sig[:, None] > 0, k1 / k1.sum(1, keepdim=True), (t == 0).float()[None])
    k1 = k1.repeat_interleave(3, 0)
    x = x.reshape(1, n * 3, h, w)
    x = F.conv2d(F.pad(x, (3, 3, 0, 0), mode="replicate"), k1[:, None, None, :], groups=n * 3)
    x = F.conv2d(F.pad(x, (0, 0, 3, 3), mode="replicate"), k1[:, None, :, None], groups=n * 3)
    x = x.reshape(n, 3, h, w) + torch.randn(n, 3, h, w, generator=g, device=dev) * r(2, 6)[:, None, None, None] / 255
    return x.clamp(0, 1).permute(0, 2, 3, 1).reshape(b, 2, h, w, 3), uv


def load(data):
    """Every render at panel size, uv (N, 2, k, 2) and visibility (N, 2, k)."""
    import cv2
    labels = json.loads((data / "labels.json").read_text())
    imgs = np.stack([panels(cv2.cvtColor(cv2.imread(str(data / "img" / f"{i:05d}.png")), cv2.COLOR_BGR2RGB))
                     for i in range(len(labels))])
    uv = np.array([[l["front"], l["side"]] for l in labels], np.float32)
    vis = np.array([[l["vis_front"], l["vis_side"]] for l in labels], np.float32)
    return imgs, uv, vis


def train(data, out, epochs=30):
    import torch
    import torch.nn.functional as F
    data, out = Path(data), Path(out)
    out.mkdir(parents=True, exist_ok=True)
    keys = json.loads((data / "keypoints.json").read_text())
    k = len(keys["points"])
    imgs, uv, vis = load(data)
    imgs, uv, vis = (torch.from_numpy(a).cuda() for a in (imgs, uv, vis))  # ~0.5 GB per 1000 renders
    idx = torch.randperm(len(imgs), generator=torch.Generator().manual_seed(0))
    nval = max(len(idx) // 15, 4)
    val, tr = idx[:nval].cuda(), idx[nval:].cuda()
    net = build(k).cuda()
    print(f"train device: {torch.cuda.get_device_name()}, {len(tr)} images, {k} keypoints", flush=True)
    opt = torch.optim.AdamW([{"params": net.backbone.parameters(), "lr": 5e-5},
                             {"params": net.head.parameters(), "lr": 1e-3}], weight_decay=0.01)
    batch = 16
    steps = epochs * math.ceil(len(tr) / batch)
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=[5e-5, 1e-3], total_steps=steps, pct_start=0.05)
    g = torch.Generator(device="cuda").manual_seed(0)
    for ep in range(epochs):
        net.train()
        order = tr[torch.randperm(len(tr), generator=g, device="cuda")]
        total = 0.0
        for b0 in range(0, len(order), batch):
            ids = order[b0:b0 + batch]
            p, u = augment(imgs[ids].float() / 255, uv[ids], g)
            target = targets(u, vis[ids])
            with torch.autocast("cuda", dtype=torch.bfloat16):
                pred = net(to_input(p)).float()
            # The heatmap error weighted toward the peaks (or "nothing anywhere" scores well), plus where each
            # heatmap says the joint is against where it is.
            heat = ((pred - target) ** 2 * (1 + 20 * target)).mean() * 100
            w = torch.where(vis[ids] > 0.5, 1.0, 0.3)
            su = soft_uv(pred, k)
            coord = ((su - u).abs().sum(-1) * w).mean() * 10
            # Both views measure a joint's height on one scale: read the side panel by the front panel's row.
            # (Relative to the true difference: augmentation drifts each panel a little on its own.)
            both = (vis[ids][:, 0] > 0.5) & (vis[ids][:, 1] > 0.5)
            row = (((su[:, 0, :, 1] - su[:, 1, :, 1]) - (u[:, 0, :, 1] - u[:, 1, :, 1])).abs() * both).mean() * 10
            loss = heat + coord + row
            opt.zero_grad()
            loss.backward()
            opt.step()
            sched.step()
            total += loss.item() * len(ids)
        if (ep + 1) % 5 == 0 or ep == epochs - 1:
            err = evaluate(net, imgs[val], uv[val], vis[val], k)
            print(f"epoch {ep + 1}/{epochs}: loss {total / len(tr):.4f}, val px error median {err[0]:.1f} p95 {err[1]:.1f}", flush=True)
    torch.save({"state": net.state_dict(), "k": k, "keypoints": keys}, out / "weights.pt")
    (out / "val.json").write_text(json.dumps({"median_px": err[0], "p95_px": err[1], "images": len(imgs)}))
    print(f"train peak GPU memory: {torch.cuda.max_memory_allocated() / 2**30:.1f} GB")


def evaluate(net, imgs, uv, vis, k):
    """Pixel error on held-out renders (not augmented), in the 392x448 panel's pixels, visible joints only."""
    import torch
    net.eval()
    errs = []
    with torch.no_grad():
        for i in range(0, len(imgs), 16):
            with torch.autocast("cuda", dtype=torch.bfloat16):
                maps = net(to_input(imgs[i:i + 16].float() / 255)).float().cpu().numpy()
            for b, m in enumerate(maps):
                got, _ = decode(m, k)
                d = np.hypot(*((got - uv[i + b].cpu().numpy()) * PANEL).transpose(2, 0, 1))
                errs += list(d[vis[i + b].cpu().numpy() > 0.5])
    net.train()
    return float(np.median(errs)), float(np.percentile(errs, 95))


def detect(weights_dir, frames, n_points):
    """Video frames (T, H, W, 3) -> ((front, vis_f), (side, vis_s)), tracks (T, n_points, 2) in each panel's own
    0..1 coordinates and peak heights as visibility, the shapes track_solve.track_views returns. Points the
    detector doesn't cover get vis 0 and a placeholder uv; detected marks the ones it does."""
    import torch
    ck = torch.load(Path(weights_dir) / "weights.pt", map_location="cpu", weights_only=False)
    k, keys = ck["k"], ck["keypoints"]["points"]
    net = build(k)
    net.load_state_dict(ck["state"])
    net = net.cuda().eval()
    print(f"detect device: {torch.cuda.get_device_name()}")
    T = len(frames)
    tracks, vis = np.zeros((2, T, n_points, 2), np.float32) + 0.5, np.zeros((2, T, n_points), np.float32)
    with torch.no_grad():
        for t0 in range(0, T, 16):
            p = torch.from_numpy(np.stack([panels(f) for f in frames[t0:t0 + 16]])).cuda().float() / 255
            with torch.autocast("cuda", dtype=torch.bfloat16):
                maps = net(to_input(p)).float().cpu().numpy()
            for b, m in enumerate(maps):
                uv, conf = decode(m, k)
                tracks[:, t0 + b, keys] = uv
                vis[:, t0 + b, keys] = conf
    detected = np.zeros(n_points, bool)
    detected[keys] = True
    return ((tracks[0], vis[0]), (tracks[1], vis[1])), detected


def check():
    import torch
    k = 3
    uv = torch.tensor([[[[0.2, 0.3], [0.5, 0.5], [0.8, 0.9]], [[0.1, 0.1], [0.45, 0.6], [0.9, 0.5]]]])
    vis = torch.tensor([[[1., 0, 1], [1, 1, 0]]])
    maps = targets(uv, vis)
    assert maps.shape == (1, 2 * k, GRID[1], 2 * GRID[0])
    got, conf = decode(maps[0].numpy(), k)
    assert np.abs(got - uv[0].numpy()).max() < 0.01, (got, uv)
    assert conf[0, 1] < 0.5 < conf[0, 0] and conf[1, 2] < 0.5 < conf[1, 1]  # hidden joints peak lower
    assert (soft_uv(maps * 5, k) - uv).abs().max() < 0.02  # the training-time decode agrees
    g = torch.Generator().manual_seed(1)
    p = torch.rand(2, 2, 64, 56, 3)
    q, uv2 = augment(p, uv.repeat(2, 1, 1, 1), g)
    assert q.shape == p.shape and uv2.shape == (2, 2, k, 2) and (uv2 - uv).abs().max() < 0.06  # drift is small
    print("check ok")


if __name__ == "__main__":
    if sys.argv[1:] == ["--check"]:
        check()
    elif sys.argv[1] == "train":
        train(sys.argv[2], sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 30)
