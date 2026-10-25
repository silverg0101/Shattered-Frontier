"""Grounded bird topology adapter; no species rules or geometry repair."""
import numpy as np
import limbs
import creature
from body_plans import GROUNDED_BIRD as FAMILY


def plan(bones, height, floor=None):
    import rig_standard as geometry
    names = [b["name"] for b in bones]
    parent = {b["name"]: b["parent"] for b in bones}
    kids = {n: [m for m in names if parent[m] == n] for n in names}
    head0 = {b["name"]: np.asarray(b["head"], float) for b in bones}
    tail0 = {b["name"]: np.asarray(b["tail"], float) for b in bones}

    def attempt(turn):
        angle = np.radians(turn)
        rotation = np.array([[np.cos(angle), -np.sin(angle), 0], [np.sin(angle), np.cos(angle), 0], [0, 0, 1]])
        head = {n: rotation @ head0[n] for n in names}
        tail = {n: rotation @ tail0[n] for n in names}
        mid = np.median([p[0] for p in head.values()])
        snap = lambda p: [0 if abs(p[0] - mid) < .03 * height else p[0] - mid, p[1], p[2]]
        groups, _ = limbs.group({"points": [snap(p) for n in names for p in (head[n], tail[n])],
                                 "bones": [{"name": n, "parent": parent[n], "head": 2*i, "tail": 2*i+1} for i,n in enumerate(names)],
                                 "calib": {"model_height": height}})
        legs = sorted({g for g in groups.values() if g.endswith("leg")})
        wings = sorted({g for g in groups.values() if g.endswith("arm")})
        if len(legs) != 2 or len(wings) != 2:
            raise ValueError(f"grounded bird needs two ground-reaching legs and two raised wing chains; found {len(legs)} legs/{len(wings)} wings")
        top = lambda g: next(n for n in names if groups[n] == g and groups.get(parent[n]) != g)
        def chain(root, group):
            out = [root]
            while True:
                cs = [n for n in kids[out[-1]] if groups[n] == group]
                if not cs:
                    return out
                # Follow the longest continuation; toes/feathers merge to their ancestor.
                out.append(max(cs, key=lambda n: np.linalg.norm(tail[n] - head[n]) + sum(np.linalg.norm(tail[m]-head[m]) for m in descendants(n))))
        def descendants(n):
            out = list(kids[n])
            for m in out:
                out.extend(kids[m])
            return out
        head_roots = [n for n in names if groups[n] == "head" and groups.get(parent[n]) != "head"]
        if len(head_roots) != 1:
            raise ValueError("grounded bird needs one unambiguous head chain")
        head_chain = chain(head_roots[0], "head")
        body = [n for n in names if groups[n] == "body"]
        if not body or np.mean([head[n][1] for n in head_chain]) >= np.mean([head[n][1] for n in body]):
            raise ValueError("bird head must face forward (-Y)")
        axial = sorted(body, key=lambda n: -head[n][1]) + head_chain
        source = {"axial": creature.chain_resample(axial, 6), "tail": [[] for _ in range(6)]}
        for limb_groups, prefix in ((legs, "H"), (wings, "W")):
            sides = set()
            for group in limb_groups:
                ns = chain(top(group), group)
                side = "L" if np.mean([head[n][0] - mid for n in ns]) > 0 else "R"
                if side in sides:
                    raise ValueError("bird paired limbs are not one left and one right")
                sides.add(side)
                source[prefix+side] = creature.chain_resample(ns, 4, [float(np.linalg.norm(tail[n]-head[n])) for n in ns])
        tails = [n for n in names if groups[n] == "tail" and groups.get(parent[n]) != "tail"]
        if tails:
            source["tail"] = creature.chain_resample(chain(tails[0], "tail"), 6)
        chains = {"axial": list(FAMILY.bones[1:7]), "tail": [f"tail_{i:02}" for i in range(1,7)],
                  **{g: [f"{g}_{i:02}" for i in range(1,5)] for g in FAMILY.limbs}}
        labels, out = {}, {"root": {"head": [0,0,0], "tail": [0,0,.1*height], "parent": None, "sources": []}}
        for group, gs in source.items():
            if group == "tail" and not tails:
                base = head[axial[0]]
                pts = [base + np.array([0,.1*height*i/6,0]) for i in range(7)]
            else:
                pts = geometry.layout(gs, head, tail, height)
            for i, sources in enumerate(gs):
                name = chains[group][i]
                real = [n for n in sources if "+" not in n]
                labels.update({n: name for n in real})
                out[name] = {"head": pts[i].tolist(), "tail": pts[i+1].tolist(), "parent": FAMILY.parents[name], "sources": real}
        for n in names:
            ancestor = n
            while ancestor and ancestor not in labels:
                ancestor = parent[ancestor]
            labels.setdefault(n, labels.get(ancestor, "pelvis"))
        kept = {s[0] for gs in source.values() for s in gs if s and "+" not in s[0]}
        return {"turn": turn, "labels": labels, "bones": {n:out[n] for n in FAMILY.bones},
                "skeleton": FAMILY.skeleton, "dropped": [n for n in names if n not in kept], "warnings": []}
    errors = []
    for turn in (0, 180, 90, 270):
        try:
            return attempt(turn)
        except ValueError as e:
            errors.append(str(e))
    raise ValueError("cannot label grounded bird: " + "; ".join(dict.fromkeys(errors)))
