"""Import the authored status-row skull; run in the editor after rendering skull.svg to skull.png."""
import os
import unreal

source = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../GameClient/ArtSource/UI/Status-v1/skull.png"))
task = unreal.AssetImportTask()
task.filename = source
task.destination_path = "/Game/Frontier/UI/Status"
task.destination_name = "T_Status_Skull"
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.EditorAssetLibrary.load_asset("/Game/Frontier/UI/Status/T_Status_Skull")
if not texture:
    raise RuntimeError("status skull import failed")
texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
unreal.EditorAssetLibrary.save_loaded_asset(texture)
