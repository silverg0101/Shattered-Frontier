"""Import the gameplay item icon packet and point each pilot item definition at its icon.

Run in the open editor: python tools/Run-InEditor.py tools/Import-ItemIcons.py
Reads GameClient/ArtSource/UI/ItemIcons-v1/manifest.json (item id -> 64/256 px PNGs). The 256 px art is imported as
/Game/Frontier/UI/Icons/T_Icon_<stem> (replacing earlier generations under the same name); a definition at
/Game/Frontier/Gameplay/Pilot/DA_<CamelCase> gets its Icon set when it exists. Items without a definition asset still get a texture.
"""
import json
import os

import unreal


PACKET = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'GameClient', 'ArtSource', 'UI', 'ItemIcons-v1'))
ICON_PATH = '/Game/Frontier/UI/Icons'
ITEM_PATH = '/Game/Frontier/Gameplay/Pilot'

manifest = json.load(open(os.path.join(PACKET, 'manifest.json')))['items']
library = unreal.EditorAssetLibrary

tasks = []
stems = {}
for item_id, files in manifest.items():
    stem = item_id.split('/')[-1]
    source = os.path.join(PACKET, files['256'])
    if not os.path.isfile(source):
        raise RuntimeError('Missing icon ' + source)
    stems[item_id] = stem
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', source)
    task.set_editor_property('destination_path', ICON_PATH)
    task.set_editor_property('destination_name', 'T_Icon_' + stem)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', False)
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

done, linked = [], []
for item_id, stem in stems.items():
    texture = library.load_asset(ICON_PATH + '/T_Icon_' + stem)
    if texture is None:
        raise RuntimeError('Import failed for ' + item_id)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('never_stream', True)
    library.save_loaded_asset(texture)
    done.append(stem)
    definition_path = ITEM_PATH + '/DA_' + ''.join(part.capitalize() for part in stem.split('_'))
    if library.does_asset_exist(definition_path):
        definition = library.load_asset(definition_path)
        definition.set_editor_property('icon', texture)
        if not library.save_loaded_asset(definition):
            raise RuntimeError('Could not save ' + definition_path)
        linked.append(stem)

unreal.log('ITEM_ICONS_IMPORTED ' + ', '.join(done) + '; linked ' + ', '.join(linked))
