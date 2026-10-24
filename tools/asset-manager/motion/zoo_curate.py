"""Zoo-300K library curation (stage A). Plain Python + numpy; runs on the GPU host.

    zoo_curate.py PACK.zip OUT_DIR    the best clip per source motion -> OUT_DIR/animal.json (no role_map yet), clips/*.bvh
    zoo_curate.py --role-map OUT_DIR  Claude maps the joints onto the shared quadruped bones (key in ANTHROPIC_API_KEY),
                                      a second call reviews the map against OUT_DIR/skeleton.png; writes role_map.json,
                                      or prints "FAIL: <reason>" and exits 1.

Scoring is ported from proof/pick.py: a clip must keep moving (every 20-frame block averages >= MIN_BLOCK deg/frame of
rotation change) and close well (start vs end pose rotation + paw distance, paws relative to the root). Cyclic motions
get the best loop window (>= MIN_LOOP frames, every paw must lift), cut, residual spread so the last frame equals frame 0,
and are written in place; travel_cm_s keeps their speed. BVH units are taken as cm, Y up.
"""
import base64, json, math, os, re, sys, urllib.error, urllib.request, zipfile
from pathlib import Path

import numpy as np

MIN_BLOCK = 0.05  # deg/frame: a 20-frame block below this is a stall
BLOCK = 20
MIN_LOOP = 15  # frames
MAX_LOOP_DEG = 4.0  # a cyclic clip whose best loop still differs by more than this mean rotation (deg) is dropped
MIN_SWING = 0.1  # every paw lifts at least this share of its leg (root to paw tip) in a loop: sitting and grooming are no cycle
TURN_W, DETOUR_W = 0.05, 0.5  # loop score per degree the root turns / cm it wanders off the straight line
CYCLIC = ("walk", "run", "trot", "gallop", "scamper", "swim", "fly")
EDIT = re.compile(r"_(?:expansion|in_betweening)_edit")
BONES = ["root", "pelvis", "spine_01", "spine_02", "spine_03", "neck", "head", *[f"tail_{i:02d}" for i in range(1, 7)],
         *[f"{l}_{i:02d}" for l in ("FL", "FR", "HL", "HR") for i in range(1, 5)]]
REQUIRED = ["pelvis", "spine_01", "head", *[f"{l}_{i:02d}" for l in ("FL", "FR", "HL", "HR") for i in range(1, 5)]]
MODEL = "claude-opus-5-5"


# ---------------------------------------------------------------- BVH
def parse(text):
    """-> (joints [{name, parent, offset, channels: [(column, kind)]}], frames F x C, frame_time, header text).
    End Sites become joints named <parent>_end with no channels (they are the paw/tail/head tips)."""
    head, motion = text.split("MOTION", 1)
    tok, joints, stack, col, i = head.split(), [], [], 0, 0
    while i < len(tok):
        t = tok[i]
        if t in ("ROOT", "JOINT"):
            joints.append({"name": tok[i + 1], "parent": stack[-1] if stack else None, "channels": []}); i += 2
        elif t == "End":
            joints.append({"name": f"{joints[stack[-1]]['name']}_end", "parent": stack[-1], "channels": []}); i += 2
        elif t == "{":
            stack.append(len(joints) - 1); i += 1
        elif t == "}":
            stack.pop(); i += 1
        elif t == "OFFSET":
            joints[stack[-1]]["offset"] = np.array(tok[i + 1:i + 4], float); i += 4
        elif t == "CHANNELS":
            n = int(tok[i + 1])
            joints[stack[-1]]["channels"] = [(col + k, c) for k, c in enumerate(tok[i + 2:i + 2 + n])]
            col += n; i += 2 + n
        else:
            i += 1
    lines = motion.strip().splitlines()
    ft = float(lines[1].split(":")[1])
    frames = np.array([[float(x) for x in l.split()] for l in lines[2:] if l.strip()]).reshape(-1, col)
    return joints, frames, ft, head


def write(path, head, frames, ft):
    rows = "\n".join(" ".join(f"{v:.6f}" for v in r) for r in frames)
    Path(path).write_text(f"{head}MOTION\nFrames: {len(frames)}\nFrame Time: {ft:.6f}\n{rows}\n")


def _rot(axis, deg):
    a = np.radians(deg); c, s = np.cos(a), np.sin(a); o, z = np.ones_like(a), np.zeros_like(a)
    m = {"X": [[o, z, z], [z, c, -s], [z, s, c]], "Y": [[c, z, s], [z, o, z], [-s, z, c]],
         "Z": [[c, -s, z], [s, c, z], [z, z, o]]}[axis]
    return np.moveaxis(np.array(m), (0, 1), (-2, -1))


def fk(joints, frames):
    """World joint positions, F x J x 3 (frames None: the rest pose, 1 x J x 3)."""
    F = 1 if frames is None else len(frames)
    pos, rot = np.zeros((F, len(joints), 3)), np.zeros((F, len(joints), 3, 3))
    for j, jt in enumerate(joints):
        local_t, local_r = np.broadcast_to(jt["offset"], (F, 3)).copy(), np.broadcast_to(np.eye(3), (F, 3, 3))
        if frames is not None:
            pc = [c for c, k in jt["channels"] if k.endswith("position")]
            if pc:
                local_t = frames[:, pc]
            for c, k in jt["channels"]:
                if k.endswith("rotation"):
                    local_r = local_r @ _rot(k[0], frames[:, c])
        p = jt["parent"]
        if p is None:
            pos[:, j], rot[:, j] = local_t, local_r
        else:
            pos[:, j] = pos[:, p] + np.einsum("fij,fj->fi", rot[:, p], local_t)
            rot[:, j] = rot[:, p] @ local_r
    return pos


def paws(joints, rest):
    """The (up to) four lowest tips in the rest pose, tail and trajectory helpers aside."""
    tips = [j for j, jt in enumerate(joints) if jt["name"].endswith("_end") and not re.search(r"tail|traj", jt["name"], re.I)]
    return sorted(tips, key=lambda j: rest[j, 1])[:4]  # ponytail: height heuristic; the role map is not known yet


def chain(joints, j):
    """Bone length from the root to joint j, from the rest offsets."""
    n = 0.0
    while joints[j]["parent"] is not None:
        n += float(np.linalg.norm(joints[j]["offset"])); j = joints[j]["parent"]
    return n


def _wrap(d):
    d = np.abs(d) % 360
    return np.minimum(d, 360 - d)


# ---------------------------------------------------------------- scoring
def blocks(R):
    """Mean rotation change (deg/frame) of each 20-frame block, the last one anchored at the end so all frames count."""
    d = _wrap(np.diff(R, axis=0)).mean(1)
    if len(d) <= BLOCK:
        return [float(d.mean())] if len(d) else [0.0]
    starts = sorted(set(range(0, len(d) - BLOCK + 1, BLOCK)) | {len(d) - BLOCK})
    return [float(d[s:s + BLOCK].mean()) for s in starts]


def score(joints, frames, cyclic):
    """-> (score, i, j, closure deg) with lower better, or None if the clip stalls or (cyclic) has no loop of at least
    MIN_LOOP frames that closes within MAX_LOOP_DEG."""
    rcols = [c for jt in joints[1:] for c, k in jt["channels"] if k.endswith("rotation")]
    R = frames[:, rcols]
    if len(R) < 2 or min(blocks(R)) < MIN_BLOCK:
        return None
    P = fk(joints, frames)
    feet = P[:, paws(joints, fk(joints, None)[0])] - P[:, :1]  # paws relative to the root
    if not cyclic:
        dr = _wrap(R[0] - R[-1]).mean()
        dp = np.linalg.norm(feet[0] - feet[-1], axis=1).mean() if feet.shape[1] else 0.0
        return float(dr + dp), 0, len(R) - 1, float(dr)
    if len(R) <= MIN_LOOP:
        return None
    root = joints[0]["channels"]
    R0 = np.broadcast_to(np.eye(3), (len(frames), 3, 3))
    for c, k in root:
        if k.endswith("rotation"):
            R0 = R0 @ _rot(k[0], frames[:, c])
    yaw = np.degrees(np.unwrap(np.arctan2(R0[:, 0, 2], R0[:, 2, 2])))  # where the root's +Z points, about Y
    xz = P[:, 0][:, [0, 2]]
    min_swing = MIN_SWING * np.mean([chain(joints, p) for p in paws(joints, fk(joints, None)[0])] or [0])
    path = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(xz, axis=0), axis=1))])
    d = _wrap(np.diff(R, axis=0)).mean(1)  # the window must keep moving too, by the same 20-frame block rule
    cd = np.concatenate([[0], np.cumsum(d)])
    m20 = (cd[BLOCK:] - cd[:-BLOCK]) / BLOCK  # m20[k]: mean of d[k:k+20]
    best = None
    for i in range(len(R) - MIN_LOOP):
        js = np.arange(i + MIN_LOOP, len(R))
        dr = _wrap(R[js] - R[i]).mean(1)
        if feet.shape[1]:
            dp = np.linalg.norm(feet[js] - feet[i], axis=2).mean(1)
            up = feet[i:, :, 1]
            swing = (np.maximum.accumulate(up) - np.minimum.accumulate(up))[MIN_LOOP:].min(1)  # every paw must lift
        else:
            dp = swing = np.zeros(len(js))
        # a loop runs one way: no U-turns or out-and-back (Zoo in-betweening edits often turn round mid-clip)
        turn = (np.maximum.accumulate(yaw[i:]) - np.minimum.accumulate(yaw[i:]))[MIN_LOOP:]
        detour = path[js] - path[i] - np.linalg.norm(xz[js] - xz[i], axis=1)
        s = dr + dp - 0.3 * swing + TURN_W * turn + DETOUR_W * detour
        moving = (cd[js] - cd[i]) / (js - i)  # windows up to 20 frames: their mean
        if len(m20) > i:
            lo = np.minimum.accumulate(m20[i:])
            long = js - i >= BLOCK
            moving[long] = lo[np.minimum(js[long] - BLOCK - i, len(lo) - 1)]
        s[(moving < MIN_BLOCK) | ((swing < min_swing) if feet.shape[1] else False)] = np.inf
        k = int(s.argmin())
        if np.isfinite(s[k]) and (best is None or s[k] < best[0]):
            best = (float(s[k]), i, int(js[k]), float(dr[k]))
    return best if best and best[3] <= MAX_LOOP_DEG else None


def cut_loop(joints, frames, i, j):
    """Frames i..j with the end-to-start residual of every channel spread linearly, so the last frame equals frame 0;
    the root's travel goes with it (the clip plays in place). -> (frames, travel per frame along x/z)."""
    seg = frames[i:j + 1].copy()
    rcols = [c for jt in joints for c, k in jt["channels"] if k.endswith("rotation")]
    seg[:, rcols] = np.unwrap(seg[:, rcols], period=360, axis=0)
    t = np.linspace(0, 1, len(seg))[:, None]
    res = seg[-1] - seg[0]
    xz = [c for c, k in joints[0]["channels"] if k in ("Xposition", "Zposition")]
    travel = res[xz] / (len(seg) - 1)
    return seg - t * res, travel


def motion_of(member):
    stem = Path(member).stem
    m = EDIT.search(stem)
    return stem[:m.start()] if m else re.sub(r"_\d+$", "", stem)


def curate(pack, out):
    out = Path(out); (out / "clips").mkdir(parents=True, exist_ok=True)
    best, fps, rest = {}, None, None
    with zipfile.ZipFile(pack) as z:
        for m in sorted(n for n in z.namelist() if re.search(r"(^|/)bvh/[^/]+\.bvh$", n)):  # .npy files are broken
            name = motion_of(m)
            cyclic = any(c in name.lower() for c in CYCLIC)
            try:
                joints, frames, ft, head = parse(z.read(m).decode(errors="replace"))
                s = score(joints, frames, cyclic)
            except (ValueError, IndexError, KeyError):
                continue
            if s and (name not in best or s[0] < best[name][0]):
                best[name] = (*s[:3], m, cyclic)
        clips = []
        for name, (s, i, j, m, cyclic) in sorted(best.items()):
            joints, frames, ft, head = parse(z.read(m).decode(errors="replace"))
            fps = fps or round(1 / ft)
            if rest is None:
                rest = {jt["name"]: [round(float(v), 3) for v in p] for jt, p in zip(joints, fk(joints, None)[0])}
            if cyclic:
                seg, travel = cut_loop(joints, frames, i, j)
                speed = float(np.linalg.norm(travel)) / ft
            else:
                seg = frames
                xz = [c for c, k in joints[0]["channels"] if k in ("Xposition", "Zposition")]
                speed = float(np.linalg.norm(seg[-1, xz] - seg[0, xz])) / (ft * max(len(seg) - 1, 1))
            write(out / "clips" / f"{name}.bvh", head, seg, ft)
            clips.append({"name": name, "file": f"clips/{name}.bvh", "frames": len(seg), "fps": round(1 / ft),
                          "loop": cyclic, "travel_cm_s": round(speed, 2), "source": Path(m).name, "score": round(s, 3)})
            print(f"CLIP {name}: {len(seg)} frames{' loop' if cyclic else ''}, score {s:.3f}, from {Path(m).name}")
    if not clips:
        raise SystemExit("FAIL: no clip in the pack keeps moving")
    clips.sort(key=lambda c: not c["loop"])  # a gait first: the skeleton sheet and the role map use clips[0]
    animal = {"id": Path(pack).stem, "fps": fps, "rest": rest, "clips": clips}
    (out / "animal.json").write_text(json.dumps(animal, indent=1))
    return animal


# ---------------------------------------------------------------- role map (Claude)
def claude(content, effort="high"):
    body = {"model": MODEL, "max_tokens": 16000, "output_config": {"effort": effort}, "fallbacks": "default",
            "messages": [{"role": "user", "content": content}]}
    req = urllib.request.Request("https://api.anthropic.com/v1/messages", json.dumps(body).encode(), {
        "x-api-key": os.environ["ANTHROPIC_API_KEY"], "anthropic-version": "2023-06-01",
        "anthropic-beta": "server-side-fallback-2026-07-01", "content-type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=900) as r:
            msg = json.loads(r.read())
    except urllib.error.HTTPError as e:
        raise SystemExit(f"FAIL: Claude API {e.code}: {e.read().decode(errors='replace')[:300]}")
    if msg.get("stop_reason") == "refusal":
        raise SystemExit("FAIL: Claude declined the role-map request")
    return "".join(b.get("text", "") for b in msg["content"] if b["type"] == "text").strip()


def skeleton_text(joints, rest):
    return "\n".join(f"{jt['name']} (parent {joints[jt['parent']]['name'] if jt['parent'] is not None else '-'})"
                     f" at {[round(float(v), 2) for v in p]}" for jt, p in zip(joints, rest))


def role_map(out):
    out = Path(out)
    animal = json.loads((out / "animal.json").read_text())
    joints, frames, _, _ = parse((out / animal["clips"][0]["file"]).read_text())
    pose = fk(joints, frames[:1])[0]  # the sheet shows this first frame too
    names = {jt["name"] for jt in joints}
    sheet = {"type": "image", "source": {"type": "base64", "media_type": "image/png",
                                         "data": base64.b64encode((out / "skeleton.png").read_bytes()).decode()}}
    brief = (f"This is the motion-capture skeleton of a {animal['id']} (a quadruped). Joints with their parent and "
           f"position in the first frame of a {animal['clips'][0]['name']} clip (cm, Y up; '_end' joints are the chain tips):\n{skeleton_text(joints, pose)}\n\n"
           "The image shows that frame from the side (top panel) and from above (bottom panel) with every joint "
           "labelled; blue = left, red = right.\n\nMap the joints onto these shared quadruped bones: "
           f"{', '.join(BONES)}. FL/FR = front left/right leg, HL/HR = hind left/right leg, left = the animal's own "
           "left; limb segment 01 is the upper leg (shoulder/hip), 04 the paw or toe tip. root is a static origin "
           "bone: map no joint to it unless a joint is a ground-level trajectory helper. Several joints may map to one "
           "bone (for example a clavicle and an upper arm both to FL_01). Every bone of pelvis, spine_01, head and "
           "all four segments of every leg must get at least one joint. List joints that fit no bone as unmapped.")
    ask = brief + '\n\nReply with JSON only: {"role_map": {"<joint>": "<bone>", ...}, "unmapped": ["<joint>", ...]}'
    text = claude([sheet, {"type": "text", "text": ask}])
    try:
        got = json.loads(text[text.index("{"):text.rindex("}") + 1])
        rm, unmapped = got["role_map"], got.get("unmapped", [])
    except (ValueError, KeyError) as e:
        raise SystemExit(f"FAIL: the role map reply was not JSON ({e}): {text[:200]}")
    bad = [f"{k}->{v}" for k, v in rm.items() if k not in names or v not in BONES]
    missing = [b for b in REQUIRED if b not in rm.values()]
    if bad or missing:
        raise SystemExit(f"FAIL: role map {'unknown ' + ', '.join(bad) + '; ' if bad else ''}"
                         f"{'missing required roles ' + ', '.join(missing) if missing else ''}".strip("; "))
    review = claude([sheet, {"type": "text", "text": (
        f"{brief}\n\nAnother model proposed this map:\n{json.dumps(rm, indent=1)}\n"
        f"unmapped: {unmapped}\n\nCheck it against the joint list and the image: left/right, front/hind, segment order "
        "from upper leg (01) to paw (04), spine/neck/head/tail order, and every required bone covered. Answer with one "
        "line only: PASS, or FAIL: <the reason>.")}])
    verdict = review.strip().splitlines()[-1].strip() if review.strip() else "FAIL: empty review"
    if not verdict.upper().startswith("PASS"):
        raise SystemExit(verdict if verdict.upper().startswith("FAIL") else f"FAIL: {verdict}")
    (out / "role_map.json").write_text(json.dumps({"role_map": rm, "unmapped": unmapped, "review": verdict}, indent=1))
    print("ROLE MAP", verdict, json.dumps(rm))


if __name__ == "__main__":
    a = sys.argv[1:]
    role_map(a[1]) if a[0] == "--role-map" else curate(a[0], a[1])
