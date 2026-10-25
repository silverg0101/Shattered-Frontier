"""Generate only the versioned shared postprocess outline, never item assets.

Run through Run-InEditor.py or the Python commandlet after source review.
The historical overlay material is left intact; runtime no longer uses it.
"""
import sys
from pathlib import Path
import unreal as u

tools = Path(__file__).resolve().parent
sys.path.insert(0, str(tools))
from action_target_outline import load_settings, shader

settings = load_settings(tools.parent / 'content/interaction/selection-render.json')
folder = '/Game/Frontier/Gameplay/Interaction'
path = folder + '/M_ActionTargetOutline'
material = u.load_asset(path)
tag = 'SF_GeneratedActionOutline'
if material and u.EditorAssetLibrary.get_metadata_tag(material, tag) != '1':
    raise RuntimeError('Refusing to replace an unowned selection material')
if not material:
    material = u.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_ActionTargetOutline', folder, u.Material, u.MaterialFactoryNew())
u.EditorAssetLibrary.set_metadata_tag(material, tag, '1')
u.MaterialEditingLibrary.delete_all_material_expressions(material)
material.set_editor_property('material_domain', u.MaterialDomain.MD_POST_PROCESS)
material.set_editor_property('blendable_location', u.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
custom = u.MaterialEditingLibrary.create_material_expression(material, u.MaterialExpressionCustom)
custom.set_editor_property('description', 'Shared visible selection boundary v1')
custom.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT3)
custom.set_editor_property('code', shader(settings))
sources = [('SceneColor', u.SceneTextureId.PPI_POST_PROCESS_INPUT0),
           ('StencilCenter', u.SceneTextureId.PPI_CUSTOM_STENCIL),
           ('CustomDepthCenter', u.SceneTextureId.PPI_CUSTOM_DEPTH),
           ('SceneDepthCenter', u.SceneTextureId.PPI_SCENE_DEPTH)]
inputs = []
for name, _ in sources:
    entry = u.CustomInput(); entry.set_editor_property('input_name', name); inputs.append(entry)
custom.set_editor_property('inputs', inputs)
for name, texture_id in sources:
    texture = u.MaterialEditingLibrary.create_material_expression(material, u.MaterialExpressionSceneTexture)
    texture.set_editor_property('scene_texture_id', texture_id)
    if not u.MaterialEditingLibrary.connect_material_expressions(texture, 'Color', custom, name):
        raise RuntimeError('Selection scene-texture connection failed: ' + name)
if not u.MaterialEditingLibrary.connect_material_property(custom, '', u.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError('Selection output connection failed')
u.MaterialEditingLibrary.recompile_material(material)
if not u.EditorAssetLibrary.save_loaded_asset(material):
    raise RuntimeError('Selection material save failed')
print('ACTION_TARGET_OUTLINE_READY', material.get_path_name(), 'StencilBit', settings.stencil_bit)
