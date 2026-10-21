"""Import the generated HUD kit and Cinzel font, and build the DA_HudKit data asset SFHudStyle reads.

Run after the editor/build includes SFHudKit and after tools/Process-HudKit.ps1:
    python tools/Run-InEditor.py tools/Import-HudKit.py
Textures land in /Game/Frontier/UI/Kit (T_Kit_<name>), the font in /Game/Frontier/UI/Fonts. 9-slice margins come from
GameClient/ArtSource/UI/HUD-Kit-v1/kit.json (pixels), stored normalised on DA_HudKit as Slate boxes expect.
"""
import json
import os

import unreal


ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'GameClient', 'ArtSource'))
KIT = os.path.join(ROOT, 'UI', 'HUD-Kit-v1')
FONT_FILE = os.path.join(ROOT, 'Fonts', 'Cinzel', 'Cinzel-Bold.ttf')
TEXTURE_PATH = '/Game/Frontier/UI/Kit'
FONT_PATH = '/Game/Frontier/UI/Fonts'

elements = json.load(open(os.path.join(KIT, 'kit.json')))['elements']
library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

tasks = []
for e in elements:
    source = os.path.join(KIT, 'processed', e['name'] + '.png')
    if not os.path.isfile(source):
        raise RuntimeError('Missing processed texture ' + source + '; run tools/Process-HudKit.ps1')
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', source)
    task.set_editor_property('destination_path', TEXTURE_PATH)
    task.set_editor_property('destination_name', 'T_Kit_' + e['name'])
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', False)
    tasks.append(task)
if os.path.isfile(FONT_FILE) and not os.environ.get('SF_SKIP_FONT'):  # font import needs Slate: run it in the open editor, not the headless commandlet
    font_task = unreal.AssetImportTask()
    font_task.set_editor_property('filename', FONT_FILE)
    font_task.set_editor_property('destination_path', FONT_PATH)
    font_task.set_editor_property('destination_name', 'FontFace_Cinzel')
    font_task.set_editor_property('replace_existing', True)
    font_task.set_editor_property('automated', True)
    font_task.set_editor_property('save', False)
    tasks.append(font_task)
tools.import_asset_tasks(tasks)

# Heading() in SFHudStyle loads this face by path; Python cannot build a composite UFont, and Slate accepts a bare UFontFace.
title_font = library.load_asset(FONT_PATH + '/FontFace_Cinzel')
if title_font is not None:
    library.save_loaded_asset(title_font)

kit_class = unreal.load_class(None, '/Script/ShatteredFrontier.SFHudKit')
if kit_class is None:
    raise RuntimeError('Build SFHudKit before importing the kit.')
kit_path = TEXTURE_PATH + '/DA_HudKit'
kit = library.load_asset(kit_path) if library.does_asset_exist(kit_path) else tools.create_asset('DA_HudKit', TEXTURE_PATH, kit_class, unreal.DataAssetFactory())
if kit is None:
    raise RuntimeError('Could not create ' + kit_path)

done = []
table = {}
for e in elements:
    texture = library.load_asset(TEXTURE_PATH + '/T_Kit_' + e['name'])
    if texture is None:
        raise RuntimeError('Import failed for ' + e['name'])
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('never_stream', True)
    library.save_loaded_asset(texture)
    left, top, right, bottom = e['margin']
    width, height = float(e['width']), float(e['height'])
    element = unreal.SFHudKitElement()
    element.set_editor_property('texture', texture)
    element.set_editor_property('margin', unreal.Margin(left / width, top / height, right / width, bottom / height))
    element.set_editor_property('inset_pixels', float(e.get('inset', 0)))
    table[e['name']] = element
    done.append(e['name'])
kit.set_editor_property('elements', table)
if not library.save_loaded_asset(kit):
    raise RuntimeError('Could not save ' + kit_path)

unreal.log('HUD_KIT_IMPORTED ' + ', '.join(done) + '; font=' + ('yes' if title_font else 'no'))
