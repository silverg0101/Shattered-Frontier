"""Export a built player MetaHuman body (mesh + metahuman_base_skel) to FBX for garment authoring in Blender.

UnrealEditor-Cmd.exe <uproject> -run=pythonscript -nullrhi -unattended -script="<this file>"
Env: SF_BODY_FACE (default Orlando) picks /Game/Frontier/Characters/Player/Built/MHC_<face>/Body/SKM_MHC_<face>_BodyMesh;
SF_BODY_OUT (default Artifacts/Characters/PlayerBase/<face>_body.fbx, git-ignored: it is regenerated from the build).
"""
import os
from pathlib import Path

import unreal

face = os.environ.get('SF_BODY_FACE', 'Orlando')
root = Path(unreal.Paths.project_dir()).parent
out = Path(os.environ.get('SF_BODY_OUT', root / f'Artifacts/Characters/PlayerBase/{face}_body.fbx'))
out.parent.mkdir(parents=True, exist_ok=True)
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
