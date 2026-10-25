"""python gait_synth.py walk|run|trot|gallop OUT.bvh [RIG_LABELS.json]
A procedural quadruped gait cycle as a looping BVH on the shared quadruped skeleton (zoo_retarget.py's roles, role map
identity), the last resort for a creature whose animal pack has no clean gait take and whose UniMate takes fail: a
walk (4-beat lateral sequence, 60% stance), a trot (diagonal pairs, 45%) or a gallop/run (hind pair then front pair, 35%,
a moment of suspension; a 4-beat transverse gallop read as hopping to the review). Y up, facing +Z, right side at -X, centimetres, 30 fps, in place: each leg's paw is placed by
a two-bone solve (the thigh, then the knee-to-paw chain kept in its rest shape) so a stance sweeps straight back along
the floor at one speed (the treadmill zoo_retarget pins) and a swing lifts in an arc. Frames: a whole cycle plus a
closing frame equal to the first. The skeleton is the creature's own rig at rest (rig_standard's labels json "bones",
Blender Z up facing -Y, turned to BVH axes), so zoo_retarget's rest-relative rotation transfer lands its paws exactly;
without one, a generic standing quadruped (hip 60 cm)."""
import json, math, sys

FPS = 30
GAITS = {  # phase of each leg's touch-down in the cycle, stance share, period s, hip swing half-angle deg, swing lift share
    "walk": {"phase": {"HL": 0.0, "FL": 0.25, "HR": 0.5, "FR": 0.75}, "stance": 0.6, "period": 1.0, "swing": 18.0, "lift": 0.18, "pitch": 0.0},
    "trot": {"phase": {"HL": 0.0, "FR": 0.0, "HR": 0.5, "FL": 0.5}, "stance": 0.45, "period": 0.6, "swing": 22.0, "lift": 0.22, "pitch": 0.0},
    "gallop": {"phase": {"HR": 0.0, "HL": 0.12, "FR": 0.5, "FL": 0.62}, "stance": 0.35, "period": 0.55, "swing": 26.0, "lift": 0.28, "pitch": 0.0},  # a pitching trunk lifted the front stance off the floor
}
GAITS["run"] = GAITS["gallop"]
LEGS = ("FL", "FR", "HL", "HR")
JOINTS = ["pelvis", "spine_01", "spine_02", "spine_03", "neck", "head", *[f"tail_{i:02}" for i in range(1, 7)],
          *[f"{l}_{i:02}" for l in LEGS for i in range(1, 5)]]


def skeleton(hip=60.0):
    """The generic standing quadruped, hip height `hip`: {joint: (parent, offset (x, y, z) cm)}; legs hang straight
    down from the hips (-X is the right side) and the shoulders under spine_03; <leg>_end is the paw's end."""
    s, l1, l2, l3, l4 = hip / 60, 0.47 * hip, 0.37 * hip, 0.19 * hip, 0.09 * hip  # legs 1.12 x the hip height: bent knees standing
    j = {"pelvis": (None, (0, hip, 0)), "spine_01": ("pelvis", (0, 0, 20 * s)), "spine_02": ("spine_01", (0, 0, 20 * s)),
         "spine_03": ("spine_02", (0, 0, 20 * s)), "neck": ("spine_03", (0, 4 * s, 14 * s)), "head": ("neck", (0, 2 * s, 14 * s)),
         "tail_01": ("pelvis", (0, 3 * s, -8 * s))}
    for i in range(2, 7):
        j[f"tail_{i:02}"] = (f"tail_{i - 1:02}", (0, 0, -7 * s))
    for leg in LEGS:
        x = (12 if leg[1] == "L" else -12) * s
        j[f"{leg}_01"] = ("spine_03" if leg[0] == "F" else "pelvis", (x, 0, 0))
        for i, l in zip((2, 3, 4), (l1, l2, l3)):
            j[f"{leg}_{i:02}"] = (f"{leg}_{i - 1:02}", (0, -l, 0))
        j[f"{leg}_end"] = (f"{leg}_04", (0, -l4, 0))
    return j


def skeleton_from(bones):
    """The creature's own rig at rest as a skeleton: rig_standard's labels json "bones" ({name: {head, tail, parent}},
    Blender metres, Z up, facing -Y) turned to BVH axes (Y up, facing +Z) and centimetres, the pelvis over the floor
    (the lowest paw end)."""
    to = lambda p: (p[0] * 100, p[2] * 100, -p[1] * 100)
    if to(bones["head"]["head"])[2] < to(bones["pelvis"]["head"])[2]:  # faces +Y instead: turn it round
        to = lambda p: (-p[0] * 100, p[2] * 100, p[1] * 100)
    floor = min(to(bones[f"{l}_04"]["tail"])[1] for l in LEGS)
    head = {n: to(bones[n]["head"]) for n in JOINTS}
    j = {}
    for n in JOINTS:
        p = bones[n]["parent"] if n != "pelvis" else None
        p = p if p in head else None
        base = head[p] if p else (0, floor, 0)
        j[n] = (p, tuple(a - b for a, b in zip(head[n], base)))
    for l in LEGS:
        j[f"{l}_end"] = (f"{l}_04", tuple(a - b for a, b in zip(to(bones[f"{l}_04"]["tail"]), head[f"{l}_04"])))
    return j


def positions(sk):
    """Rest world positions of every joint of a skeleton."""
    pos = {}
    for n, (p, off) in sk.items():
        base = pos[p] if p else (0.0, 0.0, 0.0)
        pos[n] = tuple(a + b for a, b in zip(base, off))
    return pos


def tilt(v):
    """A hanging vector's forward tilt (rad) from straight down."""
    return math.atan2(v[2], -v[1])


def frames(gait, sk):
    """[{joint: (xrot, yrot, zrot)} per frame] plus the pelvis position per frame, a closed loop, on skeleton `sk`."""
    g, pos = GAITS[gait], positions(sk)
    legs = {}
    for leg in LEGS:
        hip, knee, end = pos[f"{leg}_01"], pos[f"{leg}_02"], pos[f"{leg}_end"]
        thigh = tuple(b - a for a, b in zip(hip, knee)); lower = tuple(b - a for a, b in zip(knee, end))
        legs[leg] = {"l1": math.hypot(*thigh), "l2": math.hypot(*lower), "t_rest": tilt(thigh), "l_rest": tilt(lower),
                     "z": end[2] - hip[2], "h": hip[1] - end[1]}  # the paw's rest place under the hip
    n = round(g["period"] * FPS)
    span = sum(l["l1"] + l["l2"] for l in legs.values()) / 4
    # half the stance sweep: the swing angle, within what every leg reaches with its paw on the floor and the body
    # dropped 6% at most (the drop below), so all four paws sweep alike
    reach = min([span * math.sin(math.radians(g["swing"]))] +
                [math.sqrt(max(0.0, (0.985 * (L["l1"] + L["l2"])) ** 2 - (0.94 * L["h"]) ** 2)) for L in legs.values()])
    hip_h, trunk = pos["pelvis"][1], pos["spine_03"][2] - pos["pelvis"][2]
    out, ppos = [], []
    def sweep(leg, t):
        """(the paw's sweep forward of its rest place, its lift) at cycle time t."""
        u = (t - g["phase"][leg]) % 1.0
        if u < g["stance"]:
            return reach * (1 - 2 * u / g["stance"]), 0.0
        s = (u - g["stance"]) / (1 - g["stance"])
        return reach * (-1 + 2 * (s * s * (3 - 2 * s))), g["lift"] * span * math.sin(math.pi * s)
    for k in range(n + 1):
        t = (k % n) / n
        rot = {}
        bob = 0.02 * hip_h * math.sin(4 * math.pi * t)
        pitch = g["pitch"] * math.sin(2 * math.pi * t)
        # the body drops so every planted leg reaches the floor at its sweep (a leg standing straight at rest can only
        # sweep with the hips lower: the walk's inverted-pendulum dip, highest at mid-stance)
        drop = 0.0
        for leg in LEGS:
            L, (z, lift) = legs[leg], sweep(leg, t)
            if lift == 0.0:
                drop = max(drop, L["h"] - math.sqrt(max(0.0, (0.985 * (L["l1"] + L["l2"])) ** 2 - (L["z"] + z) ** 2)))
        for leg in LEGS:
            L, (z, lift) = legs[leg], sweep(leg, t)
            # the joint the leg hangs from rose by this (the shoulders ride the pelvis pitch too)
            rise = bob - drop + (trunk * math.sin(math.radians(pitch)) if leg[0] == "F" else 0.0)
            # the paw's wanted place from its hip: its rest place, swept and lifted, into the frame the leg hangs in
            # (the pelvis pitch for a hind leg, half of it at spine_03 for a front one)
            ph = -math.radians(pitch if leg[0] == "H" else pitch * 0.5)
            zt, yt = L["z"] + z, -(L["h"] + rise - lift)
            zt, yt = zt * math.cos(ph) + yt * math.sin(ph), -zt * math.sin(ph) + yt * math.cos(ph)
            l1, l2, fwd = L["l1"], L["l2"], leg[0] == "H"  # a hind knee points forward, an elbow back
            d = min(math.hypot(zt, yt), (l1 + l2) * 0.995)
            beta = math.atan2(zt, -yt)
            alpha = math.acos(max(-1.0, min(1.0, (l1 * l1 + d * d - l2 * l2) / (2 * l1 * d))))
            gamma = math.acos(max(-1.0, min(1.0, (l1 * l1 + l2 * l2 - d * d) / (2 * l1 * l2))))
            theta = beta + (alpha if fwd else -alpha)  # the thigh's tilt
            psi = theta - (math.pi - gamma if fwd else -(math.pi - gamma))  # the knee-to-paw chain's tilt
            a = theta - L["t_rest"]; b = psi - L["l_rest"] - a  # joint turns from rest; +X rotation tilts backward
            rot[f"{leg}_01"], rot[f"{leg}_02"] = (-math.degrees(a), 0.0, 0.0), (-math.degrees(b), 0.0, 0.0)
        rot["pelvis"] = (pitch, 2.0 * math.sin(2 * math.pi * t), 0.0)
        rot["spine_02"] = (-pitch * 0.5, 0.0, 0.0)
        rot["neck"] = (-pitch * 0.5 + 2.0 * math.sin(4 * math.pi * t), 0.0, 0.0)
        ppos.append((pos["pelvis"][0], pos["pelvis"][1] + bob - drop, pos["pelvis"][2]))
        out.append(rot)
    return out, ppos


def bvh(gait, bones=None) -> str:
    sk = skeleton_from(bones) if bones else skeleton()
    order = []
    def walk(name, depth):
        parent, off = sk[name]
        pad = "\t" * depth
        if name.endswith("_end"):
            return f"{pad}End Site\n{pad}{{\n{pad}\tOFFSET {off[0]:.4f} {off[1]:.4f} {off[2]:.4f}\n{pad}}}\n"
        order.append(name)
        s = f"{pad}{'ROOT' if parent is None else 'JOINT'} {name}\n{pad}{{\n{pad}\tOFFSET {off[0]:.4f} {off[1]:.4f} {off[2]:.4f}\n"
        s += f"{pad}\tCHANNELS {'6 Xposition Yposition Zposition' if parent is None else '3'} Zrotation Xrotation Yrotation\n"
        for c in [c for c, (p, _) in sk.items() if p == name]:
            s += walk(c, depth + 1)
        return s + f"{pad}}}\n"
    head = "HIERARCHY\n" + walk("pelvis", 0)
    rows, pos = frames(gait, sk)
    lines = []
    for rot, p in zip(rows, pos):
        vals = [p[0], p[1], p[2]]
        for name in order:
            x, y, z = rot.get(name, (0.0, 0.0, 0.0))
            vals += [z, x, y]
        lines.append(" ".join(f"{v:.4f}" for v in vals))
    return head + f"MOTION\nFrames: {len(lines)}\nFrame Time: {1 / FPS:.6f}\n" + "\n".join(lines) + "\n"


def check():
    """Self-check: every gait loops (last frame = first) and really swings the legs, on the generic skeleton and on a
    rig whose legs hang at a tilt (the rest turns cancel: a stance starts and ends at the rest paw place)."""
    for gait in ("walk", "trot", "gallop"):
        rows, pos = frames(gait, skeleton())
        assert rows[0] == rows[-1] and pos[0] == pos[-1], gait
        thighs = [r["HL_01"][0] for r in rows]
        assert max(thighs) - min(thighs) > 20, (gait, thighs)
    bones = {n: {"head": [0, 0, 0], "tail": [0, 0, 0], "parent": None} for n in JOINTS}
    for n, (p, off) in skeleton(50).items():  # the generic one, stated in Blender axes, its hind thighs tilted forward
        if not n.endswith("_end"): bones[n]["parent"] = p
    pos = positions(skeleton(50))
    for n in JOINTS:
        bones[n]["head"] = [pos[n][0] / 100, -pos[n][2] / 100, pos[n][1] / 100]
    for l in LEGS:
        bones[f"{l}_04"]["tail"] = [bones[f"{l}_04"]["head"][0], bones[f"{l}_04"]["head"][1], bones[f"{l}_04"]["head"][2] - 0.045]
    sk = skeleton_from(bones)
    assert abs(positions(sk)["head"][2] - pos["head"][2]) < 1e-6 and abs(positions(sk)["HL_end"][1]) < 1e-6, positions(sk)["HL_end"]
    rows, _ = frames("walk", sk)
    assert abs(rows[0]["HL_01"][0] - frames("walk", skeleton(50))[0][0]["HL_01"][0]) < 1e-6
    text = bvh("walk")
    assert text.count("JOINT") == 27 and text.count("End Site") == 4 and "Frames: 31" in text
    return "gait_synth ok"


if __name__ == "__main__":
    a = sys.argv[1:]
    if not a or a[0] == "--check":
        print(check())
    else:
        bones = json.load(open(a[2], encoding="utf-8"))["bones"] if len(a) > 2 else None
        open(a[1], "w").write(bvh(a[0], bones))
        print("GAIT", a[0], a[1])
