"""Import the Inventory-v6 parts (GameClient/ArtSource/UI/Inventory-v6) as UI textures.

Run in the open editor:   py -3 tools/Run-InEditor.py tools/ui/import_inventory.py
Or headless:   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -unattended -nosplash -NullRHI

Writes /Game/Frontier/UI/Kit/Inventory/T_Inventory_<part> (UI texture group, no mips, never streamed; progress_fill_mask is linear
grayscale). Rerun replaces them in place. Draw-As-Box margins and sizes are brush settings of the widgets, set by
UI/SFWidgetAuthoring.cpp (InventoryLayout) from the README table. Then: tools/ui/create_widgets.py (creates WBP_InventoryTab and
WBP_InventorySlot) and the same with SF_UI_FORCE=1 SF_UI_ONLY=WBP_Inventory to rebuild the panel layout.
"""
import os

import unreal

SRC = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'GameClient', 'ArtSource', 'UI', 'Inventory-v6'))
DEST = '/Game/Frontier/UI/Kit/Inventory'
PARTS = [
    'panel_frame', 'header_divider', 'crest_stag', 'tab_bar_back', 'tab_normal', 'tab_selected', 'tab_divider',
    'icon_tab_materials', 'icon_tab_gear', 'icon_tab_consumables', 'icon_tab_misc',
    'slot_normal', 'slot_hover', 'slot_selected', 'tooltip_frame', 'tooltip_divider', 'icon_weight',
    'progress_track', 'progress_fill_green', 'progress_fill_mask', 'scrollbar_track', 'scrollbar_thumb',
]

lib = unreal.EditorAssetLibrary

tasks = []
for part in PARTS:
    src = os.path.join(SRC, part + '.png')
    if not os.path.isfile(src):
        raise RuntimeError('missing source ' + src)
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', src)
    t.set_editor_property('destination_path', DEST)
    t.set_editor_property('destination_name', 'T_Inventory_' + part)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('automated', True)
    t.set_editor_property('save', False)
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for part in PARTS:
    path = '%s/T_Inventory_%s' % (DEST, part)
    tx = lib.load_asset(path)
    if tx is None:
        raise RuntimeError('import failed: ' + path)
    mask = part == 'progress_fill_mask'
    tx.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    tx.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tx.set_editor_property('srgb', not mask)
    tx.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE if mask else unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tx.set_editor_property('never_stream', True)
    lib.save_loaded_asset(tx)
print('import_inventory: %d textures under %s' % (len(PARTS), DEST))
