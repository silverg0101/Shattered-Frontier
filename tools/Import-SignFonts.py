"""Build offline distance-field Font assets for in-world sign text (TextRender).

Run inside the open editor: python tools/Run-InEditor.py tools/Import-SignFonts.py
Offline fonts render from Windows-installed families, so install the files in
GameClient/ArtSource/Fonts/ for the current user first. Settings match /Engine/EngineFonts/RobotoDistanceField.
"""
import unreal

DEST = '/Game/Frontier/Core/Fonts'
FAMILIES = {  # asset name: Windows family name
    'Font_Blackletter': 'Blackletter',
    'Font_BlackletterShadow': 'Blackletter Shadow',
    'Font_Grimwalde': 'Grimwalde',
    'Font_TheWayler': 'The Wayler Demo',
    'Font_TCBlackcat': 'TC Blackcat',
    'Font_WyndhamGothic': 'Wyndham Gothic',
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
for name, family in FAMILIES.items():
    path = f'{DEST}/{name}'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    opts = unreal.FontImportOptionsData()
    for k, v in {'font_name': family, 'height': 42.0, 'use_distance_field_alpha': True,
                 'distance_field_scale_factor': 16, 'texture_page_width': 512,
                 'texture_page_max_height': 256, 'x_padding': 5, 'y_padding': 5}.items():
        opts.set_editor_property(k, v)
    factory = unreal.TrueTypeFontFactory()
    holder = factory.get_editor_property('import_options') or unreal.FontImportOptions()
    holder.set_editor_property('data', opts)
    factory.set_editor_property('import_options', holder)
    font = tools.create_asset(name, DEST, unreal.Font, factory)
    if not font:
        raise RuntimeError(f'failed to create {path}')
    font.set_editor_property('font_cache_type', unreal.FontCacheType.OFFLINE)
    unreal.EditorAssetLibrary.save_loaded_asset(font)
    unreal.log(f'{path}: {family}')
