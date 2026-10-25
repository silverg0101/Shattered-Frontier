"""blender -b -P zoo_retarget.py -- RIG.glb ANIMAL.json CLIP OUT.glb OUT.json SIZE_CM HEIGHT_SCALE
Retarget a curated Zoo-300K clip (ANIMAL.json's role map, clips/<CLIP>.bvh next to it) onto a rig already on the shared
quadruped skeleton (standardise_rig: root, pelvis, spine_01..03, neck, head, tail_01..06, FL/FR/HL/HR_01..04, facing -Y).
Ported from proof/retarget.py (the 2026-10-24 hamster run):
- the model is scaled so its length (Y) is SIZE_CM, applied to the rig and mesh;
- rest-relative world rotation transfer per role; a source chain shorter or longer than the standard one is resampled
  with creature.chain_resample, a split source bone's rotation (from its parent's) spread over its pieces;
- the pelvis follows the source hips' offset scaled by the leg length ratio (the shortest legs' for its travel, the
  hind legs' for its height), vertical bob x HEIGHT_SCALE, in place;
  (a creature's UniMate clips are made on its own rig, so its leg rotations land the paws where the source's did);
- cleanup: tail follow-through, the body kept from going through the floor (a collapse lands on it), paws pinned on the floor while in contact (leg IK, 2-frame blend, four correction
  passes, baked to FK), loop seam closed (looping clips). A gait plays in place, so its planted paws sweep back under
  the body at the travel speed (treadmill): each stance is pinned to the line fitted through its contact points,
  a stand's to a point;
- metrics for creature.checks_pass: min_paw_cm, max_slide_cm (a planted paw's step off its treadmill line), seam_deg
  (the loop's end-to-start gap the retarget closed, or that is left after cleanup), flips (a joint turning > 90 deg in
  one frame); travel_cm_s (the treadmill speed the game should move the creature at) and max_step_deg (the largest
  one-frame turn) are reported, not gated.
OUT.glb holds the rig, mesh and one action named CLIP; OUT.json the metrics, contacts and per-paw heights (for
clip_sheet.py)."""
import bpy, sys, json, math
from pathlib import Path
from mathutils import Matrix, Quaternion, Vector
sys.path.insert(0, str(Path(__file__).parent))
import creature, render_views as rv  # creature.py is uploaded next to this script

TAIL_SHARE = 0.35   # share of the tail base's rotation each later segment repeats
TAIL_LAG = 1        # frames of delay per segment
TAIL_SWAY = 0.10    # radians of side sway per segment, once per clip
TAIL_CLEAR = 0.3    # the tail is lifted (at tail_02) until it clears the floor by this share of the rest hip height
CONTACT_SHARE = 0.30  # planted: in the lowest 30% of the paw's height range ...
CONTACT_M = 0.015     # ... and within 1.5 cm of its lowest point
MIN_STANCE = 3        # frames: a shorter touch is a skim, not a plant (no pin, no slide measured)
UNREACHED = 0.01      # m: a pin the leg still misses by this after two IK passes is released (the paw was pulled off it)
FLOOR_EPS = 0.002   # m: keep paws this far above the floor
BODY_SINK = 0.005   # m: the mesh may dip this far under the floor (hoof deformation) before the body is lifted
SINK_LIFT = 0.005   # share of the creature's length a paw may end under the floor for the whole clip to be lifted
TREADMILL_MIN = 0.002  # share of the creature's length a stance must sweep per frame to be a gait's treadmill, not a stand
FLIP_DEG = 90.0
CHAINS = [["pelvis"], ["spine_01", "spine_02", "spine_03", "neck"], ["head"], [f"tail_{i:02}" for i in range(1, 7)],
          *[[f"{l}_{i:02}" for i in range(1, 5)] for l in ("FL", "FR", "HL", "HR")]]
PAWS = ("FL", "FR", "HL", "HR")
TAIL = CHAINS[3]


def angle(a, b):
    return math.degrees(2 * math.acos(min(1.0, abs(a.dot(b)))))



def main(rigf, animalf, clip, out, outj, size_cm, height_scale):
    animal = json.loads(Path(animalf).read_text())
    meta = next(c for c in animal["clips"] if c["name"] == clip)
    loop = bool(meta["loop"])
    rig = rv.load(rigf, fps=animal.get("fps", 30))
    sc = bpy.context.scene
    for o in list(sc.objects):  # the importer's bone-shape icosphere is not part of the asset
        if o.type == "MESH" and o.name.startswith("Icosphere"): bpy.data.objects.remove(o)
    lo, hi = rv.bounds(rig)
    s = size_cm / 100 / (hi.y - lo.y)
    bpy.ops.object.select_all(action="DESELECT")
    for o in sc.objects: o.select_set(o == rig or o.parent == rig)
    bpy.context.view_layer.objects.active = rig
    rig.scale = [x * s for x in rig.scale]
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bpy.context.view_layer.update()
    floor = rv.bounds(rig)[0].z
    print(f"RT scale {s:.4f} -> {size_cm} cm long, floor {floor:.4f}")

    bpy.ops.import_anim.bvh(filepath=str(Path(animalf).parent / meta["file"]), global_scale=0.01)
    src = next(o for o in sc.objects if o.type == "ARMATURE" and o != rig)
    role = {j: r for j, r in animal["role_map"].items() if j in src.data.bones}
    depth = lambda b: len(b.parent_recursive)

    # paw contact point: where the paw bone (_04) crosses the rest floor, else its tail (h0 = its rest height above it)
    mw = rig.matrix_world
    contact = {}
    for p in PAWS:
        b = rig.data.bones[f"{p}_04"]; h, t = (mw @ b.head_local).z - floor, (mw @ b.tail_local).z - floor
        f = min(1.0, max(0.0, h / (h - t))) if h > t else 1.0
        contact[p] = (f, max(0.0, h + f * (t - h)))
    def paw_point(p):
        pb = rig.pose.bones[f"{p}_04"]
        return rig.matrix_world @ pb.head.lerp(pb.tail, contact[p][0])
    def paw_h(p):
        return paw_point(p).z - floor - contact[p][1]

    # leg length ratio per leg: hip to the contact point on the rig, hip to the paw end on the source
    def leg(arm, names, end):
        pts = [arm.matrix_world @ arm.data.bones[n].head_local for n in names] + [end]
        return sum((a - b).length for a, b in zip(pts, pts[1:]))
    src_leg, leg_ratio = {}, {}  # paw -> (source hip bone, source paw end bone), length ratio
    for p in PAWS:
        rb = rig.data.bones[f"{p}_04"]
        tgt_leg = leg(rig, [f"{p}_{i:02}" for i in range(1, 5)], mw @ rb.head_local.lerp(rb.tail_local, contact[p][0]))
        sl = sorted((j for j, r in role.items() if r.startswith(p + "_")), key=lambda j: depth(src.data.bones[j]))
        src_leg[p] = (sl[0], sl[-1])
        leg_ratio[p] = tgt_leg / leg(src, sl, src.matrix_world @ src.data.bones[sl[-1]].tail_local)
    ratio = leg_ratio["HL"]  # the hips' bob follows the hind legs ...
    ratio_xy = min(leg_ratio.values())  # ... their travel the shortest legs, so a lunge keeps every planted paw in reach
    hips = next(j for j, r in role.items() if r == "pelvis")
    print("RT leg ratio", {p: round(r, 4) for p, r in leg_ratio.items()}, "hips", hips)

    # each standard chain: its source bones (by role order, then depth), resampled onto it
    src_of = {}  # target bone -> (source bone, piece index, piece count) or None
    for chain in CHAINS:
        names = sorted((j for j, r in role.items() if r in chain), key=lambda j: (chain.index(role[j]), depth(src.data.bones[j])))
        groups = creature.chain_resample(names, len(chain), [src.data.bones[j].length for j in names])
        pieces = {}
        for g in groups:
            if g and "+" in g[-1]: pieces[g[-1].split("+")[0]] = pieces.get(g[-1].split("+")[0], 1) + 1
        for t, g in zip(chain, groups):
            if not g: src_of[t] = None; continue
            x, _, j = g[-1].partition("+")
            src_of[t] = (x, int(j or 0), pieces.get(x, 1))
    print("RT map", {t: v for t, v in src_of.items()})

    hr = {b.name: (src.matrix_world @ b.matrix_local).to_quaternion() for b in src.data.bones}  # source rest, world
    pel_rest = src.matrix_world @ src.data.bones[hips].head_local
    RB = {b.name: b.matrix_local.copy() for b in rig.data.bones}  # target rest, armature space
    order = []
    def walk(b):
        order.append(b); [walk(c) for c in b.children]
    [walk(b) for b in rig.data.bones if b.parent is None]
    rw = rig.matrix_world.to_quaternion()
    to_arm = rig.matrix_world.inverted().to_3x3()

    act = bpy.data.actions.new(clip); rig.animation_data_create(); rig.animation_data.action = act
    for pb in rig.pose.bones: pb.rotation_mode = "QUATERNION"
    f0 = int(src.animation_data.action.frame_range[0])
    n_src = int(src.animation_data.action.frame_range[1]) - f0 + 1
    N = n_src - 1  # keys 0..N; a looping clip's last frame equals its first (zoo_curate cuts it so)
    wrap = (lambda i: i % N) if loop else (lambda i: min(max(i, 0), N))
    # heading: the clip may face anywhere relative to the source's rest pose (Zoo runs often face back); turn it to
    # the rest heading (the rig's -Y) by the mean yaw of the hips over the clip
    hw = src.matrix_world.to_quaternion()
    fx = fy = 0.0
    for k in range(N + 1):
        sc.frame_set(f0 + k)
        v = ((hw @ src.pose.bones[hips].matrix.to_quaternion()) @ hr[hips].inverted()) @ Vector((0, -1, 0))
        a_ = math.atan2(v.y, v.x) - math.atan2(-1, 0); fx += math.cos(a_); fy += math.sin(a_)
    yaw = math.atan2(fy, fx)
    unturn = Quaternion((0, 0, 1), -yaw)
    print("RT clip heading vs rest (deg)", round(math.degrees(yaw), 1))
    OFF = []
    for k in range(N + 1):
        sc.frame_set(f0 + k); v = unturn @ (src.matrix_world @ src.pose.bones[hips].head - pel_rest)
        OFF.append(Vector((v.x * ratio_xy, v.y * ratio_xy, v.z * ratio)))
    start, drift = OFF[0].xy.to_3d(), (OFF[-1] - OFF[0]).xy.to_3d()  # in place: remove the xy travel, start to end
    OFF = [o - start - drift * (i / N) for i, o in enumerate(OFF)]
    zm = sum(o.z for o in OFF) / len(OFF)
    for o in OFF: o.z = zm + height_scale * (o.z - zm)

    Q = {b.name: [] for b in order}; LOC = []
    for k in range(N + 1):
        sc.frame_set(f0 + k)
        hw = src.matrix_world.to_quaternion()
        D = {j: unturn @ (hw @ src.pose.bones[j].matrix.to_quaternion()) @ hr[j].inverted() for j in role}
        Dt, P, first = {}, {}, {}
        for b in order:
            n = b.name; v = src_of.get(n)
            dpar = Dt[b.parent.name] if b.parent else Quaternion()
            if v is None:
                Dt[n] = dpar  # no source: keep the rest pose relative to the parent
            else:
                x, j, cnt = v
                if j == 0: first[x] = dpar
                Dt[n] = first[x].slerp(D[x], (j + 1) / cnt)  # a split bone's turn spread over its pieces
            rel = RB[n] if b.parent is None else RB[b.parent.name].inverted() @ RB[n]
            par = Matrix() if b.parent is None else P[b.parent.name]
            follow = par @ rel
            t = follow.translation + (to_arm @ OFF[k] if n == "pelvis" else Vector())
            r = rw.inverted() @ Dt[n] @ rw @ RB[n].to_quaternion() if n != "root" else follow.to_quaternion()
            P[n] = Matrix.LocRotScale(t, r, None)
            basis = rel.inverted() @ par.inverted() @ P[n]
            q = basis.to_quaternion()
            if Q[n] and Q[n][-1].dot(q) < 0: q.negate()
            Q[n].append(q)
            if n == "pelvis": LOC.append(basis.translation.copy())
    closure = {}
    if loop:  # close the loop: spread the end-vs-start mismatch over the cycle so the last key equals the first
        for n, qs in Q.items():
            closure[n] = angle(qs[0], qs[-1])
            err = qs[0] @ qs[-1].inverted()
            if err.w < 0: err.negate()
            for i in range(len(qs)): qs[i] = Quaternion().slerp(err, i / N) @ qs[i]
            qs[-1] = qs[0].copy()
        dl = LOC[0] - LOC[-1]
        LOC = [l + dl * (i / N) for i, l in enumerate(LOC)]

    def key_all():
        for i in range(N + 1):
            for b in order:
                pb = rig.pose.bones[b.name]; pb.rotation_quaternion = Q[b.name][i]
                pb.keyframe_insert("rotation_quaternion", frame=i + 1)
                if b.name == "pelvis": pb.location = LOC[i]; pb.keyframe_insert("location", frame=i + 1)

    def qmean(qs):
        m = Vector((0, 0, 0, 0))
        for q in qs: m += Vector(q) if Vector(q).dot(Vector(qs[0])) >= 0 else -Vector(q)
        return Quaternion(m).normalized()
    base_mean = qmean(Q[TAIL[0]])  # the base's motion is taken around its mean pose
    def set_tail(lift):
        """tail_02..06 from their rest pose (a generated model's rest tail rarely matches the source's, so only tail_01
        is retargeted): a lift (tail_02 only), a side sway, then a delayed share of the tail base's motion."""
        base = [base_mean.inverted() @ q for q in Q[TAIL[0]]]
        for k, n in enumerate(TAIL[1:], 1):
            C = (RB[TAIL[0]].inverted() @ RB[n]).to_quaternion()  # base's local frame -> this bone's frame
            up = (RB[n].to_3x3().inverted() @ (to_arm @ Vector((0, 0, 1)))).normalized()
            left = (RB[n].to_3x3().inverted() @ (to_arm @ Vector((1, 0, 0)))).normalized()  # facing -Y: +X lifts +Y up
            for i in range(N + 1):
                lag = C.inverted() @ base[wrap(i - TAIL_LAG * k)] @ C
                q = Quaternion(up, TAIL_SWAY * math.sin(2 * math.pi * (i - TAIL_LAG * k) / N)) @ Quaternion().slerp(lag, TAIL_SHARE)
                if k == 1: q = Quaternion(left, lift) @ q
                Q[n][i] = q

    # ground: shift so the median per-frame lowest paw sits on the floor
    key_all()
    lows = []
    for i in range(N + 1):
        sc.frame_set(i + 1); lows.append(min(paw_h(p) for p in PAWS))
    shift = -sorted(lows)[len(lows) // 2]
    pz = RB["pelvis"].to_3x3().inverted() @ (to_arm @ Vector((0, 0, shift)))
    LOC = [l + pz for l in LOC]

    hip = (mw @ rig.data.bones["pelvis"].head_local).z - floor
    lift = 0.0  # raise the tail until every tail joint clears the floor by TAIL_CLEAR of the hip height on every frame
    for _ in range(25):
        set_tail(lift); key_all()
        tl = 1e9
        for i in range(N + 1):
            sc.frame_set(i + 1); tl = min([tl] + [(rig.matrix_world @ rig.pose.bones[n].tail).z for n in TAIL])
        if tl > floor + TAIL_CLEAR * hip: break
        lift += 0.1
    print("RT tail lift", round(lift, 2), "lowest tail joint above floor (cm)", round((tl - floor) * 100, 2))

    # contact points: per paw and frame, where the retargeted leg puts it (the source's leg rotations on this rig;
    # UniMate animates the creature's own rig, so they land where the source's did) and its ankle, for the IK below.
    M = N if loop else N + 1  # distinct frames
    H = {p: [] for p in PAWS}; E = {p: [] for p in PAWS}; A = {p: [] for p in PAWS}  # height, contact point, ankle
    for i in range(M):
        sc.frame_set(i + 1)
        for p in PAWS:
            A[p].append((rig.matrix_world @ rig.pose.bones[f"{p}_03"].tail).copy()); E[p].append(paw_point(p)); H[p].append(paw_h(p))
    # contacts: per paw, planted = in the lowest CONTACT_SHARE of its height range and within CONTACT_M of its lowest
    # point. The IK's influence ramps over 2 frames in and out (rotations blend between FK and the pinned leg, so
    # the leg eases on and off the pin instead of snapping).
    idx = (lambda i: i % M) if loop else (lambda i: min(max(i, 0), M - 1))
    planted = {}
    for p in PAWS:
        hmin, hmax = min(H[p]), max(H[p])
        planted[p] = [H[p][i] <= min(hmin + CONTACT_SHARE * (hmax - hmin), hmin + CONTACT_M) for i in range(M)]
    # treadmill: the clip plays in place, so a gait's planted paws sweep back under the body at the travel speed
    # (the source's own travel was removed above; a curated loop had it removed at curation). Each stance run is
    # pinned to the straight line fitted through its contact points (a point when it barely moves: a stand), so the
    # stride survives; the clip's travel speed is the median sweep of its planted paws. Under TREADMILL_MIN of the
    # creature's length per frame a run's sweep is noise and it is pinned still.
    still = TREADMILL_MIN * size_cm / 100
    tgts, W, want, infl, vel = {}, {}, {}, {}, {}
    for p in PAWS:
        c = planted[p]
        def weight(i):  # 1 planted, 2/3 and 1/3 on the two frames either side
            d = next((d for d in (0, 1, 2) if c[idx(i - d)] or c[idx(i + d)]), 3)
            return 1 - d / 3
        w = [weight(i) for i in range(M)]
        pin, pv = [None] * M, [Vector((0, 0))] * M
        starts = [i for i in range(M) if c[i] and (not c[idx(i - 1)] or (not loop and i == 0))] or ([0] if all(c) else [])
        for s0 in starts:  # stance runs: each gets its own pin line
            run = []; i = s0
            while len(run) < M and (loop or i < M) and c[idx(i)]: run.append(idx(i)); i += 1
            if len(run) < MIN_STANCE:  # a paw skimming the floor for a frame or two (a lunge, a stumble) is not planted
                for j in run: c[j] = False
                continue
            mid = (len(run) - 1) / 2
            xy = sum((E[p][j].xy for j in run), Vector((0, 0))) / len(run)
            v = sum(((d - mid) * (E[p][j].xy - xy) for d, j in enumerate(run)), Vector((0, 0))) / (sum((d - mid) ** 2 for d in range(len(run))) or 1)
            if v.length < still: v = Vector((0, 0))
            print("RT stance", p, f"frames {s0}+{len(run)}", "sweep cm/frame", round(v.length * 100, 2), "fit residual cm",
                  round(max((E[p][j].xy - xy - v * (d - mid)).length for d, j in enumerate(run)) * 100, 2))
            for d in range(-2, len(run) + 2):
                if (loop or 0 <= s0 + d < M) and pin[idx(s0 + d)] is None: pin[idx(s0 + d)], pv[idx(s0 + d)] = xy + v * (d - mid), v
        vel[p] = pv
        T, X, I = [], [], []
        for i in range(M):  # the ankle's IK target, moved by how far its contact point should move
            e = A[p][i].copy(); xy = E[p][i].xy.copy(); h = H[p][i]
            on = pin[i] is not None and w[i] > 0
            if on:
                xy, h = pin[i].copy(), FLOOR_EPS
            e.xy += xy - E[p][i].xy; e.z += max(h, FLOOR_EPS) - H[p][i]
            T.append(e); X.append(xy if on else None)
            I.append(w[i] if on else 1.0 if H[p][i] < FLOOR_EPS else 0.0)  # off the pin: IK only to lift a sunk paw
        tgts[p], W[p], want[p], infl[p] = T, [x if pin[i] is not None else 0.0 for i, x in enumerate(w)], X, I
        print("RT contact", p, "".join("#" if x else ("+" if y > 0 else ".") for x, y in zip(c, W[p])))
    sweeps = sorted(vel[p][i].length for p in PAWS for i in range(M) if W[p][i] == 1)
    tread = sweeps[len(sweeps) // 2] if sweeps else 0.0  # the clip's treadmill speed, m per frame
    print("RT treadmill cm/frame", round(tread * 100, 2), "cm/s", round(tread * 100 * sc.render.fps, 1))

    # IK to the targets; the IK also turns the paw, so correct the targets from the solved contact points, then bake to FK
    bpy.context.view_layer.objects.active = rig; bpy.ops.object.mode_set(mode="POSE")
    emp = {}
    for p in PAWS:
        e = bpy.data.objects.new("tgt_" + p, None); sc.collection.objects.link(e); emp[p] = e
        con = rig.pose.bones[f"{p}_03"].constraints.new("IK"); con.target = e; con.chain_count = 3
        con.use_tail = True; con.use_stretch = False
    def key_targets():
        for p, e in emp.items():
            e.animation_data_clear()
            con = rig.pose.bones[f"{p}_03"].constraints[-1]
            for i in range(N + 1):
                e.location = tgts[p][idx(i)]; e.keyframe_insert("location", frame=i + 1)
                con.influence = infl[p][idx(i)]; con.keyframe_insert("influence", frame=i + 1)
    released = []  # pins the leg could not reach (a lunge's far stance end): those frames are not planted
    for it in range(5):
        key_targets(); worst, where = 0.0, ""
        for i in range(M):
            sc.frame_set(i + 1)
            for p in PAWS:
                h = paw_h(p)
                if infl[p][i] < 1:  # ramp and swing frames blend with FK: nothing to correct, unless the paw sank
                    if h < 0: infl[p][i] = 1.0; worst = max(worst, 0.01); where = where or f"{p} frame {i} sank"  # then full IK from the next pass
                    continue
                dz = FLOOR_EPS - h if W[p][i] == 1 else max(0.0, FLOOR_EPS - h)  # planted: onto the floor; else: not below
                dxy = want[p][i] - paw_point(p).xy if W[p][i] == 1 else Vector((0, 0))  # pinned: onto the pin
                if it >= 2 and W[p][i] == 1 and dxy.length > UNREACHED:  # the leg cannot hold this pin: release it
                    W[p][i], want[p][i], infl[p][i], dxy = 0.0, None, (1.0 if h < FLOOR_EPS else 0.0), Vector((0, 0))
                    released.append(f"{p} {i}")
                tgts[p][i].z += dz; tgts[p][i].xy += dxy
                if max(abs(dz), dxy.length) > worst: worst, where = max(abs(dz), dxy.length), f"{p} frame {i}"
        print("RT ik pass", it, "largest fix (cm)", round(worst * 100, 2), where)
        if worst < 0.001: break
    if released: print("RT released pins the leg could not reach:", " ".join(released))
    key_targets()
    for b in rig.data.bones: b.select = True
    bpy.ops.nla.bake(frame_start=1, frame_end=N + 1, only_selected=False, visual_keying=True, clear_constraints=True,
                     use_current_action=True, bake_types={"POSE"})
    bpy.ops.object.mode_set(mode="OBJECT")
    for e in emp.values(): bpy.data.objects.remove(e)
    act = rig.animation_data.action
    for x in list(bpy.data.actions):  # the source's and the IK targets' actions; only the clip is exported
        if x != act: bpy.data.actions.remove(x)
    act.name = clip
    for b in order:  # keep quaternion signs continuous after the bake
        fc = [act.fcurves.find(f'pose.bones["{b.name}"].rotation_quaternion', index=j) for j in range(4)]
        if None in fc: continue
        prev = None
        for k in range(len(fc[0].keyframe_points)):
            q = Vector([f.keyframe_points[k].co.y for f in fc])
            if prev is not None and q.dot(prev) < 0:
                for f in fc: f.keyframe_points[k].co.y *= -1
                q = -q
            prev = q
        for f in fc: f.update()

    # a paw the blends left a touch under the floor: the whole clip rises by that much (up to SINK_LIFT of the length)
    sunk = -min(paw_h(p) for i in range(N + 1) for p in PAWS if not sc.frame_set(i + 1))
    if 0 < sunk <= SINK_LIFT * size_cm / 100:
        dz = RB["pelvis"].to_3x3().inverted() @ (to_arm @ Vector((0, 0, sunk + FLOOR_EPS)))
        for j in range(3):
            fc = act.fcurves.find('pose.bones["pelvis"].location', index=j)
            for kp in fc.keyframe_points: kp.co.y += dz[j]
            fc.update()
        print("RT lifted the clip (cm)", round((sunk + FLOOR_EPS) * 100, 2))
    # the body never goes through the floor: in a collapse or a lie the mesh's lowest point (belly, side, head) is
    # kept on it, the pelvis rising per frame by what that needs, the lift dilated 2 frames each way so it eases in
    import numpy as np
    meshes = [o for o in sc.objects if o.type == "MESH" and o.parent == rig]
    dg = bpy.context.evaluated_depsgraph_get()
    need = []
    for i in range(N + 1):
        sc.frame_set(i + 1)
        low = floor
        for o in meshes:
            me = o.evaluated_get(dg).data
            co = np.empty(len(me.vertices) * 3, np.float32); me.vertices.foreach_get("co", co)
            Mw = np.array(o.matrix_world)
            low = min(low, float((co.reshape(-1, 3) @ Mw[:3, :3].T + Mw[:3, 3])[:, 2].min()))
        need.append(floor - low if floor - low > BODY_SINK else 0.0)
    lifts = [max(need[idx(i + d)] if loop else need[min(max(i + d, 0), N)] for d in range(-2, 3)) for i in range(N + 1)]
    if max(lifts) > 0:
        fcs = [act.fcurves.find('pose.bones["pelvis"].location', index=j) for j in range(3)]
        for k, lift in enumerate(lifts):
            dz = RB["pelvis"].to_3x3().inverted() @ (to_arm @ Vector((0, 0, lift)))
            for j, fc in enumerate(fcs): fc.keyframe_points[k].co.y += dz[j]
        for fc in fcs: fc.update()
        print("RT body lifted off the floor, most (cm)", round(max(lifts) * 100, 2), "frames", sum(1 for l in lifts if l > 0))
    # metrics, from the baked clip
    heights = {p: [] for p in PAWS}; pts = {p: [] for p in PAWS}; R = {b.name: [] for b in order}
    for i in range(N + 1):
        sc.frame_set(i + 1)
        for p in PAWS: heights[p].append(paw_h(p)); pts[p].append(paw_point(p))
        for b in order: R[b.name].append(rig.pose.bones[b.name].matrix_basis.to_quaternion())
    slide = max([(pts[p][idx(i + 1)].xy - pts[p][i].xy - vel[p][i]).length for p in PAWS for i in range(M - (0 if loop else 1))
                 if W[p][i] == 1 and W[p][idx(i + 1)] == 1] or [0.0])  # a planted paw's step off the treadmill line
    flips = [(n, i) for n, qs in R.items() for i in range(N) if angle(qs[i], qs[i + 1]) > FLIP_DEG]
    # seam: how far the loop's ends had to be pulled together (the retarget's closure), or still differ after cleanup
    seam, seam_bone = 0.0, ""
    if loop:
        for n, qs in R.items():
            v = max(closure[n], angle(qs[N], qs[0]))
            if v > seam: seam, seam_bone = v, n
    steps = {n: max(angle(qs[i], qs[i + 1]) for i in range(N)) for n, qs in R.items()}
    big = max(steps, key=steps.get)
    tail_jump = max(angle(R[n][i], R[n][i + 1]) for n in TAIL for i in range(N))
    m = {"min_paw_cm": round(min(min(v) for v in heights.values()) * 100, 2), "max_slide_cm": round(slide * 100, 2),
         "seam_deg": round(seam, 2), "flips": len(flips), "travel_cm_s": round(tread * 100 * sc.render.fps, 1)}
    info = {"clip": clip, "loop": loop, "frames": N + 1, "fps": sc.render.fps, "size_cm": size_cm, "scale": s,
            "leg_ratio": ratio, "tail_lift": lift, "seam_bone": seam_bone, "flip_bones": sorted({n for n, _ in flips}),
            "tail_max_step_deg": round(tail_jump, 1), "max_step_deg": round(steps[big], 1), "max_step_bone": big, "contacts": {p: "".join("#" if x else "." for x in planted[p]) for p in PAWS},
            "heights_m": heights}
    for p, v in heights.items(): print("RT paw", p, " ".join(f"{x*100:4.1f}" for x in v))
    print("RT metrics", json.dumps(m), "seam bone", seam_bone, "largest step", info["max_step_deg"], big,
          "tail", info["tail_max_step_deg"])
    Path(outj).write_text(json.dumps({**m, **info}))

    sc.frame_start, sc.frame_end = 1, N + 1
    bpy.data.objects.remove(src)
    bpy.ops.object.select_all(action="DESELECT")
    for o in sc.objects: o.select_set(o == rig or o.parent == rig)
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True, export_animations=True,
                              export_animation_mode="ACTIONS", export_force_sampling=True, export_frame_range=True)
    print("RT wrote", out, "frames", N + 1)


if __name__ == "__main__":
    a = sys.argv[sys.argv.index("--") + 1:]
    try:
        main(a[0], a[1], a[2], a[3], a[4], float(a[5]), float(a[6]))
    except Exception:  # Blender exits 0 after a script error; the job must see the failure
        import traceback; traceback.print_exc(); sys.exit(1)
