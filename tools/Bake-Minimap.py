"""Bake the HUD minimap: a top-down orthographic capture of Greenlands_Silverbrook.

Headless commandlet rendering captures black, so the capture runs in a -game client and the import in a commandlet:
  1. UnrealEditor.exe GameClient/ShatteredFrontier.uproject /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -windowed -RenderOffscreen -unattended -ResX=1280 -ResY=720 -nosound -ExecCmds="py tools/Bake-Minimap.py"
     (console command SFBakeMinimap, Private/UI/SFMinimapBake.cpp, writes GameClient/ArtSource/UI/Minimap/T_Minimap_Silverbrook.png, logs MINIMAP_BOUNDS, quits)
  2. SF_BAKE_PHASE=import UnrealEditor-Cmd.exe GameClient/ShatteredFrontier.uproject -run=pythonscript -script=tools/Bake-Minimap.py
     (imports the PNG as /Game/Frontier/UI/Minimap/T_Minimap_Silverbrook)
Bounds (cm, square) must match [/Script/ShatteredFrontier.SFHudMinimapWidget] MapBoundsCm in GameClient/Config/DefaultGame.ini.
"""
import os
import unreal as u

NAME = 'T_Minimap_Silverbrook'
OUT_DIR = '/Game/Frontier/UI/Minimap'
RES = 4096
HERE = os.path.dirname(os.path.abspath(__file__))
PNG_DIR = os.path.abspath(os.path.join(HERE, '..', 'GameClient', 'ArtSource', 'UI', 'Minimap'))


def import_png():
    task = u.AssetImportTask()
    task.filename = os.path.join(PNG_DIR, NAME + '.png')
    task.destination_path = OUT_DIR
    task.destination_name = NAME
    task.replace_existing = True
    task.automated = True
    task.save = True
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = u.EditorAssetLibrary.load_asset(OUT_DIR + '/' + NAME)
    tex.set_editor_property('srgb', True)
    tex.set_editor_property('mip_gen_settings', u.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    tex.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_UI)
    u.EditorAssetLibrary.save_asset(OUT_DIR + '/' + NAME, only_if_is_dirty=False)
    print('MINIMAP_IMPORTED', OUT_DIR + '/' + NAME)


if os.environ.get('SF_BAKE_PHASE') == 'import':
    import_png()
else:
    state = {'t': 0.0, 'done': False}

    def tick(delta):
        world = next((w for w in u.ObjectIterator(u.World) if u.GameplayStatics.get_player_controller(w, 0)), None)
        if not world:
            return
        state['t'] += delta
        if state['t'] > 8 and not state['done']:  # let lighting / streaming settle
            state['done'] = True
            try:
                u.SystemLibrary.execute_console_command(world, 'SFBakeMinimap %d' % RES)
            except Exception as ex:
                u.log_error('MINIMAP_ERROR %r' % (ex,))
        if state['t'] > 14:
            u.unregister_slate_post_tick_callback(state['handle'])
            u.SystemLibrary.execute_console_command(world, 'quit')
    state['handle'] = u.register_slate_post_tick_callback(tick)
