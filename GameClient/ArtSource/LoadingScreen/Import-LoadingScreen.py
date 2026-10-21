"""Import the Greenlands loading splash as /Game/Frontier/UI/LoadingScreen/T_Splash_Greenlands_Silverbrook.

Run in the open editor:  python tools/Run-InEditor.py GameClient/ArtSource/LoadingScreen/Import-LoadingScreen.py
UI texture settings: UI texture group, no mips, sRGB, never streamed. USFLoadingScreenSettings.SplashTexture points here.
"""
import os

import unreal

SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Greenlands-Silverbrook-Splash-v1.png')
DEST = '/Game/Frontier/UI/LoadingScreen'
NAME = 'T_Splash_Greenlands_Silverbrook'

task = unreal.AssetImportTask()
task.set_editor_property('filename', SOURCE)
task.set_editor_property('destination_path', DEST)
task.set_editor_property('destination_name', NAME)
task.set_editor_property('replace_existing', True)
task.set_editor_property('automated', True)
task.set_editor_property('save', False)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

texture = unreal.EditorAssetLibrary.load_asset(DEST + '/' + NAME)
if texture is None:
    raise RuntimeError('Import failed for ' + SOURCE)
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
texture.set_editor_property('srgb', True)
texture.set_editor_property('never_stream', True)
unreal.EditorAssetLibrary.save_loaded_asset(texture)
unreal.log('Imported ' + DEST + '/' + NAME)
