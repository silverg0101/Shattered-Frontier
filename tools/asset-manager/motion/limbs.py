"""Colour-coded views: the model rendered with each limb painted its own flat colour, for the video model's first
frame. Left and right can't be confused in the video, and each limb is found by its colour.

    blender -b -P limbs.py -- MODEL OUT_DIR [--turn DEG]    # OUT_DIR/views.png, joints.json, limbs.json
    python limbs.py --check                                  # self-check of the grouping, no Blender

Bones are grouped from the skeleton alone (any creature, any bone names): a limb is a chain branching off the centre
line, a leg if it reaches the floor, an arm if not. The body is the centre-line part that joins the limbs; centre
chains hanging off it are the head (forward or up) and the tail (back or down). Small chains off the head or tail
(ears, whiskers) go with it. Each vertex takes the colour of the limb with the most weight on it. limbs.json holds
the colours and bones per limb and the checks: each limb one piece, left/right alike, no colour far from its bones.
"""
import json, sys
from pathlib import Path

import numpy as np

# sRGB. Pairs are (left, right); legs take them front to back, then arms. Fixed order, so a colour always means
# the same thing: red is the front left leg of every quadruped.
PAIRS = [((1.0, 0.05, 0.05), (0.05, 0.3, 1.0)),   # red, blue
         ((1.0, 0.85, 0.0), (0.0, 0.75, 0.1)),    # yellow, green
         ((1.0, 0.45, 0.0), (0.0, 0.85, 0.9)),    # orange, cyan
         ((1.0, 1.0, 1.0), (0.45, 0.2, 0.05))]    # white, brown
HEAD, TAIL, BODY = (1.0, 0.4, 0.75), (0.55, 0.1, 0.9), (0.2, 0.2, 0.2)  # pink, purple, charcoal
UNWEIGHTED = (0.0, 0.0, 0.0)
# bleed: % of a limb's vertices 5 % of height nearer another limb; stray: vertices a bone pulls far from itself
LIMITS = {"pieces": 1, "pair_ratio": 0.7, "bleed_pct": 2.0, "stray_vertices": 20}


def group(j):
    """bone name -> limb name, and limb name -> sRGB colour."""
    P, h = np.array(j["points"]), j["calib"]["model_height"]
    by = {b["name"]: b for b in j["bones"]}
    kids = {n: [] for n in by}
    for b in j["bones"]:
        if b["parent"]:
            kids[b["parent"]].append(b["name"])

    def below(n):
        out = [n]
        for m in out:
            out += kids[m]
        return out

    off = lambda n: abs(P[by[n]["head"], 0]) > 0.01 * h
    pts = lambda names: P[[i for n in names for i in (by[n]["head"], by[n]["tail"])]]
    roots = [b["name"] for b in j["bones"] if off(b["name"]) and not (b["parent"] and off(b["parent"]))]
    reach = {r: np.linalg.norm(pts(below(r)) - P[by[r]["head"]], axis=-1).max() for r in roots}
    legs = [r for r in roots if reach[r] > 0.1 * h and pts(below(r))[:, 2].min() - P[:, 2].min() < 0.08 * h]
    arms = [r for r in roots if r not in legs and reach[r] > 0.1 * h]
    # The body: every centre bone on the way from the root to a limb.
    body = set()
    for r in legs + arms:
        n = by[r]["parent"]
        while n and n not in body:
            body.add(n)
            n = by[n]["parent"]
    if not body:  # no limbs (a snake): the whole centre line is body
        body = {n for n in by if not off(n)}
    limb = {n: "body" for n in body}
    for n in by:  # a centre chain off the body: head if it heads forward (-y) or up, else tail
        b = by[n]
        if n in limb or off(n) or (b["parent"] and b["parent"] not in body):
            continue
        tip = pts(below(n)).mean(0) - P[b["head"]]
        name = "head" if -tip[1] + tip[2] > 0 else "tail"
        for m in below(n):
            limb[m] = name
    colours, used = {"body": BODY, "head": HEAD, "tail": TAIL}, 0
    for kind, rs in (("leg", legs), ("arm", arms)):
        rs = sorted(rs, key=lambda r: P[by[r]["head"], 1])  # front (-y) first
        lefts, rights = [r for r in rs if P[by[r]["head"], 0] > 0], [r for r in rs if P[by[r]["head"], 0] < 0]
        if len(lefts) != len(rights):  # unpaired limbs go with the bone they hang off, below
            continue
        for k, pair in enumerate(zip(lefts, rights)):
            rank = "" if len(lefts) == 1 else "front " if k == 0 else "hind " if k == len(lefts) - 1 else f"{k + 1} "
            for side, r, c in zip(("left", "right"), pair, PAIRS[used % len(PAIRS)]):
                name = f"{rank}{side} {kind}"
                colours[name] = c
                for m in below(r):
                    limb[m] = name
            used += 1
    for n in by:  # the rest (unpaired limbs, ears, whiskers, small chains) go with the bone they hang off
        chain = [n]
        while chain[-1] not in limb and by[chain[-1]]["parent"]:
            chain.append(by[chain[-1]]["parent"])
        for m in chain:
            limb.setdefault(m, limb.get(chain[-1], "body"))
    return limb, colours


def linear(c):
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


def seg_dist(v, a, b):
    """Distance from each point v (N,3) to each segment a-b (S,3): (N,S)."""
    ab = b - a
    t = np.clip(((v[:, None] - a) * ab).sum(-1) / np.maximum((ab * ab).sum(-1), 1e-12), 0, 1)
    return np.linalg.norm(v[:, None] - (a + t[..., None] * ab), axis=-1)


def strays(dist, h):
    """Vertices a bone moves from much farther away than its own skin: a toe pulling the hip. Measured against the
    bone's usual reach, so a spine bone that broadly covers the torso is fine."""
    dist = np.asarray(dist)
    usual = np.median(dist)
    return int((dist > max(3 * usual, usual + 0.05 * h)).sum())


def pieces(n, edges, members, co=None):
    """Connected pieces among the member vertices, biggest first (vertex counts). Vertices at the same spot count
    as joined: a glTF import splits the mesh along every UV seam."""
    if co is not None:
        _, first = np.unique(np.round(co, 6), axis=0, return_inverse=True)
        lead = {}
        edges = list(edges) + [(lead.setdefault(int(k), i), i) for i, k in enumerate(np.ravel(first))]
    parent = list(range(n))
    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    for a, b in edges:
        if members[a] and members[b]:
            parent[find(a)] = find(b)
    roots = [find(i) for i in range(n) if members[i]]
    return sorted(np.unique(roots, return_counts=True)[1].tolist(), reverse=True) if roots else []


def main():
    import bpy
    sys.path.insert(0, str(Path(__file__).parent))
    import render_views as rv
    argv = sys.argv[sys.argv.index("--") + 1:]
    model, out = argv[0], Path(argv[1])
    turn = float(argv[argv.index("--turn") + 1]) if "--turn" in argv else 0.0
    out.mkdir(parents=True, exist_ok=True)
    arm = rv.load(model, turn)
    lo, hi = rv.bounds(arm)
    h = hi.z - lo.z
    pts, bones = rv.points(arm)
    limb, colours = group({"points": [list(p) for p in pts], "bones": bones, "calib": {"model_height": h}})
    names = list(colours)
    mat = bpy.data.materials.new("limbs")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    attr = nodes.new("ShaderNodeAttribute")
    attr.attribute_name = "limb"
    bsdf = nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.8
    mat.node_tree.links.new(attr.outputs["Color"], bsdf.inputs["Base Color"])
    P = np.array([list(p) for p in pts])
    deform = [b for b in bones if b["deform"]]
    seg_a, seg_b = P[[b["head"] for b in deform]], P[[b["tail"] for b in deform]]
    seg_limb = np.array([names.index(limb[b["name"]]) for b in deform])
    seg_index = {b["name"]: i for i, b in enumerate(deform)}
    counts, bleed, split, reach, unweighted = {n: 0 for n in names}, {n: 0 for n in names}, {}, {}, 0
    for o in bpy.context.scene.objects:
        if o.type != "MESH" or not any(m.type == "ARMATURE" and m.object == arm for m in o.modifiers):
            continue
        group_limb = {g.index: names.index(limb[g.name]) for g in o.vertex_groups if g.name in limb}
        me = o.data
        score = np.zeros((len(me.vertices), len(names)))
        for v in me.vertices:
            for g in v.groups:
                if g.group in group_limb:
                    score[v.index, group_limb[g.group]] += g.weight
        best = score.argmax(1)
        none = score.max(1) <= 0
        unweighted += int(none.sum())
        co = np.array([o.matrix_world @ v.co for v in me.vertices])
        d = seg_dist(co, seg_a, seg_b)
        seg_of = {g.index: seg_index[g.name] for g in o.vertex_groups if g.name in seg_index}
        for v in me.vertices:
            for g in v.groups:
                if g.weight > 0.05 and g.group in seg_of:
                    reach.setdefault(deform[seg_of[g.group]]["name"], []).append(d[v.index, seg_of[g.group]])
        rgb = np.array([linear(colours[n]) for n in names])[best]
        rgb[none] = UNWEIGHTED
        col = me.color_attributes.new("limb", "FLOAT_COLOR", "POINT")
        col.data.foreach_set("color", np.hstack([rgb, np.ones((len(rgb), 1))]).ravel())
        me.materials.clear()
        me.materials.append(mat)
        edges = [tuple(e.vertices) for e in me.edges]
        for k, n in enumerate(names):
            mine = (best == k) & ~none
            counts[n] += int(mine.sum())
            # the body is what's left between the limbs: its skin sits near hip and shoulder bones by nature
            if n != "body" and mine.any() and (seg_limb == k).any() and (seg_limb != k).any():
                own, other = d[mine][:, seg_limb == k].min(1), d[mine][:, seg_limb != k].min(1)
                bleed[n] += int((own - other > 0.05 * h).sum())
            split[n] = split.get(n, []) + pieces(len(me.vertices), edges, mine, co)
    checks = {}
    for n in names:
        big = [c for c in split.get(n, []) if c >= 0.05 * max(counts[n], 1)]
        checks[n] = {"colour": colours[n], "bones": sorted(b for b, l in limb.items() if l == n),
                     "vertices": counts[n], "pieces": len(big),
                     "bleed_pct": round(100 * bleed[n] / max(counts[n], 1), 2)}
    problems = [f"{n}: {c['pieces']} pieces" for n, c in checks.items() if c["vertices"] and c["pieces"] > LIMITS["pieces"]]
    problems += [f"{n}: {c['bleed_pct']}% of its vertices sit nearer another limb's bones"
                 for n, c in checks.items() if c["bleed_pct"] > LIMITS["bleed_pct"]]
    for n in names:
        if "left" in n.split():
            r = n.replace("left", "right")
            if r in counts and max(counts[n], counts[r]):
                ratio = min(counts[n], counts[r]) / max(counts[n], counts[r])
                if ratio < LIMITS["pair_ratio"]:
                    problems.append(f"{n} / {r}: vertex counts differ ({counts[n]} vs {counts[r]})")
    problems += [f"{n}: no vertices" for n in names if not counts[n] and n != "body"]
    if unweighted:
        problems.append(f"{unweighted} vertices no bone moves (black)")
    stray = {b: c for b, c in sorted(((b, strays(r, h)) for b, r in reach.items()), key=lambda kv: -kv[1]) if c}
    problems += [f"{b} pulls {c} vertices far from itself" for b, c in stray.items() if c >= LIMITS["stray_vertices"]]
    cams, calib = rv.setup(lo, hi)
    image = rv.render(cams, out / "views.png")
    uv = rv.project(cams, pts)
    (out / "joints.json").write_text(json.dumps({
        "calib": calib, "turn": turn, "points": [list(p) for p in pts], "bones": bones,
        "uv": uv, "seen": rv.seen(image, uv)}))
    (out / "limbs.json").write_text(json.dumps({"limbs": checks, "unweighted": unweighted, "stray": stray,
                                                "problems": problems,
                                                "passed": not problems, "limits": LIMITS}, indent=1))
    for n, c in checks.items():
        print(f"limb {n}: {len(c['bones'])} bones, {c['vertices']} vertices, {c['pieces']} piece(s), bleed {c['bleed_pct']}%")
    print("limbs " + ("PASSED" if not problems else "FAILED: " + "; ".join(problems)))


def check():
    # A quadruped facing -y (spine 0-1-2, legs off both ends, head forward, tail back, ears off the head)
    # and a biped (legs reach the floor, arms don't, head above).
    def rig(points, bones):
        return {"calib": {"model_height": 1.0}, "points": points,
                "bones": [{"name": n, "head": a, "tail": b, "parent": p} for n, a, b, p in bones]}
    q = rig([[0, 0.3, 0.6], [0, 0, 0.6], [0, -0.3, 0.6],
             [0.1, -0.3, 0.5], [0.1, -0.3, 0], [-0.1, -0.3, 0.5], [-0.1, -0.3, 0],
             [0.1, 0.3, 0.5], [0.1, 0.3, 0], [-0.1, 0.3, 0.5], [-0.1, 0.3, 0],
             [0, -0.5, 0.7], [0, 0.8, 0.5], [0.05, -0.5, 0.8], [0.05, -0.5, 0.9]],
            [("pelvis", 0, 1, None), ("chest", 1, 2, "pelvis"),
             ("fl", 3, 4, "chest"), ("fr", 5, 6, "chest"), ("hl", 7, 8, "pelvis"), ("hr", 9, 10, "pelvis"),
             ("neck", 2, 11, "chest"), ("tail", 0, 12, "pelvis"), ("ear", 13, 14, "neck")])
    limb, colours = group(q)
    assert limb == {"pelvis": "body", "chest": "body", "fl": "front left leg", "fr": "front right leg",
                    "hl": "hind left leg", "hr": "hind right leg", "neck": "head", "tail": "tail", "ear": "head"}, limb
    assert colours["front left leg"] == PAIRS[0][0] and colours["hind right leg"] == PAIRS[1][1], colours
    b = rig([[0, 0, 1], [0, 0, 1.4], [0.1, 0, 1], [0.1, 0, 0], [-0.1, 0, 1], [-0.1, 0, 0],
             [0.2, 0, 1.4], [0.6, 0, 1.4], [-0.2, 0, 1.4], [-0.6, 0, 1.4], [0, 0, 1.7]],
            [("hips", 0, 1, None), ("thigh_l", 2, 3, "hips"), ("thigh_r", 4, 5, "hips"),
             ("arm_l", 6, 7, "hips"), ("arm_r", 8, 9, "hips"), ("neck", 1, 10, "hips")])
    limb, colours = group(b)
    assert limb["thigh_l"] == "left leg" and limb["arm_r"] == "right arm" and limb["neck"] == "head", limb
    assert colours["left arm"] == PAIRS[1][0], colours  # the arms take the pair after the legs
    v = np.array([[0, 0, 0], [2, 0, 1]], float)
    assert np.allclose(seg_dist(v, np.array([[0, 0, 0.0]]), np.array([[2, 0, 0.0]]))[:, 0], [0, 1])
    assert pieces(5, [(0, 1), (1, 2), (3, 4)], [True, True, True, True, False]) == [3, 1]
    assert strays([0.01] * 100, 1.0) == 0 and strays([0.01] * 100 + [0.3] * 25, 1.0) == 25
    assert strays(np.linspace(0, 0.2, 100), 1.0) == 0  # a broad spine bone
    seam = np.array([[0, 0, 0], [1, 0, 0], [1, 0, 0], [2, 0, 0]], float)  # 1 and 2: one spot, split at a UV seam
    assert pieces(4, [(0, 1), (2, 3)], [True] * 4, seam) == [4]
    print("check ok")


if __name__ == "__main__":
    check() if sys.argv[1:] == ["--check"] else main()
