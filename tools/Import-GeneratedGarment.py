"""GEAR-02: import a Clean-GeneratedGarment.py result as a skeletal mesh on the player's MetaHuman skeleton, with its baked texture.
In the editor: python tools/Run-InEditor.py tools/Import-GeneratedGarment.py [name=Hood]
Reads Artifacts/Characters/PlayerBase/<name lower>/{<name lower>.fbx, T_<name>_BaseColor.png}; writes under /Game/Frontier/Characters/Player/OutfitAsset/<name>:
T_<name>_BaseColor, MI_<name> (instance of Garments/M_PlayerCloth, Tiling 1: the bake is not a tiling cloth), SKM_<name>.
"""
import sys
from pathlib import Path

import unreal

NAME = next((a.split('=', 1)[1] for a in sys.argv[1:] if a.startswith('name=')), 'Hood')
DIR = f'/Game/Frontier/Characters/Player/OutfitAsset/{NAME}'
SRC = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent / f'Artifacts/Characters/PlayerBase/{NAME.lower()}'
L = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
BODY = '/Game/Frontier/Characters/Player/Built/MHC_Orlando/Body/SKM_MHC_Orlando_BodyMesh'


def task(path, name, options=None):
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', str(path)); t.set_editor_property('destination_path', DIR)
    t.set_editor_property('destination_name', name); t.set_editor_property('replace_existing', True)
    t.set_editor_property('automated', True); t.set_editor_property('save', False)
    if options:
        t.set_editor_property('options', options)
    TOOLS.import_asset_tasks([t])
    asset = L.load_asset(f'{DIR}/{name}')
    if asset is None:
        raise RuntimeError(f'{name} did not import')
    return asset


tex = task(SRC / f'T_{NAME}_BaseColor.png', f'T_{NAME}_BaseColor')
tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
mel = unreal.MaterialEditingLibrary
mi_path = f'{DIR}/MI_{NAME}'
mi = L.load_asset(mi_path) if L.does_asset_exist(mi_path) else TOOLS.create_asset(f'MI_{NAME}', DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mi.set_editor_property('parent', L.load_asset('/Game/Frontier/Characters/Player/Garments/M_PlayerCloth'))
mel.set_material_instance_texture_parameter_value(mi, 'BaseTex', tex)
mel.set_material_instance_scalar_parameter_value(mi, 'Tiling', 1.0)
mel.set_material_instance_scalar_parameter_value(mi, 'NormalStrength', 0.0)
mel.set_material_instance_scalar_parameter_value(mi, 'Roughness', 0.9)
ui = unreal.FbxImportUI()
for k, v in (('import_mesh', True), ('import_as_skeletal', True), ('import_materials', False), ('import_textures', False),
             ('import_animations', False), ('skeleton', L.load_asset(BODY).get_editor_property('skeleton'))):
    ui.set_editor_property(k, v)
data = ui.get_editor_property('skeletal_mesh_import_data')
data.set_editor_property('import_morph_targets', False); data.set_editor_property('update_skeleton_reference_pose', False)
mesh = task(SRC / f'{NAME.lower()}.fbx', f'SKM_{NAME}', ui)
slots = [unreal.SkeletalMaterial(material_interface=mi, material_slot_name=s.get_editor_property('material_slot_name'))  # new structs: editing the slots in place did not stick
         for s in mesh.get_editor_property('materials')]
mesh.modify()
mesh.set_editor_property('materials', slots)
for a in (tex, mi, mesh):
    L.save_loaded_asset(a, only_if_is_dirty=False)  # a material-slot edit does not always mark the mesh dirty: without this it saved WorldGridMaterial
if any(s.get_editor_property('material_interface') != mi for s in L.load_asset(f'{DIR}/SKM_{NAME}').get_editor_property('materials')):
    raise RuntimeError(f'SKM_{NAME} material slots did not take MI_{NAME}')
unreal.log(f'texture {tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()}')
unreal.log(f'GENERATED_GARMENT_IMPORTED {mesh.get_path_name()} slots={len(slots)} extent={mesh.get_bounds().box_extent}')
