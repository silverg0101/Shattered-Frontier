"""Generate and assign the Voidreign physics asset (Editor Python, headless-safe). The FBX import's
create_physics_asset option is ignored by the 5.8 Interchange path, so run this after Import-Voidreign.py.

    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -nullrhi -unattended -script="<project>/tools/Create-VoidreignPhysics.py"
"""
import unreal

MESH = '/Game/Frontier/Creatures/Voidreign/V1/Mesh/SK_Voidreign'
mesh = unreal.load_asset(MESH)
if mesh.get_editor_property('physics_asset') is not None:
    unreal.log('PHYS_CREATE exists ' + mesh.get_editor_property('physics_asset').get_path_name())
else:
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    phys = sub.create_physics_asset(mesh, True)  # engine defaults: capsules per sizeable bone, assigned to the mesh
    if phys is None: raise RuntimeError('Physics asset creation failed')
    unreal.EditorAssetLibrary.save_loaded_asset(phys, only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
    unreal.log('PHYS_CREATE created ' + phys.get_path_name())
