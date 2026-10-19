"""In-engine check (Editor Python, headless): animation bone translations vs the skeleton reference pose.

    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -nullrhi -unattended
        -script="<project>/tools/Check-UnrealAnimScale.py --folder /Game/... --bone pelvis --out <json>"

For every AnimSequence in the folder, compares the bone's local translation at frame 0 with the reference
pose. A ratio near 1 means units/axes survived import; ~0.01 or ~100 means a unit-scale fault.
"""
import json, shlex
from pathlib import Path
import unreal

toks = shlex.split(unreal.SystemLibrary.get_command_line(), posix=False)
i = next(i for i, t in enumerate(toks) if 'Check-UnrealAnimScale.py' in t)
argv = [t.strip('"') for t in toks[i + 1:]]
arg = lambda k: argv[argv.index(k) + 1]
folder, bone, out = arg('--folder'), arg('--bone'), Path(arg('--out'))
rows = []
for path in unreal.EditorAssetLibrary.list_assets(folder, recursive=True):
    a = unreal.load_asset(path)
    if not isinstance(a, unreal.AnimSequence): continue
    ref = unreal.AnimPoseExtensions.get_ref_bone_pose(unreal.AnimPoseExtensions.get_reference_pose(a.get_editor_property('skeleton')), bone, unreal.AnimPoseSpaces.LOCAL).translation
    got = unreal.AnimationLibrary.get_bone_pose_for_frame(a, bone, 0, False).translation
    rl, gl = ref.length(), got.length()
    rows.append(dict(anim=a.get_name(), ref_cm=[ref.x, ref.y, ref.z], frame0_cm=[got.x, got.y, got.z], ratio=gl / rl if rl > 1e-6 else None,
                     frames=unreal.AnimationLibrary.get_num_frames(a), root_motion=a.get_editor_property('enable_root_motion')))
ratios = [r['ratio'] for r in rows if r['ratio']]
res = dict(folder=folder, bone=bone, count=len(rows), ratio_min=min(ratios) if ratios else None, ratio_max=max(ratios) if ratios else None, rows=rows)
out.parent.mkdir(parents=True, exist_ok=True); out.write_text(json.dumps(res, indent=1))
unreal.log('ANIM_SCALE ' + json.dumps({k: res[k] for k in ['folder', 'count', 'ratio_min', 'ratio_max']}))
