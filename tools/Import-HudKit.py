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
FONTS = {  # asset name: source file; Cinzel titles, EB Garamond text, Source Sans 3 numerals (OFL, static TTFs: variable fonts render as tofu)
    'FontFace_Cinzel': os.path.join(ROOT, 'Fonts', 'Cinzel', 'Cinzel-Bold.ttf'),
    'FontFace_EBGaramondRegular': os.path.join(ROOT, 'Fonts', 'EBGaramond', 'EBGaramond-Regular.ttf'),
    'FontFace_EBGaramondBold': os.path.join(ROOT, 'Fonts', 'EBGaramond', 'EBGaramond-Bold.ttf'),
    'FontFace_SourceSansRegular': os.path.join(ROOT, 'Fonts', 'SourceSans3', 'SourceSans3-Regular.ttf'),
    'FontFace_SourceSansBold': os.path.join(ROOT, 'Fonts', 'SourceSans3', 'SourceSans3-Bold.ttf'),
}
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
if not os.environ.get('SF_SKIP_FONT'):  # font import needs Slate: run it in the open editor, not the headless commandlet
    for font_name, font_file in FONTS.items():
        if not os.path.isfile(font_file):
            continue
        font_task = unreal.AssetImportTask()
        font_task.set_editor_property('filename', font_file)
        font_task.set_editor_property('destination_path', FONT_PATH)
        font_task.set_editor_property('destination_name', font_name)
        font_task.set_editor_property('replace_existing', True)
        font_task.set_editor_property('automated', True)
        font_task.set_editor_property('save', False)
        tasks.append(font_task)
tools.import_asset_tasks(tasks)

# Heading() in SFHudStyle loads this face by path; Python cannot build a composite UFont, and Slate accepts a bare UFontFace.
title_font = library.load_asset(FONT_PATH + '/FontFace_Cinzel')
for font_name in FONTS:
    face = library.load_asset(FONT_PATH + '/' + font_name)
    if face is not None:
        library.save_loaded_asset(face)

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

# Kit-v5 (ArtSource/UI/Kit-v5, authored at 2x the 1080p UI size): same T_Kit_<name> assets, overriding v4 pieces of the same name.
# The v4 slot/toast names the existing widgets use are aliased to their v5 equivalents.
V5 = os.path.join(ROOT, 'UI', 'Kit-v5')
ALIASES = {'slot_empty': 'slot_bag_normal', 'slot_selected': 'slot_bag_selected', 'toast_card': 'toast_frame'}
if os.path.isfile(os.path.join(V5, 'kit.json')):
    pieces = json.load(open(os.path.join(V5, 'kit.json')))['pieces']
    tasks_v5 = []
    for name, piece in pieces.items():
        source = os.path.join(V5, piece['file'])
        if not os.path.isfile(source):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', source)
        task.set_editor_property('destination_path', TEXTURE_PATH)
        task.set_editor_property('destination_name', 'T_Kit_' + name)
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('automated', True)
        task.set_editor_property('save', False)
        tasks_v5.append(task)
    tools.import_asset_tasks(tasks_v5)
    for name, piece in pieces.items():
        texture = library.load_asset(TEXTURE_PATH + '/T_Kit_' + name)
        if texture is None:
            continue
        texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('never_stream', True)
        library.save_loaded_asset(texture)
        width, height = float(texture.blueprint_get_size_x()), float(texture.blueprint_get_size_y())
        left, top, right, bottom = piece.get('nine_slice_px') or [0, 0, 0, 0]
        element = unreal.SFHudKitElement()
        element.set_editor_property('texture', texture)
        element.set_editor_property('margin', unreal.Margin(left / width, top / height, right / width, bottom / height))
        element.set_editor_property('inset_pixels', float(left) * 0.5)
        element.set_editor_property('draw_scale', 0.5)
        table[name] = element
        done.append('v5:' + name)
    for alias, target in ALIASES.items():
        if target in table:
            table[alias] = table[target]

# Kit-v5 scene art used by widgets: the paperdoll figure and the equipment-slot ghost icons (unsliced, drawn at their pixel size).
SCENES = os.path.join(V5, 'scenes')
if os.path.isdir(SCENES):
    scene_files = [f for f in sorted(os.listdir(SCENES)) if f.startswith(('ghost_', 'paperdoll_')) and f.endswith('.png')]
    scene_tasks = []
    for scene_file in scene_files:
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', os.path.join(SCENES, scene_file))
        task.set_editor_property('destination_path', TEXTURE_PATH)
        task.set_editor_property('destination_name', 'T_Kit_' + scene_file[:-4])
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('automated', True)
        task.set_editor_property('save', False)
        scene_tasks.append(task)
    tools.import_asset_tasks(scene_tasks)
    for scene_file in scene_files:
        name = scene_file[:-4]
        texture = library.load_asset(TEXTURE_PATH + '/T_Kit_' + name)
        if texture is None:
            continue
        texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('never_stream', True)
        library.save_loaded_asset(texture)
        element = unreal.SFHudKitElement()
        element.set_editor_property('texture', texture)
        table[name] = element
        done.append('scene:' + name)
kit.set_editor_property('elements', table)
if not library.save_loaded_asset(kit):
    raise RuntimeError('Could not save ' + kit_path)

unreal.log('HUD_KIT_IMPORTED ' + ', '.join(done) + '; font=' + ('yes' if title_font else 'no'))
