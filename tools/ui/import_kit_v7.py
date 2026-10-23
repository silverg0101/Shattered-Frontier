"""Import the shared menu kit v7 parts (GameClient/ArtSource/UI/Kit-v7, 60 PNGs) as UI textures.

Run in the open editor:   py -3 tools/Run-InEditor.py tools/ui/import_kit_v7.py
Or headless:   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -unattended -nosplash -NullRHI

Writes /Game/Frontier/UI/Kit/v7/T_KitV7_<part> (UI texture group, no mips, never streamed). Rerun replaces them in place.
The parts are 2x art. Scale convention (docs/dependency-map/ui.md): kit widgets are authored in ART PIXELS and a window's root
ScaleBox draws them at 0.5, because Slate measures Box 9-slice margins in texture pixels, never in brush ImageSize.
Margins and sizes come from the README table; UI/SFWidgetAuthoring.cpp (KitBrush) holds them. Then run tools/ui/create_widgets.py.
"""
import glob
import os

import unreal

SRC = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'GameClient', 'ArtSource', 'UI', 'Kit-v7'))
DEST = '/Game/Frontier/UI/Kit/v7'
PARTS = sorted(os.path.splitext(os.path.basename(p))[0] for p in glob.glob(os.path.join(SRC, '*.png')) if not os.path.basename(p).startswith('reference-'))
if len(PARTS) < 60:
    raise RuntimeError('expected at least 60 Kit-v7 parts, found %d' % len(PARTS))

lib = unreal.EditorAssetLibrary
tasks = []
for part in PARTS:
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', os.path.join(SRC, part + '.png'))
    t.set_editor_property('destination_path', DEST)
    t.set_editor_property('destination_name', 'T_KitV7_' + part)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('automated', True)
    t.set_editor_property('save', False)
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for part in PARTS:
    path = '%s/T_KitV7_%s' % (DEST, part)
    tx = lib.load_asset(path)
    if tx is None:
        raise RuntimeError('import failed: ' + path)
    tx.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    tx.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tx.set_editor_property('srgb', True)
    tx.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tx.set_editor_property('never_stream', True)
    lib.save_loaded_asset(tx)
print('import_kit_v7: %d textures under %s' % (len(PARTS), DEST))
