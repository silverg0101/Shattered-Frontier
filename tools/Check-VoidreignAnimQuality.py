"""Measure Voidreign animation faults from the imported Unreal sequences (Editor Python; editor open or headless).

    python tools/Run-InEditor.py tools/Check-VoidreignAnimQuality.py

Per clip, from component-space bone transforms at every frame:
  - flips: largest frame-to-frame rotation of any bone (deg); tail/mane chains and legs reported separately;
  - buckle: leg joints whose bend direction (sign of the sagittal joint angle) differs from the rest pose or
    reverses mid-clip.
Writes Artifacts/Art/Voidreign/anim-quality.json and logs a ranked summary.
"""
import json, math
from pathlib import Path
import unreal

PROJECT = Path(__file__).resolve().parents[1]
FOLDER = '/Game/Frontier/Creatures/Voidreign/V1/Animations'
LEGS = {'fore_L': ['shoulder', 'elbow', 'carpus', 'fetlock'], 'fore_R': ['shoulder', 'elbow', 'carpus', 'fetlock'],
        'hind_L': ['hip', 'stifle', 'hock', 'fetlock'], 'hind_R': ['hip', 'stifle', 'hock', 'fetlock']}
FLIP_DEG = 35.0
BODY = 'PRI_body_pelvis'


def comp_space(seq, frame, names):
    """Component-space transforms by composing local poses up the hierarchy of the reference skeleton."""
    skel = seq.get_editor_property('skeleton'); ref = unreal.AnimPoseExtensions.get_reference_pose(skel)
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(seq, frame, unreal.AnimPoseEvaluationOptions())
    return {n: unreal.AnimPoseExtensions.get_bone_pose(pose, n, unreal.AnimPoseSpaces.WORLD) for n in names}


def angle_between(q0, q1):
    d = abs(q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w)
    return math.degrees(2 * math.acos(min(1.0, d)))


def bend(a, b, c, body):
    """Signed sagittal bend at b for chain a-b-c in the pelvis frame (world axes misread it once the body turns):
    sign of the cross product's lateral (X) component."""
    a, b, c = (body.inverse_transform_location(t.translation) for t in (a, b, c))
    u = b - a; v = c - b
    return u.y * v.z - u.z * v.y


results = []
reg = unreal.AssetRegistryHelpers.get_asset_registry()
reg.scan_paths_synchronous([FOLDER], True)  # EditorAssetLibrary.list_assets can miss folders created outside this session
seqs = [d.get_asset() for d in reg.get_assets_by_path(FOLDER, recursive=False)]
seqs = [s for s in seqs if isinstance(s, unreal.AnimSequence)]
skel = seqs[0].get_editor_property('skeleton')
names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(unreal.AnimPoseExtensions.get_reference_pose(skel))]
ref = unreal.AnimPoseExtensions.get_reference_pose(skel)
ref_ws = {n: unreal.AnimPoseExtensions.get_ref_bone_pose(ref, n, unreal.AnimPoseSpaces.WORLD) for n in names}
ref_bend = {}
for leg, parts in LEGS.items():
    chain = [f'PRI_{leg}_{p}' for p in parts]
    for i in (1, 2): ref_bend[chain[i]] = bend(ref_ws[chain[i - 1]], ref_ws[chain[i]], ref_ws[chain[i + 1]], ref_ws[BODY])
for seq in seqs:
    n = unreal.AnimationLibrary.get_num_frames(seq)
    prev = None; worst = dict(tail=(0, None, None), mane=(0, None, None), leg=(0, None, None), body=(0, None, None))
    buckles = {}
    for f in range(n):
        ws = comp_space(seq, f, names)
        if prev:
            for b in names:
                d = angle_between(prev[b].rotation, ws[b].rotation)
                group = 'tail' if 'tail' in b else 'mane' if 'mane' in b else 'leg' if any(k in b for k in LEGS) else 'body'
                if d > worst[group][0]: worst[group] = (round(d, 1), b, f)
        for leg, parts in LEGS.items():
            chain = [f'PRI_{leg}_{p}' for p in parts]
            for i in (1, 2):
                s = bend(ws[chain[i - 1]], ws[chain[i]], ws[chain[i + 1]], ws[BODY])
                if s * ref_bend[chain[i]] < 0: buckles.setdefault(chain[i], []).append(f)
        prev = ws
    flips = {g: v for g, v in worst.items() if v[0] > FLIP_DEG}
    results.append(dict(clip=seq.get_name().replace('A_Voidreign_', ''), frames=n, worst_step_deg=worst,
                        flips=flips, buckled_joints={j: [fr[0], fr[-1], len(fr)] for j, fr in buckles.items()},
                        score=sum(v[0] for v in flips.values()) + 10 * sum(len(fr) for fr in buckles.values()) / max(n, 1)))
results.sort(key=lambda r: -r['score'])
out = PROJECT / 'Artifacts' / 'Art' / 'Voidreign' / 'anim-quality.json'
out.parent.mkdir(parents=True, exist_ok=True); out.write_text(json.dumps(results, indent=1))
summary = [(r['clip'], round(r['score'], 1), {g: v[0] for g, v in r['flips'].items()}, list(r['buckled_joints'])) for r in results[:15]]
unreal.log('ANIM_QUALITY ' + json.dumps(dict(clips=len(results), with_flips=sum(1 for r in results if r['flips']),
                                             with_buckles=sum(1 for r in results if r['buckled_joints']), worst=summary)))
