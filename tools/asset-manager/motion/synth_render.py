"""Blender: training images for one model's joint detector, filmed exactly like a job's first frame.

    blender -b -P synth_render.py -- MODEL OUT_DIR [N] [--turn DEG] [--seed S] [--limbs]

Picks the keypoints worth detecting (keypoints.json), then renders N random poses of the model from the same two
cameras, grey and light as render_views.py, at half size: OUT_DIR/img/NNNN.png plus labels.json with each
keypoint's uv in its own panel (0..1) and whether it is visible there. Half the poses bend every bone at random,
half are gait-like swings with random phases (the case a single side view gets wrong: legs apart in depth).
"""
import json, math, random, sys
from pathlib import Path

import bpy
from mathutils import Euler, Matrix, Quaternion, Vector

sys.path.insert(0, str(Path(__file__).parent))
import render_views as rv

MAX_KEYPOINTS = 48
HIDDEN = 0.06  # a joint is hidden in a view when something sits this far (x model height) in front of it


def pick_keypoints(pts, bones, seen, height):
    """Points worth detecting: on the model, on a bone that deforms it (not an IK helper), not inside a small
    subtree (fingers, face) and not lying along a longer bone (twist bones). The rest then follow their parent."""
    kids = {}
    for b in bones:
        kids.setdefault(b["parent"], []).append(b)
    def subtree(b):
        out = [b["head"], b["tail"]]
        for c in kids.get(b["name"], []):
            out += subtree(c)
        return out
    drop = set()
    for b in bones:
        sub = [i for i in subtree(b) if seen[i]]  # glTF stores no tails: an importer's invented end tail can be metres off
        if sub and max(max(pts[i][a] for i in sub) - min(pts[i][a] for i in sub) for a in range(3)) < 0.12 * height:
            # A hand with its fingers is ~10 % of a body: keep only its head. A first joint further in (the ball of
            # a foot) stays, or the bone above it can't be aimed.
            near = [c["head"] for c in kids.get(b["name"], []) if c["head"] == b["tail"] and seen[c["head"]]]
            drop |= set(sub) - {b["head"]} - set(near[:1] if len(kids.get(b["name"], [])) == 1 else [])
    deform = {i for b in bones if b.get("deform", True) for i in (b["head"], b["tail"])}
    long = [(pts[b["head"]], pts[b["tail"]]) for b in bones  # invented tails off the model don't count
            if seen[b["head"]] and seen[b["tail"]] and (pts[b["tail"]] - pts[b["head"]]).length >= 0.03 * height]
    def inside_a_bone(p):
        for a, c in long:
            d = c - a
            t = (p - a).dot(d) / d.length_squared
            if 0.05 < t < 0.95 and (a + d * t - p).length < 0.03 * height:
                return True
        return False
    # Heads only: they are real joints. glTF stores no tails, so an imported rig's tails are an importer's guess
    # that swings off the body once posed; a tail that matters is the next bone's head anyway.
    heads = {b["head"] for b in bones}
    keep = [i for i in range(len(pts))
            if i in heads and seen[i] and i in deform and i not in drop and not inside_a_bone(pts[i])]
    # A kept joint whose bone has nothing kept below it can't be aimed (a foot rigid to the shin). Add the next
    # joint straight on along the bone, if on the model: the ball of a foot, the middle-finger root of a hand.
    # Branches (a thumb) stay dropped, and so do end tails: on a glTF rig they are guesses that land far off once
    # posed, so a head keeps no nod point.
    for b in [b for b in bones if b["head"] in keep]:
        axis = pts[b["tail"]] - pts[b["head"]]
        cands = [c["head"] for c in kids.get(b["name"], [])]
        if not axis.length or not cands or any(c in keep for c in cands):
            continue
        def straight(i):
            d = pts[i] - pts[b["head"]]
            return d.normalized().dot(axis.normalized()) if d.length >= 0.03 * height else -1
        best = max((i for i in cands if seen[i]), key=straight, default=None)
        # One child (a foot's ball) is the way on, whatever an imported bone's guessed axis says; among several
        # (a hand's fingers) only the one straight on counts.
        if best is not None and straight(best) > -1 and (len(cands) == 1 or straight(best) >= math.cos(math.radians(45))):
            keep.append(best)
    keep = sorted(set(keep))
    if len(keep) > MAX_KEYPOINTS:  # keep the ends of the longest bones
        score = {i: 0.0 for i in keep}
        for b in bones:
            n = (pts[b["tail"]] - pts[b["head"]]).length
            for i in (b["head"], b["tail"]):
                if i in score:
                    score[i] = max(score[i], n)
        keep = sorted(sorted(keep, key=lambda i: -score[i])[:MAX_KEYPOINTS])
    return keep


def category(pts, bones, height, lo, hi):
    """A rig's body plan, which detectors of the same plan share what they learned from (app.py warm-starts from
    the best of them): its number of legs, each a chain of deforming bones that comes down to the ground, and
    whether the body stands upright (taller than it is long, facing the front camera) or is long (a quadruped)."""
    by_name = {b["name"]: b for b in bones}
    low = lambda b: pts[b["head"]][2] - lo[2] < 0.12 * height
    legs = set()
    for b in bones:
        if b.get("deform", True) and low(b):
            while b["parent"] in by_name and low(by_name[b["parent"]]):  # up the foot to the bone above the ground
                b = by_name[b["parent"]]
            legs.add(b["parent"])  # the shin: where the leg comes down
    legs.discard(None)
    return f"{len(legs)}-legged {'upright' if hi[2] - lo[2] >= hi[1] - lo[1] else 'long'}"


def bone_lengths(arm, height):
    """Bone length as head to farthest child head: glTF stores no tails, so an imported end bone's tail is a
    guess (capped at 10 % of the model's height)."""
    w = arm.matrix_world
    return {pb.name: max(((w @ c.head) - (w @ pb.head)).length for c in pb.children) if pb.children
            else min(((w @ pb.tail) - (w @ pb.head)).length, 0.1 * height) for pb in arm.pose.bones}


def joint_ranges(arm, lengths, height):
    """How far each bone may bend in a random pose, read from the skeleton's shape (no rig stores limits):
    the trunk (spine, neck) a little per bone, the head more, a limb's root and next joint (hip and knee,
    shoulder) a lot, short branch bones (clavicles) little, the rest of a limb (elbow, wrist, ankle) moderately.
    A limb is a chain that leaves the trunk. The trunk starts down the root's longest deforming chain (the hips),
    then follows whichever child carries on most straight (spine, neck, head; for a quadruped spine and neck)."""
    weighted = rv.weighted_bones(arm) or {pb.name for pb in arm.pose.bones}
    def reach(pb):  # length of the longest deforming chain below a bone (IK helpers count for nothing)
        return lengths[pb.name] * (pb.name in weighted) + max((reach(c) for c in pb.children), default=0)
    w = arm.matrix_world
    trunk, pb = set(), next(b for b in arm.pose.bones if b.parent is None)
    trunk.add(pb.name)
    pb = max(pb.children, key=reach, default=None)  # root -> hips
    while pb:
        trunk.add(pb.name)
        kids = [c for c in pb.children if c.name in weighted and lengths[c.name] > 0]
        if not kids:
            break
        if len(kids) == 1:  # no branching: the trunk goes on (neck -> head, whatever the head's guessed tail says)
            pb = kids[0]
            continue
        here = (w @ pb.tail) - (w @ pb.head) if len(kids) == 1 else (w @ kids[0].head) - (w @ pb.head)
        if pb.parent and pb.parent.name in trunk and pb.parent.parent:  # carry on the way the trunk was going
            here = (w @ pb.head) - (w @ pb.parent.head)
        dirs = {c.name: ((w @ max(c.children, key=reach).head) if c.children else (w @ c.tail)) - (w @ c.head)
                for c in kids}
        best = max(kids, key=lambda c: dirs[c.name].normalized().dot(here.normalized()) if here.length else 0)
        if here.length and dirs[best.name].normalized().dot(here.normalized()) < math.cos(math.radians(45)):
            break
        pb = best
    ranges = {}
    for pb in arm.pose.bones:
        if pb.name in trunk:
            ranges[pb.name] = 45 if not pb.children else 20
            continue
        chain, b = [pb], pb  # the limb from where it leaves the trunk down to this bone
        while b.parent and b.parent.name not in trunk:
            b = b.parent
            chain.insert(0, b)
        # The limb's root joint is its first bone of real length (thigh; upperarm after a short clavicle): it and
        # the next joint (knee, elbow) swing wide, short bones before it (clavicle) little, the rest moderately.
        root = next((i for i, c in enumerate(chain) if lengths[c.name] >= 0.08 * height), len(chain))
        k = len(chain) - 1
        ranges[pb.name] = 25 if k < root else 80 if k <= root + 1 else 50
        if lengths[pb.name] < 0.03 * height:
            ranges[pb.name] = 10
    return ranges


def random_pose(arm, rng, base, ranges, height):
    gait = rng.random() < 0.5
    t = rng.uniform(0, 2 * math.pi)
    for pb in arm.pose.bones:
        free = pb.parent is None or not pb.bone.use_connect and pb.parent.parent is None  # root, pelvis
        lim = math.radians(10 if free else ranges[pb.name])
        if gait:  # swings up to each joint's range, with some twist about the bone
            amp, ph = rng.uniform(0.3, 1.0), rng.uniform(0, 2 * math.pi)
            r = Euler((amp * lim * math.sin(t + ph), amp * lim / 3 * math.sin(t + 0.7 * ph),
                       amp * lim * math.cos(t + 1.3 * ph))).to_quaternion()
        else:  # sqrt: as many poses near a joint's limit as near rest (the limits are where detectors fail)
            axis = Vector([rng.gauss(0, 1) for _ in range(3)]).normalized()
            r = Quaternion(axis, lim * math.sqrt(rng.random()))
        if free and pb.parent is None:
            r = Euler([math.radians(rng.uniform(-10, 10)) for _ in range(3)]).to_quaternion()
        m = base[pb.name] @ r.to_matrix().to_4x4()
        if free and pb.parent is None:  # bone-local offset; scaled from metres by the armature's own scale
            s = 1 / arm.matrix_world.to_scale()[0]
            m = Matrix.Translation(Vector((rng.uniform(-.03, .03), rng.uniform(-.04, .04), rng.uniform(-.03, .03))) * height * s) @ m
        pb.matrix_basis = m


def visible(scene, cam, p, height):
    """False when the first surface the camera ray meets lies well in front of the joint (another body part)."""
    d = cam.matrix_world.to_quaternion() @ Vector((0, 0, -1))
    origin = p - d * 50
    hit, loc, *_ = scene.ray_cast(bpy.context.evaluated_depsgraph_get(), origin, d)
    return not hit or (p - loc).length < HIDDEN * height or (loc - origin).length > (p - origin).length


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    model, out = argv[0], Path(argv[1])
    n = int(argv[2]) if len(argv) > 2 and not argv[2].startswith("--") else 1500
    turn = float(argv[argv.index("--turn") + 1]) if "--turn" in argv else 0.0
    rng = random.Random(int(argv[argv.index("--seed") + 1]) if "--seed" in argv else 1)
    (out / "img").mkdir(parents=True, exist_ok=True)
    arm = rv.load(model, turn)
    if "--limbs" in argv:  # filmed limb-coloured (limbs.py): the detector learns the look of a limb-coloured take
        import limbs
        limbs.paint(arm, rv)
    lo, hi = rv.bounds(arm)
    for o in bpy.context.scene.objects:  # widgets hidden from the render must not block the visibility rays either
        if o.type == "MESH" and o.hide_render:
            o.hide_viewport = True
    pts, bones = rv.points(arm)
    cams, calib = rv.setup(lo, hi)
    height = calib["model_height"]
    image = rv.render(cams, out / "views.png")
    uv0 = rv.project(cams, pts)
    seen = rv.seen(image, uv0)
    keys = pick_keypoints(pts, bones, seen, height)
    (out / "keypoints.json").write_text(json.dumps({"points": keys, "names": [
        next(f"{b['name']}.{e}" for b in bones for e in ("head", "tail") if b[e] == i) for i in keys],
        "category": category(pts, bones, height, lo, hi)}))

    scene = bpy.context.scene
    scene.render.resolution_percentage = 50
    scene.cycles.samples = 8
    scene.render.use_persistent_data = True  # keeps the scene loaded between renders: 3x faster
    base = {pb.name: pb.matrix_basis.copy() for pb in arm.pose.bones}
    ranges = joint_ranges(arm, bone_lengths(arm, height), height)
    roots = [o for o in scene.objects if o.parent is None and o.type not in ("CAMERA", "LIGHT")]  # turn the model only
    yaw0 = {o.name: o.rotation_euler.z for o in roots}
    arm.animation_data_clear()
    labels = []
    for k in range(n):
        random_pose(arm, rng, base, ranges, height)
        yaw = math.radians(rng.uniform(-15, 15)) if rng.random() < 0.3 else 0.0
        for o in roots:
            o.rotation_euler.z = yaw0[o.name] + yaw
        bpy.context.view_layer.update()
        now = [None] * len(pts)
        for b in bones:
            pb = arm.pose.bones[b["name"]]
            now[b["head"]], now[b["tail"]] = arm.matrix_world @ pb.head, arm.matrix_world @ pb.tail
        uv = rv.project(cams, [now[i] for i in keys])
        labels.append({
            "front": [[2 * u, v] for u, v in uv["front"]], "side": [[2 * u - 1, v] for u, v in uv["side"]],
            "vis_front": [visible(scene, cams["front"], now[i], height) for i in keys],
            "vis_side": [visible(scene, cams["side"], now[i], height) for i in keys]})
        rv.render(cams, out / "img" / f"{k:05d}.png")
        if k % 100 == 0:
            print(f"synth {k}/{n}", flush=True)
    (out / "labels.json").write_text(json.dumps(labels))
    print(f"synth: {n} images, {len(keys)} keypoints")


if __name__ == "__main__":
    main()
