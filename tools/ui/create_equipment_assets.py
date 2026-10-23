"""Equipment window assets (Editor Python, UE 5.8). Re-runnable.

Headless: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file>" -unattended -nosplash
Editor open: py -3 tools/Run-InEditor.py tools/ui/create_equipment_assets.py

1. /Game/Frontier/UI/Equipment/RT_EquipmentPreview: the character preview target (500 x 700, RGBA16f) that
   ASFEquipmentPreviewStudio's scene capture draws into (SceneColorHDR: linear colour, alpha = 1 - coverage).
2. /Game/Frontier/UI/Equipment/M_EquipmentPreview: the UI material WBP_Equipment's Preview image shows: the target's colour,
   opacity = 1 - its alpha, so only the character covers preview_backdrop.
3. Every item definition under /Game/Frontier whose EquipmentSlot is the old 'tool' moves to 'MainHand' (the gathering tools).
Then run tools/ui/create_widgets.py (SF_UI_ONLY=WBP_EquipSlot,WBP_Equipment).
"""
import unreal

DIR = '/Game/Frontier/UI/Equipment'
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary

rt_path = DIR + '/RT_EquipmentPreview'
rt = lib.load_asset(rt_path) if lib.does_asset_exist(rt_path) else tools.create_asset('RT_EquipmentPreview', DIR, unreal.TextureRenderTarget2D, unreal.TextureRenderTargetFactoryNew())
rt.set_editor_property('size_x', 500)
rt.set_editor_property('size_y', 700)
rt.set_editor_property('render_target_format', unreal.TextureRenderTargetFormat.RTF_RGBA16F)
rt.set_editor_property('clear_color', unreal.LinearColor(0, 0, 0, 1))
lib.save_loaded_asset(rt, False)

m_path = DIR + '/M_EquipmentPreview'
if lib.does_asset_exist(m_path):
    lib.delete_asset(m_path)  # rebuilt from this script every run
m = tools.create_asset('M_EquipmentPreview', DIR, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
sample = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -500, 0)
sample.set_editor_property('texture', rt)
sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
cover = mel.create_material_expression(m, unreal.MaterialExpressionOneMinus, -200, 160)
mel.connect_material_expressions(sample, 'A', cover, '')
mel.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(cover, '', unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(m)
lib.save_loaded_asset(m, False)

moved = []
for path in lib.list_assets('/Game/Frontier', recursive=True, include_folder=False):
    data = unreal.EditorAssetLibrary.find_asset_data(path)
    if str(data.asset_class_path.asset_name) != 'InventoryItemDefinition':
        continue
    item = lib.load_asset(path)
    if str(item.get_editor_property('EquipmentSlot')) == 'tool':
        item.set_editor_property('EquipmentSlot', 'MainHand')
        lib.save_loaded_asset(item, False)
        moved.append(item.get_name())
unreal.log('EQUIPMENT_ASSETS rt=%s material=%s tools_to_mainhand=%s' % (rt_path, m_path, moved))
