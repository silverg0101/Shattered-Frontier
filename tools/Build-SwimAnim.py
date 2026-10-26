"""Build /Game/Frontier/Characters/Player/Anim/AS_SF_Swim: a looping front-crawl clip for the UE4 mannequin skeleton.

py -3 tools/Run-InEditor.py tools/Build-SwimAnim.py

No licensed swim clip exists in the project, so the clip is authored here from the skeleton's reference pose: the body leans
into the water, arms windmill in antiphase (pull under the body, recover over it), legs flutter-kick, the head stays up.
ASFE1Character::UpdateSwimPose plays it while CharacterMovement is swimming (the replicated swim flag) and returns to
ThirdPerson_AnimBP afterwards; the MetaHuman retarget reads the mannequin mesh, so it follows. Re-run to rebuild. Tuning
knobs are the constants below (degrees, seconds).
"""
import math
import unreal as u

OUT_DIR, NAME = '/Game/Frontier/Characters/Player/Anim', 'AS_SF_Swim'
SKELETON = '/Game/Mannequin/Character/Mesh/SK_Mannequin_Skeleton'
FPS, SECONDS = 60, 1.2            # one stroke cycle, looped
LEAN, ROLL, LIFT_CM = 76.0, 12.0, 52.0   # body pitch into the water, roll with each stroke, pelvis lift
ARM_SPLAY, KICK, KNEE = 8.0, 20.0, 14.0
SPINE_ARCH = (1.0, 1.0, 1.0)      # spine_01..03 extension, then neck/head to look ahead
NECK, HEAD = 40.0, 28.0

Q = lambda x, y, z, w: (x, y, z, w)


def mul(a, b):
    ax, ay, az, aw = a; bx, by, bz, bw = b
    return (aw*bx + ax*bw + ay*bz - az*by, aw*by - ax*bz + ay*bw + az*bx, aw*bz + ax*by - ay*bx + az*bw, aw*bw - ax*bx - ay*by - az*bz)


def inv(a): return (-a[0], -a[1], -a[2], a[3])


def axis(ax, deg):
    s, c = math.sin(math.radians(deg) / 2), math.cos(math.radians(deg) / 2)
    return (ax[0]*s, ax[1]*s, ax[2]*s, c)


X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)
rx = lambda d: axis(X, d); ry = lambda d: axis(Y, d)

skel = u.load_asset(SKELETON)
assert skel, SKELETON
ref = u.AnimPoseExtensions.get_reference_pose(skel)


def world(bone):
    q = u.AnimPoseExtensions.get_ref_bone_pose(ref, bone, u.AnimPoseSpaces.WORLD).rotation
    return (q.x, q.y, q.z, q.w)


def local(bone):
    t = u.AnimPoseExtensions.get_ref_bone_pose(ref, bone, u.AnimPoseSpaces.LOCAL)
    return t.translation, t.scale3d


PARENT = {'pelvis': 'root', 'spine_01': 'pelvis', 'spine_02': 'spine_01', 'spine_03': 'spine_02', 'neck_01': 'spine_03', 'head': 'neck_01',
          'clavicle_l': 'spine_03', 'upperarm_l': 'clavicle_l', 'lowerarm_l': 'upperarm_l', 'hand_l': 'lowerarm_l',
          'clavicle_r': 'spine_03', 'upperarm_r': 'clavicle_r', 'lowerarm_r': 'upperarm_r', 'hand_r': 'lowerarm_r',
          'thigh_l': 'pelvis', 'calf_l': 'thigh_l', 'foot_l': 'calf_l', 'thigh_r': 'pelvis', 'calf_r': 'thigh_r', 'foot_r': 'calf_r'}


def delta_for(bone, t):
    """Component-space delta D(t) of one bone, applied before its parents' (so it swings in the leaned body's own frame)."""
    side = 0.5 if bone.endswith('_r') else 0.0
    ta = (t + side) % 1.0
    if bone == 'pelvis': return mul(ry(ROLL * math.sin(2 * math.pi * t)), rx(-LEAN))
    if bone in ('spine_01', 'spine_02', 'spine_03'): return rx(SPINE_ARCH[int(bone[-1]) - 1])
    if bone == 'neck_01': return rx(NECK)
    if bone == 'head': return rx(HEAD)
    if bone.startswith('upperarm'):
        phi = 180.0 - 360.0 * ta      # 180 = reaching ahead, 90 = pulling under the body, 0 = trailing, -90 = recovering over it
        return mul(axis(Z, ARM_SPLAY * (1 if bone.endswith('_l') else -1)), rx(phi))
    if bone.startswith('lowerarm'):
        u_ = ta * 2 if ta < 0.5 else (ta - 0.5) * 2
        return rx(20 + (25 if ta < 0.5 else 55) * math.sin(math.pi * u_))
    if bone.startswith('thigh'): return rx(KICK * math.sin(2 * math.pi * 2 * ta))
    if bone.startswith('calf'): return rx(KNEE + KNEE * math.sin(2 * math.pi * 2 * ta + 1.2))
    return (0, 0, 0, 1)


frames = int(FPS * SECONDS)
path = f'{OUT_DIR}/{NAME}'
if u.EditorAssetLibrary.does_asset_exist(path):
    u.EditorAssetLibrary.delete_asset(path)
factory = u.AnimSequenceFactory(); factory.set_editor_property('target_skeleton', skel)
seq = u.AssetToolsHelpers.get_asset_tools().create_asset(NAME, OUT_DIR, u.AnimSequence, factory)
ctl = seq.controller
ctl.open_bracket('Build swim clip', False)
ctl.set_frame_rate(u.FrameRate(FPS, 1), False)
ctl.set_number_of_frames(u.FrameNumber(frames), False)       # frames + 1 keys; the last repeats the first, so the loop closes
for bone in PARENT:
    ctl.add_bone_curve(bone, False)
    wb, wp = world(bone), world(PARENT[bone])
    pos, scl = local(bone)
    poss, rots, scls = [], [], []
    for k in range(frames + 1):
        t = (k % frames) / frames
        q = mul(mul(inv(wp), delta_for(bone, t)), wb)
        rots.append(u.Quat(*q)); scls.append(scl)
        poss.append(u.Vector(pos.x, pos.y, pos.z + (LIFT_CM if bone == 'pelvis' else 0)))
    ctl.set_bone_track_keys(bone, poss, rots, scls, False)
ctl.close_bracket(False)
u.EditorAssetLibrary.save_loaded_asset(seq)
print('SWIM_ANIM_BUILT', path, 'frames', frames, 'length_s', seq.get_editor_property('sequence_length'))
