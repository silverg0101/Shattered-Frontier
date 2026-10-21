"""Import the v5 loading-screen art into /Game/Frontier/UI/LoadingScreen/.

Run after tools/Process-HudKit.ps1 -Kit GameClient/ArtSource/UI/LoadingScreen-v5 (writes processed/):
    python tools/Run-InEditor.py GameClient/ArtSource/UI/LoadingScreen-v5/Import-LoadingScreen-v5.py
T_LS_Background comes from source/ls_background.png (opaque); T_LS_<Name> from processed/ls_<name>.png (alpha). UI texture
group, no mips, sRGB, never streamed.
"""
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
DEST = '/Game/Frontier/UI/LoadingScreen'
FILES = {
    'T_LS_Background': 'source/ls_background.png',
    'T_LS_Emblem': 'processed/ls_emblem.png',
    'T_LS_Rule': 'processed/ls_rule.png',
    'T_LS_BarFrame': 'processed/ls_bar_frame.png',
    'T_LS_BarFill': 'processed/ls_bar_fill.png',
    'T_LS_Spinner': 'processed/ls_spinner.png',
}

tasks = []
for name, rel in FILES.items():
    path = os.path.join(HERE, rel)
    if not os.path.isfile(path):
        raise RuntimeError('Missing ' + path)
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', path)
    t.set_editor_property('destination_path', DEST)
    t.set_editor_property('destination_name', name)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('automated', True)
    t.set_editor_property('save', False)
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for name in FILES:
    texture = unreal.EditorAssetLibrary.load_asset(DEST + '/' + name)
    if texture is None:
        raise RuntimeError('Import failed for ' + name)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('never_stream', True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    unreal.log('Imported ' + DEST + '/' + name)
