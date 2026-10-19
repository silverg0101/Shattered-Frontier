"""Dump a skeletal mesh's reference skeleton (names, parents, local/world ref transforms in cm) to JSON.
Headless-safe (no render device), unlike FBX export of skeletal meshes.

    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -nullrhi -unattended
        -script="<project>/tools/Dump-UnrealSkeleton.py --mesh /Game/Mannequin/Character/Mesh/SK_Mannequin --out <file.json>"
"""
import json, shlex
from pathlib import Path
import unreal

toks = shlex.split(unreal.SystemLibrary.get_command_line(), posix=False)
i = next(i for i, t in enumerate(toks) if 'Dump-UnrealSkeleton.py' in t)
argv = [t.strip('"') for t in toks[i + 1:]]
mesh_path, out = argv[argv.index('--mesh') + 1], Path(argv[argv.index('--out') + 1])
mesh = unreal.load_asset(mesh_path)
skel = mesh.get_editor_property('skeleton')
pose = unreal.AnimPoseExtensions.get_reference_pose(skel)
comp = unreal.SkeletalMeshComponent(); comp.set_skinned_asset_and_update(mesh)


def tr(t):
    q = t.rotation
    return dict(t=[t.translation.x, t.translation.y, t.translation.z], q=[q.w, q.x, q.y, q.z], s=[t.scale3d.x, t.scale3d.y, t.scale3d.z])


bones = []
for name in unreal.AnimPoseExtensions.get_bone_names(pose):
    parent = comp.get_parent_bone(name)
    bones.append(dict(name=str(name), parent=str(parent) if str(parent) != 'None' else None,
                      local=tr(unreal.AnimPoseExtensions.get_ref_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)),
                      world=tr(unreal.AnimPoseExtensions.get_ref_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD))))
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(dict(schema_version=1, mesh=mesh_path, skeleton=skel.get_path_name(), units='cm, Unreal left-handed Z-up', bones=bones), indent=1))
unreal.log(f'SKELETON_DUMPED {len(bones)} {out}')
