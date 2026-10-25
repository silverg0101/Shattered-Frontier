"""Create the single shared functional action-target rim; never edit item art.

Run through Run-InEditor.py. Existing owner-authored material is preserved.
"""
import unreal as u

path = '/Game/Frontier/Gameplay/Interaction/M_ActionTargetRim'
material = u.load_asset(path)
if not material:
    material = u.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_ActionTargetRim', '/Game/Frontier/Gameplay/Interaction', u.Material, u.MaterialFactoryNew())
    material.set_editor_property('blend_mode', u.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    fresnel = u.MaterialEditingLibrary.create_material_expression(material, u.MaterialExpressionFresnel)
    fresnel.set_editor_property('exponent', 5.0)
    fresnel.set_editor_property('base_reflect_fraction', 0.0)
    color = u.MaterialEditingLibrary.create_material_expression(material, u.MaterialExpressionConstant3Vector)
    color.set_editor_property('constant', u.LinearColor(0.03, 0.05, 0.06, 1))
    multiply = u.MaterialEditingLibrary.create_material_expression(material, u.MaterialExpressionMultiply)
    u.MaterialEditingLibrary.connect_material_expressions(fresnel, '', multiply, 'A')
    u.MaterialEditingLibrary.connect_material_expressions(color, '', multiply, 'B')
    u.MaterialEditingLibrary.connect_material_property(multiply, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    u.MaterialEditingLibrary.connect_material_property(fresnel, '', u.MaterialProperty.MP_OPACITY)
    u.MaterialEditingLibrary.recompile_material(material)
    u.EditorAssetLibrary.save_loaded_asset(material)
print('ACTION_TARGET_MATERIAL_READY', material.get_path_name())
