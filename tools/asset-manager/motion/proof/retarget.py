"""blender -b -P retarget.py -- SRC.bvh RAT.glb OUT.glb I J [PAWS.json]
Retarget a Zoo-300K hamster BVH (3ds Max Biped) onto the v100 rat rig (bone_N) by world-space rotation
deltas from each rig's rest pose. Source frames I..J (0-based) become one loop; J's pose is blended onto I's.
Root: pelvis offset from rest scaled by the hind-leg-length ratio, vertical bob scaled by BOB; in place.
Cleanup: tail chain is a lagging, swaying follow-through; paws are pinned to the floor during contact with
IK and baked back to FK; no paw tip goes below the floor. PAWS.json gets per-paw tip heights for plotting."""
import bpy, sys, json, math
from mathutils import Matrix, Quaternion, Vector
args = sys.argv[sys.argv.index("--") + 1:]
src, ratf, out, I, J = args[:5]; I, J = int(I), int(J)
paws_json = args[5] if len(args) > 5 else None

# tuning knobs
BOB = 0.65          # share of the source's vertical root bob kept (rat runs lower than the hamster)
TAIL_LIFT = 1.3     # radians: the rest tail hangs to the floor; lift it back at bone_54
TAIL_SHARE = 0.35   # share of the tail base's rotation each later segment repeats
TAIL_LAG = 1        # frames of delay per segment
TAIL_SWAY = 0.10    # radians of side sway per segment, one sway per loop
CONTACT_H = 0.025   # m above the paw's lowest point: lower than this, and not among its fastest quarter, is planted
FLOOR_EPS = 0.002   # m: keep tips this far above the floor

# hamster joint -> rat bones (several rat bones may follow one source joint)
MAP = {"Bip01_Pelvis": ["bone_0"], "Bip01_Spine": ["bone_1"], "Bip01_Spine1": ["bone_2"],
       "Bip01_Spine2": ["bone_3"], "Bip01_Neck": ["bone_4"], "Bip01_Head": ["bone_6"], "BN_Tail": ["bone_53"]}
for s, r in (("L", (15, 19, 21, 23, 25, 39, 43, 44, 45)), ("R", (27, 31, 33, 35, 37, 46, 50, 51, 52))):
    sh, fing, th, toes = r[0], r[1:5], r[5], r[6:]
    for k, n in zip(("Clavicle", "UpperArm", "Forearm", "Hand"), range(sh, sh + 4)): MAP[f"Bip01_{s}_{k}"] = [f"bone_{n}"]
    MAP[f"Bip01_{s}_Finger0"] = [f"bone_{n}" for n in fing]
    for k, n in zip(("Thigh", "Calf", "HorseLink", "Foot"), range(th, th + 4)): MAP[f"Bip01_{s}_{k}"] = [f"bone_{n}"]
    MAP[f"Bip01_{s}_Toe0"] = [f"bone_{n}" for n in toes]
SRC_OF = {t: s for s, ts in MAP.items() for t in ts}
TAIL = [f"bone_{n}" for n in range(53, 59)]
# paw: IK end bone, chain length, tip bones whose tails touch the floor
PAWS = {"LF": ("bone_18", 3, [f"bone_{n}" for n in (20, 22, 24, 26)]),
        "RF": ("bone_30", 3, [f"bone_{n}" for n in (32, 34, 36, 38)]),
        "LH": ("bone_42", 4, ["bone_43", "bone_44", "bone_45"]),
        "RH": ("bone_49", 4, ["bone_50", "bone_51", "bone_52"])}

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene; sc.render.fps = 30
bpy.ops.import_scene.gltf(filepath=ratf)
rat = next(o for o in bpy.data.objects if o.type == "ARMATURE")
for o in list(bpy.data.objects):  # the importer's bone-shape icosphere is not part of the asset
    if o.type == "MESH" and o.name.startswith("Icosphere"): bpy.data.objects.remove(o)
bpy.ops.import_anim.bvh(filepath=src)
ham = next(o for o in bpy.data.objects if o.type == "ARMATURE" and o != rat)

def chain_len(arm, names):
    p = [arm.matrix_world @ arm.data.bones[n].head_local for n in names]
    return sum((a - b).length for a, b in zip(p, p[1:]))
ratio = chain_len(rat, ["bone_39", "bone_40", "bone_41", "bone_42", "bone_43"]) / \
        chain_len(ham, ["Bip01_L_Thigh", "Bip01_L_Calf", "Bip01_L_HorseLink", "Bip01_L_Foot", "Bip01_L_Toe0"])
print("RT ratio", ratio)

hr = {b.name: (ham.matrix_world @ b.matrix_local).to_quaternion() for b in ham.data.bones}  # source rest, world
pel_rest = ham.matrix_world @ ham.data.bones["Bip01_Pelvis"].head_local
RB = {b.name: b.matrix_local.copy() for b in rat.data.bones}  # target rest, armature space
order = []
def walk(b):
    order.append(b); [walk(c) for c in b.children]
[walk(b) for b in rat.data.bones if b.parent is None]
rw = rat.matrix_world.to_quaternion()
to_arm = rat.matrix_world.inverted().to_3x3()

act = bpy.data.actions.new("HamsterRun"); rat.animation_data_create(); rat.animation_data.action = act
for pb in rat.pose.bones: pb.rotation_mode = "QUATERNION"
f0 = int(ham.animation_data.action.frame_range[0])
frames = list(range(I, J + 1)); N = len(frames) - 1  # frame N repeats frame 0
OFF = []
for k in frames:
    sc.frame_set(f0 + k); OFF.append((ham.matrix_world @ ham.pose.bones["Bip01_Pelvis"].head - pel_rest) * ratio)
zm = sum(o.z for o in OFF) / len(OFF)
for o in OFF: o.z = zm + BOB * (o.z - zm)

Q = {b.name: [] for b in order}; LOC = []
for fi, k in enumerate(frames):
    sc.frame_set(f0 + k)
    hw = ham.matrix_world.to_quaternion()
    D = {s: (hw @ ham.pose.bones[s].matrix.to_quaternion()) @ hr[s].inverted() for s in MAP}
    P = {}
    for b in order:
        n = b.name; s = SRC_OF.get(n)
        rel = RB[n] if b.parent is None else RB[b.parent.name].inverted() @ RB[n]
        par = Matrix() if b.parent is None else P[b.parent.name]
        follow = par @ rel  # where the bone goes if it keeps its rest pose relative to its parent
        t = RB[n].translation + to_arm @ OFF[fi] if b.parent is None else follow.translation
        r = (rw.inverted() @ D[s] @ rw @ RB[n].to_quaternion()) if s else follow.to_quaternion()
        P[n] = Matrix.LocRotScale(t, r, None)
        basis = rel.inverted() @ par.inverted() @ P[n]
        q = basis.to_quaternion()
        if Q[n] and Q[n][-1].dot(q) < 0: q.negate()
        Q[n].append(q)
        if b.parent is None: LOC.append(basis.translation.copy())
# close the loop: spread the end-vs-start mismatch over the cycle so the last key equals the first
for n, qs in Q.items():
    err = qs[0] @ qs[-1].inverted()
    if err.w < 0: err.negate()
    for i in range(len(qs)): qs[i] = Quaternion().slerp(err, i / N) @ qs[i]
    qs[-1] = qs[0].copy()
dl = LOC[0] - LOC[-1]
LOC = [l + dl * (i / N) for i, l in enumerate(LOC)]

def key_all():
    for i in range(N + 1):
        for b in order:
            pb = rat.pose.bones[b.name]; pb.rotation_quaternion = Q[b.name][i]
            pb.keyframe_insert("rotation_quaternion", frame=i + 1)
            if b.parent is None: pb.location = LOC[i]; pb.keyframe_insert("location", frame=i + 1)

def set_tail(lift):
    """Bones 54..58: lift (54 only), then a delayed share of the tail base's rotation, then a side sway."""
    base = Q["bone_53"]
    for k, n in enumerate(TAIL[1:], 1):
        C = (RB["bone_53"].inverted() @ RB[n]).to_quaternion()  # base's local frame -> this bone's frame
        up = (RB[n].to_3x3().inverted() @ (to_arm @ Vector((0, 0, 1)))).normalized()
        left = (RB[n].to_3x3().inverted() @ (to_arm @ Vector((0, 1, 0)))).normalized()
        for i in range(N + 1):
            lag = C.inverted() @ base[(i - TAIL_LAG * k) % N] @ C
            q = Quaternion().slerp(lag, TAIL_SHARE)
            sway = Quaternion(up, TAIL_SWAY * math.sin(2 * math.pi * (i - TAIL_LAG * k) / N))
            q = sway @ q
            if k == 1: q = Quaternion(left, lift) @ q
            Q[n][i] = q

ground = min((rat.matrix_world @ rat.data.bones[t].tail_local).z for _, _, tips in PAWS.values() for t in tips)
def tips_low(p):
    return min((rat.matrix_world @ rat.pose.bones[t].tail).z for t in PAWS[p][2])

# ground: shift so the median per-frame lowest paw sits on the floor (paws touch in turn)
key_all()
lows = []
for i in range(N):
    sc.frame_set(i + 1); lows.append(min(tips_low(p) for p in PAWS))
shift = ground - sorted(lows)[len(lows) // 2]
for i in range(N + 1): LOC[i] += RB["bone_0"].to_3x3().inverted() @ (to_arm @ Vector((0, 0, shift)))

# tail: raise the lift until no tail joint reaches the floor on any frame
lift = TAIL_LIFT
for _ in range(10):
    set_tail(lift); key_all()
    tl = 1e9
    for i in range(N):
        sc.frame_set(i + 1); tl = min([tl] + [(rat.matrix_world @ rat.pose.bones[n].tail).z for n in TAIL])
    if tl > ground + 0.01: break
    lift += 0.1
print("RT tail lift", round(lift, 2), "lowest tail joint above floor", round(tl - ground, 3))

# contacts: per paw, planted frames = within CONTACT_H of its lowest point (and under 4 cm). The source's paws
# sweep fast even when down (it runs in place), so speed can't separate stance from swing here; every stance
# run is pinned at its mean xy on the floor instead, with a 2-frame ramp in and out.
H = {p: [] for p in PAWS}; E = {p: [] for p in PAWS}
for i in range(N):
    sc.frame_set(i + 1)
    for p, (end, _, _) in PAWS.items():
        E[p].append((rat.matrix_world @ rat.pose.bones[end].tail).copy()); H[p].append(tips_low(p) - ground)
tgts, W = {}, {}
for p in PAWS:
    hmin = min(H[p])
    c = [H[p][i] < min(hmin + CONTACT_H, 0.04) for i in range(N)]
    def weight(i):  # 1 planted, 2/3 and 1/3 on the two frames either side
        d = next((d for d in (0, 1, 2) if c[(i - d) % N] or c[(i + d) % N]), 3)
        return 1 - d / 3
    w = [weight(i) for i in range(N)]
    pin = [None] * N
    starts = [i for i in range(N) if c[i] and not c[i - 1]] or ([0] if all(c) else [])
    for s0 in starts:  # stance runs (cyclic): each gets its own pin point
        run = []; i = s0
        while c[i % N] and len(run) < N: run.append(i % N); i += 1
        xy = sum((E[p][j].xy for j in run), Vector((0, 0))) / len(run)
        for d in range(-2, len(run) + 2): pin[(s0 + d) % N] = pin[(s0 + d) % N] or xy
    T = []
    for i in range(N):
        e = E[p][i].copy(); off = e.z - (H[p][i] + ground)  # end-bone tail height above its lowest tip
        if pin[i] is not None and w[i] > 0:
            e.xy = e.xy.lerp(pin[i], w[i]); e.z += w[i] * (ground + FLOOR_EPS - (e.z - off))
        e.z = max(e.z, ground + FLOOR_EPS + off)
        T.append(e)
    tgts[p], W[p] = T, [x if pin[i] is not None else 0.0 for i, x in enumerate(w)]
    print("RT contact", p, "".join("#" if x else ("+" if y > 0 else ".") for x, y in zip(c, W[p])))

# IK to the targets; the IK also turns the paw, so correct target heights from the solved tips, then bake to FK
bpy.context.view_layer.objects.active = rat; bpy.ops.object.mode_set(mode="POSE")
emp = {}
for p, (end, chain, _) in PAWS.items():
    e = bpy.data.objects.new("tgt_" + p, None); sc.collection.objects.link(e); emp[p] = e
    con = rat.pose.bones[end].constraints.new("IK"); con.target = e; con.chain_count = chain
    con.use_tail = True; con.use_stretch = False
def key_targets():
    for p, e in emp.items():
        e.animation_data_clear()
        for i in range(N + 1):
            e.location = tgts[p][i % N]; e.keyframe_insert("location", frame=i + 1)
for it in range(4):
    key_targets(); worst = 0.0
    for i in range(N):
        sc.frame_set(i + 1)
        for p in PAWS:
            h = tips_low(p) - ground
            dz = FLOOR_EPS - h if W[p][i] == 1 else max(0.0, FLOOR_EPS - h)  # planted: onto the floor; else: not below
            tgts[p][i].z += dz; worst = max(worst, abs(dz))
    print("RT ik pass", it, "largest height fix (cm)", round(worst * 100, 2))
    if worst < 0.001: break
key_targets()
for b in rat.data.bones: b.select = True
bpy.ops.nla.bake(frame_start=1, frame_end=N + 1, only_selected=False, visual_keying=True, clear_constraints=True,
                 use_current_action=True, bake_types={"POSE"})
bpy.ops.object.mode_set(mode="OBJECT")
emp = list(emp.values())
for e in emp: bpy.data.objects.remove(e)
act = rat.animation_data.action
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

heights = {p: [] for p in PAWS}
for i in range(N + 1):
    sc.frame_set(i + 1)
    for p in PAWS: heights[p].append(tips_low(p) - ground)
mn = min(min(v) for v in heights.values())
for p, v in heights.items(): print("RT paw", p, " ".join(f"{x*100:4.1f}" for x in v))
print("RT min paw height above floor (cm)", round(mn * 100, 2))
if paws_json: json.dump({"fps": 30, "floor": 0.0, "heights_m": heights}, open(paws_json, "w"))
sc.frame_start, sc.frame_end = 1, N + 1
bpy.data.objects.remove(ham)
bpy.ops.object.select_all(action="DESELECT")
for o in bpy.data.objects: o.select_set(o == rat or o.parent == rat)
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True, export_animations=True,
                          export_animation_mode="ACTIVE_ACTIONS", export_force_sampling=True, export_frame_range=True)
print("RT wrote", out, "frames", N + 1)
