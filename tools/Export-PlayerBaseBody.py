"""Export a built player MetaHuman body (mesh + metahuman_base_skel) to FBX for garment authoring in Blender.

UnrealEditor-Cmd.exe <uproject> -run=pythonscript -nullrhi -unattended -script="<this file>"
Env: SF_BODY_FACE (default Orlando) picks /Game/Frontier/Characters/Player/Built/MHC_<face>/Body/SKM_MHC_<face>_BodyMesh;
SF_BODY_OUT (default Artifacts/Characters/PlayerBase/<face>_body.fbx, git-ignored: it is regenerated from the build).
Also writes <face>_head.obj beside it first (the face mesh LOD0 in Unreal coordinates via Geometry Script,
re-skinned onto the gender's bare build: the body every face wears in game), for
tools/Build-PlayerGarment.py --head: the head mesh carries the neck and a chest bib the tunic must cover. SF_HEAD_ONLY=1 skips the FBX.
"""
import json
import os
from pathlib import Path

import unreal

face = os.environ.get('SF_BODY_FACE', 'Orlando')
root = Path(unreal.Paths.project_dir()).parent
out = Path(os.environ.get('SF_BODY_OUT', root / f'Artifacts/Characters/PlayerBase/{face}_body.fbx'))
out.parent.mkdir(parents=True, exist_ok=True)
pick = lambda r, t: next(x for x in r if isinstance(x, t)) if isinstance(r, tuple) else r
L = unreal.GeometryScript_List
B = unreal.GeometryScript_BoneWeights


def face_dm(name):
    mesh = unreal.EditorAssetLibrary.load_asset(f'/Game/Frontier/Characters/Player/Built/MHC_{name}/Face/SKM_MHC_{name}_FaceMesh')
    dm = unreal.DynamicMesh()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(mesh, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    return dm, {b.name: b.world_transform for b in pick(B.get_all_bones_info(dm), unreal.Array)}


# In game every face is leader-posed onto its gender's bare build (the Mutable outfit body), so its head is re-skinned from its own
# bind pose into that build's: v' = sum w * Bare(bone) * Face(bone)^-1 * v. Raw positions differ by up to ~25 cm between faces.
gender = next(f['gender'] for f in json.load(open(root / 'GameClient/ArtSource/Characters/Player/faces.json'))['faces'] if f['id'] == face)
frame = 'HildaBare' if gender == 'female' else 'OrlandoBare'
dm, bind = face_dm(face)
_, bare = face_dm(frame) if frame != face else (None, bind)
names = [b.name for b in pick(B.get_all_bones_info(dm), unreal.Array)]
points = L.convert_vector_list_to_array(pick(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(dm, False), unreal.GeometryScriptVectorList))
if frame != face:
    moved = []
    for vid, p in enumerate(points):
        q = unreal.Vector(0, 0, 0)
        for w in pick(B.get_vertex_bone_weights(dm, vid), unreal.Array):
            n = names[w.bone_index]
            q += bare[n].transform_location(bind[n].inverse_transform_location(p)) * w.weight
        moved.append(q)
    points = moved
tris = L.convert_triangle_list_to_array(pick(unreal.GeometryScript_MeshQueries.get_all_triangle_indices(dm, False), unreal.GeometryScriptTriangleList))
head_out = out.with_name(f'{face}_head.obj')
with open(head_out, 'w') as fh:
    fh.writelines(f'v {p.x:.4f} {p.y:.4f} {p.z:.4f}\n' for p in points)
    fh.writelines(f'f {t.x + 1} {t.y + 1} {t.z + 1}\n' for t in tris)
unreal.log(f'PLAYER_HEAD_EXPORTED {head_out} ({len(points)} verts)')

if os.environ.get('SF_HEAD_ONLY'):  # the body FBX export asserts (MeshObject) in this 5.8 build; keep the FBX already on disk
    raise SystemExit(0)
mesh = unreal.EditorAssetLibrary.load_asset(f'/Game/Frontier/Characters/Player/Built/MHC_{face}/Body/SKM_MHC_{face}_BodyMesh')
if mesh is None:
    raise RuntimeError(f'No built body for {face}: run tools/Build-PlayerMetaHumans.py')
options = unreal.FbxExportOption()
options.set_editor_property('collision', False)
options.set_editor_property('level_of_detail', False)  # LOD0 only: the garment is fitted to the full-resolution body
options.set_editor_property('export_morph_targets', True)
task = unreal.AssetExportTask()
task.object = mesh
task.filename = str(out)
task.automated = True
task.replace_identical = True
task.prompt = False
task.options = options
task.exporter = unreal.SkeletalMeshExporterFBX()
if not unreal.Exporter.run_asset_export_task(task):
    raise RuntimeError(f'FBX export failed for {face}')
unreal.log(f'PLAYER_BODY_EXPORTED {out} ({out.stat().st_size // 1024} KB)')
