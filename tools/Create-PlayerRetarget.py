"""Create the IK rigs and retargeter that drive a player MetaHuman body from the UE4 mannequin (UE 5.8 editor Python).

UnrealEditor-Cmd.exe <uproject> -run=pythonscript -nullrhi -unattended -script="<this file>"
Needs a built face (tools/Build-PlayerMetaHumans.py); the MetaHuman rig is made on its body mesh, which shares
metahuman_base_skel with every face. Writes /Game/Frontier/Characters/Player/Retarget/{IK_SF_Mannequin, IK_SF_MetaHuman,
RTG_SF_MannequinToMetaHuman}; re-runnable (existing assets are rebuilt in place). ASFE1Character's hidden mannequin keeps
the gameplay animation; USFMetaHumanRetargetAnimInstance copies its pose onto the MetaHuman body every frame.
"""
import unreal

ROOT = '/Game/Frontier/Characters/Player/Retarget'
MANNEQUIN = '/Game/Mannequin/Character/Mesh/SK_Mannequin'
METAHUMAN = '/Game/Frontier/Characters/Player/Built/MHC_Orlando/Body/SKM_MHC_Orlando_BodyMesh'

library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def asset(name, cls, factory):
    path = f'{ROOT}/{name}'
    return library.load_asset(path) if library.does_asset_exist(path) else tools.create_asset(name, ROOT, cls, factory)


def ik_rig(name, mesh_path):
    mesh = library.load_asset(mesh_path)
    if mesh is None:
        raise RuntimeError(f'Missing {mesh_path}')
    rig = asset(name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    controller = unreal.IKRigController.get_controller(rig)
    controller.set_skeletal_mesh(mesh)
    for chain in [c.chain_name for c in controller.get_retarget_chains()]:
        controller.remove_retarget_chain(chain)
    controller.apply_auto_generated_retarget_definition()  # Epic's humanoid template: spine, neck, head, arms, legs, fingers
    controller.apply_auto_fbik()
    library.save_loaded_asset(rig)
    unreal.log(f'PLAYER_RETARGET_RIG {name} chains={len(controller.get_retarget_chains())}')
    return rig, mesh


source, source_mesh = ik_rig('IK_SF_Mannequin', MANNEQUIN)
target, target_mesh = ik_rig('IK_SF_MetaHuman', METAHUMAN)
retargeter = asset('RTG_SF_MannequinToMetaHuman', unreal.IKRetargeter, unreal.IKRetargetFactory())
controller = unreal.IKRetargeterController.get_controller(retargeter)
controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source)
controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target)
controller.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, source_mesh)
controller.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, target_mesh)
controller.remove_all_ops()
controller.add_default_ops()
controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, target)
controller.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
# The mannequin has no metacarpals; fuzzy mapping hands each finger's curl to its MetaHuman palm bone too (clawed hands).
for chain in [c.chain_name for c in unreal.IKRigController.get_controller(target).get_retarget_chains()]:
    if 'Metacarpal' in str(chain):
        controller.set_source_chain('None', chain)
    unreal.log(f'PLAYER_RETARGET_MAP {chain} <- {controller.get_source_chain(chain)}')
controller.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)  # both rest in an A-pose; align the limb directions
# Arms copy rotations only: the default IK op pins the MetaHuman's hands to the mannequin's hand positions, and the different
# arm lengths bend the elbows to reach them. Legs keep IK so the feet stay planted.
ik = controller.get_op_controller(controller.get_index_of_op_by_name('Run IK Rig'))
settings = ik.get_settings()
settings.chains = [c for c in settings.chains if 'Arm' not in str(c.target_chain_name)]
ik.set_settings(settings)
unreal.log(f'PLAYER_RETARGET_IK {[str(c.target_chain_name) for c in ik.get_settings().chains]}')
library.save_loaded_asset(retargeter)
unreal.log(f'PLAYER_RETARGET_READY ops={controller.get_num_retarget_ops()}')
