"""Put a SkinTokens-rigged creature (bones bone_N) on the shared quadruped skeleton.

    blender -b -P rig_standard.py -- --label IN.glb LABELS.json SHEET.png   label the bones, draw the labelled skeleton
    python rig_standard.py --review LABELS.json SHEET.png                     Claude checks the sheet (ANTHROPIC_API_KEY):
                                                                              prints REVIEW PASS, or exits with FAIL: why
    blender -b -P rig_standard.py -- IN.glb LABELS.json OUT.glb               rename, add root, collapse/pad, clean the
                                                                              weights, export, then --verify the export
    blender -b -P rig_standard.py -- --verify OUT.glb                         exact bone list; posing one leg moves no
                                                                              vertex of another leg's paw

Labels come from limbs.group (legs, head, tail, body); front vs hind by distance along the spine from the head, left vs
right by the sign of X once the model faces -Y (the turn that makes it so is found here and applied to the output).
Chains collapse or pad with creature.chain_resample: collapsed bones' weights merge into the bone that keeps their
group, padded bones get no weights. The trunk (pelvis..neck) is laid by length from the hind legs to the front legs; a
rig with too few trunk bones has its trunk skin spread along it by position. Bones off a chain (toes, ears, jaw) merge into the chain bone they hang off.
The weight clean-up (from proof/wfix.py): a vertex weighted to two limbs keeps only the limb nearest it by bone
distance (limb roots may still blend), and a vertex wholly on one leg but far nearer another limb's bone moves there.
"""
import json, math, sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import creature, limbs  # creature.py is uploaded next to this script
from zoo_curate import BONES

LEGS = ("FL", "FR", "HL", "HR")
AXIAL = ["pelvis", "spine_01", "spine_02", "spine_03", "neck", "head"]
TAIL = [f"tail_{i:02d}" for i in range(1, 7)]
CHAINS = {"axial": AXIAL, "tail": TAIL, **{g: [f"{g}_{i:02d}" for i in range(1, 5)] for g in LEGS}}
PARENT = {"root": None, "pelvis": "root", "tail_01": "pelvis", "HL_01": "pelvis", "HR_01": "pelvis",
          "FL_01": "spine_03", "FR_01": "spine_03",
          **{c[i]: c[i - 1] for c in CHAINS.values() for i in range(1, len(c))}}
LIMB_ROOTS = {"FL_01", "FR_01", "HL_01", "HR_01", "tail_01", "neck"}  # may blend with the body and each other


def limb_of(std):
    """Which limb a standard bone belongs to for the weight clean-up; None for the body."""
    pre = std.split("_")[0]
    return pre if pre in LEGS else "tail" if pre == "tail" else "head" if std in ("neck", "head") else None


# ---------------------------------------------------------------- labelling (no Blender)
def plan(bones, h, floor=None):
    """bones [{name, parent, head, tail, weight?}] in world space (Blender, Z up), h the model height -> labels.json
    content: turn, labels {bone: standard}, dropped, warnings, bones {standard: {head, tail, parent, sources}}.
    weight: the skin weight a bone carries, summed over the vertices; with it, the turn whose head chain carries the
    most skin wins (a head outweighs a tail, though a boar's lowered snout sits below its raised tail). floor: the
    mesh's lowest z; no joint goes below it (SkinTokens can end the paw bones under the feet)."""
    found, errors = [], []
    for turn in (0, -90, 90, 180):
        try:
            found.append(plan_turned(bones, h, turn, floor))
        except ValueError as e:
            errors.append(f"turned {turn}: {e}")
    if not found:
        raise SystemExit("FAIL: no head, body and four legs found: " + "; ".join(errors))
    # should a tail pass for a head: the head is heavier, else (no weights, or the choices of head nearly tied) higher
    first = {}
    for r in found:
        first.setdefault(r["_heads"], r)
    ws = sorted((r["_head_w"] for r in first.values()), reverse=True)
    weighed = all("weight" in b for b in bones) and ws[0] > 0 and (len(ws) < 2 or ws[0] >= 1.5 * ws[1])
    best = max(found, key=lambda r: r["_head_w"] if weighed else r["_head_z"])
    del best["_head_z"], best["_head_w"], best["_heads"]
    return best


def plan_turned(bones, h, turn, floor=None):
    t = math.radians(turn)
    R = np.array([[math.cos(t), -math.sin(t), 0], [math.sin(t), math.cos(t), 0], [0, 0, 1]])
    names = [b["name"] for b in bones]
    head = {b["name"]: R @ np.asarray(b["head"], float) for b in bones}
    tail = {b["name"]: R @ np.asarray(b["tail"], float) for b in bones}
    parent = {b["name"]: b["parent"] for b in bones}
    kids = {n: [m for m in names if parent[m] == n] for n in names}
    # limbs.group wants the centre line at x = 0; generated rigs wobble about it by a centimetre or so
    x0 = float(np.median([head[n][0] for n in names]))
    snap = lambda p: [0.0 if abs(p[0] - x0) < 0.03 * h else p[0] - x0, p[1], p[2]]
    def group(keep):
        pts = [snap(p) for n in keep for p in (head[n], tail[n])]
        return limbs.group({"points": pts, "bones": [{"name": n, "parent": parent[n], "head": 2 * i, "tail": 2 * i + 1}
                                                     for i, n in enumerate(keep)], "calib": {"model_height": h}})[0]
    limb = group(names)
    arms = {l for l in limb.values() if l.endswith("arm")}
    if arms and len({l for l in limb.values() if l.endswith("leg")}) == 4:
        # a pair that never reaches the floor (long ears, whiskers) is no leg on a quadruped: group without it, then
        # each of its bones goes with the bone it hangs off
        limb = group([n for n in names if limb[n] not in arms])
        for n in names:
            m = n
            while m not in limb:
                m = parent[m]
            limb[n] = limb[m]
    def below(n):
        out = [n]
        for m in out:
            out += kids[m]
        return out
    # a centre chain off the body is the head when it sits ahead of the body, the tail when behind: limbs.group's own
    # rule (heads forward or up) reads a drooping snout as a tail
    body_y = np.mean([head[n][1] for n in names if limb[n] == "body"])
    for n in [n for n in names if limb[n] in ("head", "tail") and limb.get(parent[n]) == "body"]:
        kind = "head" if np.mean([head[m][1] for m in below(n)]) < body_y else "tail"
        for m in below(n):
            limb[m] = kind if limb[m] in ("head", "tail") else limb[m]
    top = lambda kind: [n for n in names if limb[n] == kind and limb.get(parent[n]) != kind]
    size = {}
    def subtree(n):  # bone length below n, n included
        if n not in size:
            size[n] = np.linalg.norm(tail[n] - head[n]) + sum(subtree(m) for m in kids[n])
        return size[n]
    legs = sorted({l for l in limb.values() if l.endswith(("leg", "arm"))})
    if len(legs) != 4:
        raise ValueError(f"{len(legs)} limbs ({', '.join(legs) or 'none'}), a quadruped has 4")
    if not top("head"):
        raise ValueError("no head chain")
    leg_root = {l: next(n for n in names if limb[n] == l and limb.get(parent[n]) != l) for l in legs}
    # a first bone starting far nearer the centre line than the other legs' (a collarbone stub inside the chest, its
    # twin's put on the centre line) is body: the leg starts at its child, or the pair is a segment out of step
    xs = lambda l: abs(head[leg_root[l]][0] - x0)
    mid = float(np.median([xs(l) for l in legs]))
    for l in legs:
        while xs(l) < 0.5 * mid and len(kids[leg_root[l]]) == 1:
            leg_root[l] = kids[leg_root[l]][0]
    head_root = max(top("head"), key=subtree)

    def up(n):  # n and its ancestors, root last
        out = [n]
        while parent[out[-1]]:
            out.append(parent[out[-1]])
        return out
    def path(a, b):  # the bones from a to b through the tree, both included
        ua, ub = up(a), up(b)
        common = next(n for n in ua if n in ub)
        return ua[:ua.index(common) + 1] + ub[:ub.index(common)][::-1]
    # the spine runs from the leg attachment farthest from the head to the bone the head hangs off; the skeleton root
    # may sit anywhere on it (SkinTokens often roots at the shoulders, the hips then on a branch towards the tail).
    # A leg on a short centre stub off the spine attaches where the stub leaves it.
    if not parent[head_root]:
        raise ValueError("the head is the skeleton root")
    spine = max((path(parent[leg_root[l]], parent[head_root]) for l in legs), key=len)
    at = {l: next(n for n in path(parent[leg_root[l]], parent[head_root]) if n in spine) for l in legs}
    stub = {l: path(parent[leg_root[l]], at[l])[:-1] for l in legs}
    if any(sum(np.linalg.norm(tail[n] - head[n]) for n in s) > 0.25 * h for s in stub.values()):
        raise ValueError("a leg hangs off a side branch of the body, not the spine to the head")
    # front vs hind: distance along the spine from the head
    pos = {n: i for i, n in enumerate(spine)}
    order = sorted(legs, key=lambda l: -pos[at[l]])
    side = lambda l: "L" if np.mean([head[n][0] - x0 for n in names if limb[n] == l]) > 0 else "R"
    std_of = {}
    for pair, prefix in ((order[:2], "F"), (order[2:], "H")):
        if {side(l) for l in pair} != {"L", "R"}:
            raise ValueError(f"the {'front' if prefix == 'F' else 'hind'} legs are not one left and one right")
        for l in pair:
            std_of[l] = prefix + side(l)
    hind = max(pos[at[l]] for l in order[2:])
    span = lambda chain: [float(np.linalg.norm((head[chain[i + 1]] if i + 1 < len(chain) else tail[c]) - head[c]))
                          for i, c in enumerate(chain)]

    def main_path(n, dominant):
        """n and the bones it continues into. dominant: past a fork, into a child carrying 2x the length of any other
        (a leg past a dewclaw); else a fork ends the chain (the head, where ears and jaw branch off)."""
        path = [n]
        while kids[path[-1]]:
            ks = sorted(kids[path[-1]], key=subtree, reverse=True)
            if len(ks) > 1 and not (dominant and subtree(ks[0]) >= 2 * subtree(ks[1])):
                break
            path.append(ks[0])
        return path
    neck_head = main_path(head_root, False)
    body_src, spine_src, neck_src = spine[:hind + 1], spine[hind + 1:], neck_head[:-1]
    tails = top("tail")
    tail_src = main_path(max(tails, key=subtree), True) if tails else []
    # the trunk (pelvis, spine_01..03, neck) by length along the rig's own line from the hips to the head: pelvis where
    # the hind legs join, the spine evenly up to where the front legs join, the neck from there; each trunk bone of the
    # rig goes to the standard bone its middle falls in (a rig with too few trunk bones then leaves gaps, and the
    # build spreads the trunk's skin by position instead)
    joint = lambda x, y: head[y] if parent[y] == x else head[x]
    line = [head[spine[0]] if len(spine) < 2 or parent[spine[1]] == spine[0] else tail[spine[0]]]
    line += [joint(x, y) for x, y in zip(spine, spine[1:])] + [head[n] for n in neck_head]
    cut = len(spine) + 1  # the anchors lie on the spine part: up to where the head chain starts
    leg_y = lambda pair: float(np.mean([head[leg_root[l]][1] for l in pair]))
    s_hip, s_sh = at_y(line[:cut], leg_y(order[2:])), at_y(line[:cut], leg_y(order[:2]))
    s_sh = max(s_sh, s_hip)
    trunk_pts = [point_at(line, s_hip + k / 4 * (s_sh - s_hip)) for k in range(5)] + [head[neck_head[-1]]]
    trunk_src = body_src + spine_src + neck_src
    at_t = along(np.array(trunk_pts), np.array([(head[m] + tail[m]) / 2 for m in trunk_src])) if trunk_src else []
    # the bones behind the hind legs stay pelvis, the ones up to the front legs' stay spine
    fa = max(pos[at[l]] for l in order[:2])
    cap = lambda m: 0 if m in body_src else 3 if m in spine[:fa + 1] else 4
    groups = {"axial": [[m for m, t in zip(trunk_src, at_t) if min(int(t), cap(m)) == i] for i in range(5)] + [[neck_head[-1]]],
              "tail": creature.chain_resample(tail_src, 6, span(tail_src))}
    for l in legs:
        src = main_path(leg_root[l], True)
        groups[std_of[l]] = creature.chain_resample(src, 4, span(src))
    warnings = [f"turned {turn} degrees to face -Y"] if turn else []
    for chain, src in (("spine", spine_src), ("neck", neck_src), ("tail", tail_src)):
        want = 3 if chain == "spine" else 1 if chain == "neck" else 6
        if len(src) < want:
            warnings.append(f"{chain}: {len(src)} bone(s) padded to {want}" + (" (stub bones)" if not src else ""))
    for l in legs:
        if sum(1 for g in groups[std_of[l]] if "+" not in g[0]) < 4:
            warnings.append(f"{std_of[l]}: leg padded to 4 bones")

    labels = {m: CHAINS[c][i] for c, gs in groups.items() for i, g in enumerate(gs) for m in g if "+" not in m}
    kept = {g[0] for gs in groups.values() for g in gs if g and "+" not in g[0]}
    for n in names:  # toes, ears, jaw, side chains: into the chain bone they hang off
        if n not in labels:
            labels[n] = next(labels[m] for m in up(n) if m in labels) if any(m in labels for m in up(n)) else "pelvis"

    out = {"root": {"head": [0, 0, 0], "tail": [0, 0, round(0.1 * h, 6)], "parent": None, "sources": []}}
    for c, gs in groups.items():
        if c == "tail" and not tail_src:  # no tail: stubs straight back from the pelvis
            a = head[body_src[0]]
            pts = [a + np.array([0, 0.1 * h * i / 6, 0]) for i in range(7)]
        else:
            pts = nonzero(trunk_pts + [tail[neck_head[-1]]], h) if c == "axial" else layout(gs, head, tail, h)
        skin = next((b["skin"] for b in bones if b["name"] == neck_head[-1] and "skin" in b), None)
        if c == "axial" and skin:  # a head bone stopping short of the head it carries: along its skin instead
            lo, hi, cen = (R @ np.asarray(v, float) for v in skin)
            front, back = min(lo[1], hi[1]), max(lo[1], hi[1])  # facing -Y
            if pts[-1][1] > front + 0.5 * (back - front):
                pts[-2] = np.array([cen[0], min(pts[-2][1], back), cen[2]])
                pts[-1] = np.array([cen[0], front, cen[2]])
                warnings.append("head: the rig's head bone stopped short of the head; laid along its skin")
        if floor is not None:
            pts = [np.array([p[0], p[1], max(p[2], floor)]) for p in pts]
        for i, g in enumerate(gs):
            out[CHAINS[c][i]] = {"head": [round(float(v), 6) for v in pts[i]], "tail": [round(float(v), 6) for v in pts[i + 1]],
                                 "parent": PARENT[CHAINS[c][i]], "sources": [m for m in g if "+" not in m]}
    out = {b: out[b] for b in BONES}
    if not out["head"]["head"][1] < out["pelvis"]["head"][1] - 0.05 * h:
        raise ValueError("the head is not in front of the pelvis")
    return {"turn": turn, "labels": labels, "dropped": [n for n in names if n not in kept], "warnings": warnings, "bones": out,
            "_head_z": float(np.mean([head[n][2] for n in names if limb[n] == "head"])),
            "_head_w": sum(b.get("weight", 0) for b in bones if limb[b["name"]] == "head"),
            "_heads": frozenset(n for n in names if limb[n] == "head")}


def arc(pts):
    return np.concatenate([[0], np.cumsum([np.linalg.norm(b - a) for a, b in zip(pts, pts[1:])])])


def at_y(pts, y):
    """Arc length along the polyline pts to its first point at depth y (facing -Y), else to its nearest point."""
    pts, s = [np.asarray(p, float) for p in pts], arc(pts)
    for i, (a, b) in enumerate(zip(pts, pts[1:])):
        if a[1] != b[1] and (a[1] - y) * (b[1] - y) <= 0:
            return float(s[i] + (y - a[1]) / (b[1] - a[1]) * (s[i + 1] - s[i]))
    return float(s[min(range(len(pts)), key=lambda i: abs(pts[i][1] - y))])


def point_at(pts, d):
    pts, s = [np.asarray(p, float) for p in pts], arc(pts)
    i = min(int(np.searchsorted(s, d, side="right")) - 1, len(pts) - 2)
    i = max(i, 0)
    return pts[i] + (d - s[i]) / max(s[i + 1] - s[i], 1e-12) * (pts[i + 1] - pts[i])


def along(P, Q):
    """Where points Q (m x 3) sit along the joint chain P (k+1 x 3): i + u on the nearest segment i (bone i, u in 0..1)."""
    a, d = P[:-1], P[1:] - P[:-1]
    u = np.clip(np.einsum("mkc,kc->mk", Q[:, None] - a[None], d) / np.maximum((d * d).sum(1), 1e-12), 0, 1)
    dist = np.linalg.norm(Q[:, None] - (a[None] + u[..., None] * d[None]), axis=2)
    k = dist.argmin(1)
    return k + u[np.arange(len(Q)), k]


TRUNK = AXIAL[:5]  # pelvis .. neck


def spread_trunk(W, P, co):
    """A trunk with a standard bone no rig bone went to: each vertex's trunk weight (W columns TRUNK, in order) spread
    over the trunk bones by where it sits along the trunk joints P (6 points, pelvis head .. head head), blending
    linearly between neighbouring bone middles. In place."""
    tot = W.sum(1)
    t = np.clip(along(P, co) - 0.5, 0, 4)
    i = np.minimum(t.astype(int), 3)
    f = t - i
    W[:] = 0
    W[np.arange(len(W)), i] = tot * (1 - f)
    W[np.arange(len(W)), i + 1] += tot * f


def layout(groups, head, tail, h):
    """Joint points along a chain: len(groups) + 1 points, bone i from point i to i + 1. A group starts at its first
    bone's head; an inserted piece x+j sits j/k of the way from x's head to the next group's start; empty groups
    share the gap between their neighbours; the chain ends at its last real bone's tail."""
    n, starts = len(groups), [None] * len(groups)
    for i, g in enumerate(groups):
        if g and "+" not in g[0]:
            starts[i] = head[g[0]]
    real = [m for g in groups for m in g if "+" not in m]
    starts.append(tail[real[-1]] if not groups[-1] or "+" not in groups[-1][0] else None)
    for i, g in enumerate(groups):
        if g and "+" not in g[0]:
            k = 1 + sum(1 for gg in groups if gg and gg[0].startswith(g[0] + "+"))
            nxt = next((s for s in starts[i + k:] if s is not None), tail[g[-1]])
            for j in range(1, k):
                starts[i + j] = head[g[0]] + j / k * (nxt - head[g[0]])
            if i + k == n and starts[n] is None:
                starts[n] = tail[g[-1]]
    for i in range(n + 1):  # empty groups: evenly between the known points around them
        if starts[i] is None:
            a = max(k for k in range(i) if starts[k] is not None)
            b = min(k for k in range(i, n + 1) if starts[k] is not None)
            starts[i] = starts[a] + (i - a) / (b - a) * (starts[b] - starts[a])
    return nonzero(starts, h)


def nonzero(starts, h):
    """No zero-length bones: a joint on top of the one before moves 0.02 h on along the chain."""
    starts = [np.asarray(p, float) for p in starts]
    d = starts[-1] - starts[0]
    d = d / np.linalg.norm(d) if np.linalg.norm(d) > 1e-9 else np.array([0, 0, 1.0])
    for i in range(1, len(starts)):
        if np.linalg.norm(starts[i] - starts[i - 1]) < 0.005 * h:
            starts[i] = starts[i - 1] + 0.02 * h * d
    return starts


def review(labels_path, sheet_path):
    import base64
    from zoo_curate import claude
    p = json.loads(Path(labels_path).read_text())
    rows = "\n".join(f"{b}: from {', '.join(v['sources']) or ('nothing (skin spread along the trunk)' if b in TRUNK else 'nothing (inserted)')} at "
                     f"{[round(x, 3) for x in v['head']]} -> {[round(x, 3) for x in v['tail']]}" for b, v in p["bones"].items())
    sheet = {"type": "image", "source": {"type": "base64", "media_type": "image/png",
                                         "data": base64.b64encode(Path(sheet_path).read_bytes()).decode()}}
    text = claude([sheet, {"type": "text", "text": (
        "A generated quadruped creature's skeleton was relabelled onto shared bone names. The image shows the model "
        "(grey, see-through) with the relabelled bones from its left side (top panel) and from above (bottom panel, "
        "head to the left); blue = the animal's left, red = its right, grey = centre. The creature faces -Y, Z up, "
        "so its left is +X. Bones (head -> tail, metres) and the original bones each one took:\n" + rows +
        "\n\nCheck the labels against the image: FL/FR = front left/right leg, HL/HR = hind left/right leg, left = "
        "the animal's own left; leg segments 01 (shoulder/hip) down to 04 (paw); pelvis at the hips, spine_01..03 "
        "towards the chest, neck and head in the head, tail_01..06 from the base to the tip; every bone inside the "
        "body part it names. Answer with one line only: PASS, or FAIL: <the reason>.")}], effort="medium")
    verdict = text.strip().splitlines()[-1].strip() if text.strip() else "FAIL: empty review"
    if not verdict.upper().startswith("PASS"):
        raise SystemExit(verdict if verdict.upper().startswith("FAIL") else f"FAIL: {verdict}")
    print("REVIEW", verdict)


# ---------------------------------------------------------------- Blender
def load(model):
    import render_views as rv
    arm = rv.load(model)
    lo, hi = rv.bounds(arm)
    mw = arm.matrix_world
    w, pts = {}, {}
    for o in limbs.skinned(arm):
        gname = {g.index: g.name for g in o.vertex_groups}
        for v in o.data.vertices:
            for g in v.groups:
                w[gname[g.group]] = w.get(gname[g.group], 0) + g.weight
            if v.groups:  # the vertices each bone carries most of, for skin_box
                pts.setdefault(gname[max(v.groups, key=lambda g: g.weight).group], []).append(o.matrix_world @ v.co)
    box = lambda p: [np.percentile(p, 2, 0).tolist(), np.percentile(p, 98, 0).tolist(), np.mean(p, 0).tolist()]
    bones = [{"name": b.name, "parent": b.parent.name if b.parent else None, "weight": w.get(b.name, 0.0),
              "head": list(mw @ b.head_local), "tail": list(mw @ b.tail_local)}
             | ({"skin": box(np.array(pts[b.name]))} if len(pts.get(b.name, ())) >= 20 else {}) for b in arm.data.bones]
    return arm, bones, hi.z - lo.z


def turn_scene(turn):
    import bpy
    from mathutils import Matrix
    R = Matrix.Rotation(math.radians(turn), 4, "Z")
    for o in bpy.context.scene.objects:
        if o.parent is None:  # matrix, not rotation_euler: glTF imports use quaternion rotation
            o.matrix_world = R @ o.matrix_world
    bpy.context.view_layer.update()


def sheet(p, path):
    """The relabelled bones over the see-through model, side view over top view, every bone labelled."""
    import bmesh, bpy
    from mathutils import Vector
    import render_views as rv
    sc, W, H = bpy.context.scene, 1600, 800
    arm = next(o for o in sc.objects if o.type == "ARMATURE")
    lo, hi = rv.bounds(arm)
    arm.hide_render = True
    for o in limbs.skinned(arm):
        o.color = (0.75, 0.75, 0.75, 1)
    col = {"L": (0.1, 0.3, 0.95, 1), "R": (0.9, 0.1, 0.1, 1), "C": (0.25, 0.25, 0.25, 1)}
    side_of = lambda b: "L" if b[:2] in ("FL", "HL") else "R" if b[:2] in ("FR", "HR") else "C"
    c, size = (lo + hi) / 2, hi - lo
    L, r = size.y, size.y * 0.004
    for s in "LRC":
        cu = bpy.data.curves.new(f"bones{s}", "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = r
        for b, v in p["bones"].items():
            if side_of(b) == s and b != "root":
                sp = cu.splines.new("POLY"); sp.points.add(1)
                sp.points[0].co, sp.points[1].co = (*v["head"], 1), (*v["tail"], 1)
        o = bpy.data.objects.new(f"bones{s}", cu); o.color = col[s]; sc.collection.objects.link(o)
    ball = bpy.data.meshes.new("ball")
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=r * 2.2); bm.to_mesh(ball); bm.free()
    for b, v in p["bones"].items():
        if b != "root":
            o = bpy.data.objects.new(f"j_{b}", ball); o.location = v["head"]; o.color = col[side_of(b)]; sc.collection.objects.link(o)
    sc.render.engine = "BLENDER_WORKBENCH"
    sh = sc.display.shading
    sh.light, sh.color_type = "STUDIO", "OBJECT"
    sc.render.resolution_x, sc.render.resolution_y = W, H
    sc.view_settings.view_transform = "Standard"
    sc.world = bpy.data.worlds.new("w"); sc.world.color = (1, 1, 1)
    fs, far = L * 0.028, L * 10
    skin = limbs.skinned(arm)

    def labels(rot, offset):
        made = []
        for b, v in p["bones"].items():
            if b == "root":
                continue
            cu = bpy.data.curves.new(f"t_{b}", "FONT"); cu.body = b; cu.size = fs; cu.align_x = "CENTER"
            o = bpy.data.objects.new(f"t_{b}", cu); o.rotation_euler = rot; o.color = col[side_of(b)]
            o.location = (Vector(v["head"]) + Vector(v["tail"])) / 2 + offset(b)
            sc.collection.objects.link(o); made.append(o)
        return made

    def shot(loc, rot, scale):
        cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); cam.data.type = "ORTHO"
        cam.data.ortho_scale, cam.data.clip_end = scale, 1e5
        cam.location, cam.rotation_euler = loc, rot; sc.collection.objects.link(cam); sc.camera = cam
        sc.render.filepath = str(Path(path).with_suffix(".tmp.png"))
        layers = []
        for model in (True, False):  # the model alone, then the bones and labels alone on a clear film
            for o in sc.objects:
                if o.type == "MESH" and not o.name.startswith("j_"):
                    o.hide_render = not (model and o in skin)
                elif o.type in ("CURVE", "FONT", "MESH"):
                    o.hide_render = model
            sc.render.film_transparent = not model
            bpy.ops.render.render(write_still=True)
            img = bpy.data.images.load(sc.render.filepath); w, hh = img.size
            px = np.empty(w * hh * 4, np.float32); img.pixels.foreach_get(px); bpy.data.images.remove(img)
            layers.append(px.reshape(hh, w, 4))
        Path(sc.render.filepath).unlink()
        bpy.data.objects.remove(cam)
        model, over = layers
        base = 0.55 + 0.45 * model  # the model faded, the bones drawn over it
        a = over[..., 3:4]
        return np.concatenate([base[..., :3] * (1 - a) + over[..., :3] * a, np.ones_like(a)], -1)
    # side: camera on +X (the animal's left) looking -X, head (-Y) to the left
    rot = (math.radians(90), 0, math.radians(90))
    made = labels(rot, lambda b: Vector((0, 0, fs * (1.2 if side_of(b) != "R" else -2.0))))
    side = shot((c.x + far, c.y, c.z), rot, max(L, size.z * W / H) * 1.2)
    for o in made:
        bpy.data.objects.remove(o)
    # top: looking down, head to the left, the animal's left (+X) at the bottom
    rot = (0, 0, math.radians(90))
    row = lambda b: int(b[-2:]) if b[:2] in LEGS or b.startswith("tail") else 1  # legs and tail: a row per bone
    labels(rot, lambda b: Vector(({"L": 1, "R": -1, "C": -1}[side_of(b)] * fs * (1.2 + row(b)), 0, 0)))
    top = shot((c.x, c.y, c.z + far), rot, max(L, size.x * W / H) * 1.5)  # room for the label rows
    out = np.concatenate([top, side], axis=0)  # pixel rows go bottom-up: side view on top
    img = bpy.data.images.new("sheet", out.shape[1], out.shape[0]); img.pixels.foreach_set(out.ravel())
    img.filepath_raw, img.file_format = str(path), "PNG"; img.save()


def label_main(model, labels_path, sheet_path):
    import render_views as rv
    arm, bones, h = load(model)
    p = plan(bones, h, rv.bounds(arm)[0].z)
    Path(labels_path).write_text(json.dumps(p, indent=1))
    turn_scene(p["turn"])
    sheet(p, sheet_path)
    for b, v in p["bones"].items():
        print(f"LABEL {b}: {', '.join(v['sources']) or '(inserted)'}")
    print("LABEL warnings:", "; ".join(p["warnings"]) or "none")


def clean(W, segs, limb, root, co):
    """The cross-limb clean-up on a weight matrix W (vertices x bones), in place. segs (a, b): bone segments; limb:
    limb index per bone (-1 body); root: limb roots, which may blend. Returns (vertices cleaned, vertices rebound)."""
    d = limbs.seg_dist(co, *segs)
    nl = limb.max() + 1
    member = np.stack([limb == k for k in range(nl)], 1)  # bones x limbs
    tot = W @ member
    dl = np.where(member[None], d[:, :, None], np.inf).min(1)  # vertices x limbs: nearest bone of each limb
    multi = (tot > 0).sum(1) >= 2
    keep = np.where(tot > 0, dl, np.inf).argmin(1)
    drop = multi[:, None] & (limb[None] >= 0) & (limb[None] != keep[:, None]) & ~root[None] & (W > 0)
    W[drop] = 0
    cleaned = int(drop.any(1).sum())
    s = W.sum(1, keepdims=True)
    np.divide(W, s, out=W, where=s > 0)
    tot = W @ member
    owner = np.where(tot.max(1) > 0, tot.argmax(1), -1)
    near = d.argmin(1)
    ln = limb[near]
    mine = dl[np.arange(len(W)), np.maximum(owner, 0)]
    legs = np.isin(owner, [k for k in range(nl) if k < 4])  # limb indices 0-3 are the legs
    move = legs & (ln >= 0) & (ln != owner) & ~root[near] & (mine >= 2.5 * d[np.arange(len(W)), near])
    W[move] = 0
    W[np.flatnonzero(move), near[move]] = 1
    return cleaned, int(move.sum())


def floaters(co, edges, h, gap=0.05):
    """Vertex indices of mesh islands more than gap*h from the largest one: image-to-3D debris such as a stray paw on
    the floor. Skinned to a body bone, it stays put while the legs move, and reads as a dislocated paw."""
    co = np.asarray(co, float)
    parent = np.arange(len(co))
    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    _, same = np.unique(np.round(co, 5), axis=0, return_inverse=True)  # weld: glTF splits islands along UV seams
    first = {}
    for a, b in [*map(tuple, edges), *((first.setdefault(s, i), i) for i, s in enumerate(same.ravel()))]:
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb
    root = np.array([find(i) for i in range(len(co))])
    ids, n = np.unique(root, return_counts=True)
    main = co[root == ids[n.argmax()]]
    out = []
    for r in ids[ids != ids[n.argmax()]]:
        idx = np.flatnonzero(root == r)
        d = min(np.linalg.norm(main[None] - co[idx[k:k + 64], None], axis=-1).min() for k in range(0, len(idx), 64))
        if d > gap * h:
            out += idx.tolist()
    return out


def floor_sheet(co, faces, h, band=0.03, margin=0.05, floor=None, weight=None, area=0.01):
    """Face indices of a thin horizontal sheet at paw level spanning wider than the body: a drawn ground shadow that
    image-to-3D made into a slab joined to the feet. Faces facing up or down wholly within the bottom `band` of the
    height above `floor` (the whole model's lowest z; default this mesh's), in a piece (faces sharing a vertex)
    reaching more than margin*h past the footprint of the mesh above it, that is a real slab (faces facing both up
    and down, together at least area*h^2) or carries next to no skin (weight: each vertex's summed skin weight;
    mean under 0.05). A tail lying on the floor or flat splayed toes face only up, and are skinned."""
    co = np.asarray(co, float)
    z0 = co[:, 2].min() if floor is None else floor
    above = co[co[:, 2] > z0 + band * h]
    if not len(above):
        return []
    lo, hi = above[:, :2].min(0) - margin * h, above[:, :2].max(0) + margin * h
    cand, nz = [], {}
    for i, f in enumerate(faces):
        p = co[list(f)]
        n = sum(np.cross(p[k] - p[0], p[k + 1] - p[0]) for k in range(1, len(p) - 1))  # 2 x area, polygons too
        if p[:, 2].max() <= z0 + band * h and abs(n[2]) > 0.9 * np.linalg.norm(n) > 0:
            cand.append(i)
            nz[i] = n[2] / 2
    _, weld = np.unique(np.round(co, 5), axis=0, return_inverse=True)  # glTF splits the mesh along UV seams
    weld, parent = weld.ravel(), {}
    def find(x):
        while parent.setdefault(x, x) != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x
    for i in cand:
        for v in faces[i]:
            parent[find(("f", i))] = find(("v", int(weld[v])))
    pieces = {}
    for i in cand:
        pieces.setdefault(find(("f", i)), []).append(i)
    out = []
    for g in pieces.values():
        vs = [v for i in g for v in faces[i]]
        pts = co[vs][:, :2]
        if not ((pts.min(0) < lo).any() or (pts.max(0) > hi).any()):
            continue
        slab = min(nz[i] for i in g) < 0 < max(nz[i] for i in g) and sum(abs(nz[i]) for i in g) >= area * h * h
        bare = weight is not None and float(np.mean(np.asarray(weight, float)[vs])) < 0.05
        if slab or bare:
            out += g
    return sorted(out)


def build_main(model, labels_path, out):
    import bpy
    import render_views as rv
    from mathutils import Matrix
    p = json.loads(Path(labels_path).read_text())
    arm, _, h = load(model)
    meshes = limbs.skinned(arm)
    for o in [o for o in bpy.context.scene.objects if o.type == "MESH" and o not in meshes]:
        print("BUILD removed", o.name, "(not skinned to the rig)")
        bpy.data.objects.remove(o)
    R = Matrix.Rotation(math.radians(p["turn"]), 4, "Z")
    floor = rv.bounds(arm)[0].z  # the whole model's floor: a mesh of its own (an eye, a tusk) has a higher lowest point
    for o in meshes:  # bake the turn and every object transform into the mesh: the export stands at the origin
        o.data.transform(R @ o.matrix_world, shape_keys=True)
        # the ground sheet first: its upright shreds are then loose islands for floaters
        sheet_faces = floor_sheet([v.co for v in o.data.vertices], [tuple(f.vertices) for f in o.data.polygons], h,
                                  floor=floor, weight=[sum(g.weight for g in v.groups) for v in o.data.vertices])
        if sheet_faces:
            import bmesh
            bm = bmesh.new()
            bm.from_mesh(o.data)
            bm.faces.ensure_lookup_table()
            bmesh.ops.delete(bm, geom=[bm.faces[i] for i in sheet_faces], context="FACES")
            bm.to_mesh(o.data)
            bm.free()
            print(f"BUILD {o.name}: removed {len(sheet_faces)} faces of a ground sheet at paw level")
        e = np.zeros(len(o.data.edges) * 2, int)
        o.data.edges.foreach_get("vertices", e)
        drop = floaters([v.co for v in o.data.vertices], e.reshape(-1, 2), h)
        if drop:
            import bmesh
            bm = bmesh.new()
            bm.from_mesh(o.data)
            bm.verts.ensure_lookup_table()
            bmesh.ops.delete(bm, geom=[bm.verts[i] for i in drop], context="VERTS")
            bm.to_mesh(o.data)
            bm.free()
            print(f"BUILD {o.name}: removed {len(drop)} vertices of loose islands floating off the body")
    arm.animation_data_clear()
    arm.matrix_world = Matrix.Identity(4)
    for o in meshes:
        o.parent, o.matrix_parent_inverse, o.matrix_basis = arm, Matrix.Identity(4), Matrix.Identity(4)
        for m in o.modifiers:
            if m.type == "ARMATURE":
                m.object = arm
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm.data.edit_bones
    for b in list(eb):
        eb.remove(b)
    for b in BONES:
        e = eb.new(b)
        e.head, e.tail = p["bones"][b]["head"], p["bones"][b]["tail"]
    for b in BONES:
        if PARENT[b]:
            eb[b].parent = eb[PARENT[b]]
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    a = np.array([p["bones"][b]["head"] for b in BONES])
    bvec = np.array([p["bones"][b]["tail"] for b in BONES])
    names = [*LEGS, "tail", "head"]  # clean() takes indices 0-3 as the legs
    limb = np.array([names.index(limb_of(b)) if limb_of(b) else -1 for b in BONES])
    root = np.array([b in LIMB_ROOTS for b in BONES])
    for o in meshes:  # merge weights onto the standard bones, then clean cross-limb weights
        gstd = {g.index: BONES.index(p["labels"][g.name]) for g in o.vertex_groups if g.name in p["labels"]}
        W = np.zeros((len(o.data.vertices), len(BONES)), np.float32)
        for v in o.data.vertices:
            for g in v.groups:
                if g.group in gstd:
                    W[v.index, gstd[g.group]] += g.weight
        co = np.array([v.co for v in o.data.vertices])
        if any(not p["bones"][b]["sources"] for b in TRUNK):  # too few trunk bones in the rig: spread by position
            ti = [BONES.index(b) for b in TRUNK]
            sub = W[:, ti]
            spread_trunk(sub, np.array([p["bones"][b]["head"] for b in AXIAL]), co)
            W[:, ti] = sub
            print(f"BUILD {o.name}: trunk skin spread along pelvis..neck")
        n1, n2 = clean(W, (a, bvec), limb, root, co)
        print(f"BUILD {o.name}: {n1} vertices lost weight on a second limb, {n2} rebound to a nearer limb")
        o.vertex_groups.clear()
        for k, b in enumerate(BONES):
            g = o.vertex_groups.new(name=b)
            for i in np.flatnonzero(W[:, k] > 0):
                g.add([int(i)], float(W[i, k]), "REPLACE")
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_yup=True, export_animations=False,
                              export_skins=True, use_active_scene=True)
    print("BUILD wrote", out)
    verify(out)


def verify(path):
    """Exact bone list and hierarchy; each leg posed in turn moves no vertex owned by another leg's paw."""
    import bpy
    from mathutils import Euler
    arm, bones, h = load(path)
    got = {b["name"]: b["parent"] for b in bones}
    if sorted(got) != sorted(BONES):  # SystemExit, not assert: Blender exits 0 on an uncaught exception
        raise SystemExit(f"FAIL: bones {sorted(set(got) ^ set(BONES))} differ from the standard")
    bad = [b for b in BONES if got[b] != PARENT[b]]
    if bad:
        raise SystemExit(f"FAIL: wrong parents for {bad}")
    meshes = limbs.skinned(arm)

    def posed():
        bpy.context.view_layer.update()
        dg, out = bpy.context.evaluated_depsgraph_get(), []
        for o in meshes:
            e = o.evaluated_get(dg); m = e.to_mesh()
            co = np.empty(len(m.vertices) * 3); m.vertices.foreach_get("co", co)
            M = np.array(o.matrix_world)
            out.append(co.reshape(-1, 3) @ M[:3, :3].T + M[:3, 3]); e.to_mesh_clear()
        return np.concatenate(out)
    rest = posed()
    W = []
    for o in meshes:
        idx = {g.index: g.name for g in o.vertex_groups}
        w = np.zeros((len(o.data.vertices), len(BONES)))
        for v in o.data.vertices:
            for g in v.groups:
                if idx[g.group] in BONES:
                    w[v.index, BONES.index(idx[g.group])] = g.weight
        W.append(w)
    W = np.concatenate(W)
    paw = {g: W[:, BONES.index(f"{g}_04")] > 0.5 for g in LEGS}
    leg = {g: W[:, [BONES.index(f"{g}_{i:02d}") for i in range(2, 5)]].sum(1) > 0.5 for g in LEGS}
    report = {"bones": BONES, "vertices": len(W)}
    for g in LEGS:
        pb = arm.pose.bones[f"{g}_02"]
        pb.rotation_mode = "XYZ"; pb.rotation_euler = Euler((math.radians(60), 0, 0))
        moved = np.linalg.norm(posed() - rest, axis=1)
        pb.rotation_euler = Euler((0, 0, 0))
        report[f"{g}_02 posed"] = {"own paw moved mm": round(float(moved[paw[g]].max(initial=0)) * 1000, 2),
                                   **{f"{o}_04 paw max mm": round(float(moved[paw[o]].max(initial=0)) * 1000, 3)
                                      for o in LEGS if o != g},
                                   "other legs' vertices moved >1 mm": int(sum((moved[leg[o]] > 1e-3).sum() for o in LEGS if o != g))}
        if report[f"{g}_02 posed"]["own paw moved mm"] <= 1:
            raise SystemExit(f"FAIL: posing {g}_02 does not move its paw")
        bleed = {o: float(moved[paw[o]].max(initial=0)) for o in LEGS if o != g}
        if max(bleed.values()) >= 1e-4:
            raise SystemExit(f"FAIL: posing {g}_02 moves another paw: {bleed}")
    print("VERIFY", json.dumps(report))
    print("VERIFY PASS")


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    if argv[0] == "--review":
        review(*argv[1:3])
    elif argv[0] == "--label":
        label_main(*argv[1:4])
    elif argv[0] == "--verify":
        verify(argv[1])
    else:
        build_main(*argv[:3])
