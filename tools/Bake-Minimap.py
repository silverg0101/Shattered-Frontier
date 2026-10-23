"""Bake the HUD minimap: a top-down orthographic capture of Greenlands_Silverbrook.

Headless commandlet rendering captures black, so the capture runs in a -game client and the import in a commandlet:
  1. UnrealEditor.exe GameClient/ShatteredFrontier.uproject /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -windowed -ResX=1280 -ResY=720 -nosound -ExecCmds="py tools/Bake-Minimap.py"
     (writes docs/evidence/minimap/T_Minimap_Silverbrook.png, prints MINIMAP_BOUNDS, quits)
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
PNG_DIR = os.path.abspath(os.path.join(HERE, '..', 'docs', 'evidence', 'minimap'))


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


def capture(world):
    # The ground is one big StaticMeshActor (half-extent 1250 m); sky / far-water meshes (> 2 km half-extent) are hidden.
    lo = [1e12, 1e12]; hi = [-1e12, -1e12]; z_top = -1e12
    for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor):
        c, e = a.get_actor_bounds(False)
        if max(e.x, e.y) > 200000:
            a.set_actor_hidden_in_game(True)
        elif max(e.x, e.y) > 100000:
            lo = [min(lo[0], c.x - e.x), min(lo[1], c.y - e.y)]; hi = [max(hi[0], c.x + e.x), max(hi[1], c.y + e.y)]
            z_top = max(z_top, c.z + e.z)
    side = max(hi[0] - lo[0], hi[1] - lo[1])  # square: 1:1 in X and Y
    cx = (lo[0] + hi[0]) / 2; cy = (lo[1] + hi[1]) / 2
    print('MINIMAP_BOUNDS %.0f %.0f %.0f %.0f' % (cx - side / 2, cy - side / 2, cx + side / 2, cy + side / 2))
    rt = u.RenderingLibrary.create_render_target2d(world, RES, RES, u.TextureRenderTargetFormat.RTF_RGBA8)
    host = u.GameplayStatics.get_all_actors_of_class(world, u.PlayerStart)[0]  # no spawn API in a game world: borrow an actor for the capture component
    xf = u.Transform(u.Vector(cx, cy, z_top + 20000.0), u.Rotator(roll=0.0, pitch=-90.0, yaw=0.0), u.Vector(1, 1, 1))
    comp = u.new_object(u.SceneCaptureComponent2D, outer=host)
    comp.register_component()
    comp.set_world_location_and_rotation(xf.translation, xf.rotation.rotator(), False, False)
    comp.projection_type = u.CameraProjectionMode.ORTHOGRAPHIC
    comp.ortho_width = side
    comp.texture_target = rt
    comp.capture_source = u.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    comp.capture_every_frame = False
    comp.capture_on_movement = False
    comp.always_persist_rendering_state = True
    # Readable map: no fog / bloom / vignette / sky haze.
    comp.show_flag_settings = [u.EngineShowFlagsSetting(show_flag_name=f, enabled=False)
                               for f in ('Fog', 'AtmosphericFog', 'VolumetricFog', 'Bloom', 'Vignette', 'MotionBlur', 'LensFlares', 'Atmosphere', 'Cloud')]
    comp.capture_scene()
    os.makedirs(PNG_DIR, exist_ok=True)
    u.RenderingLibrary.export_render_target(world, rt, PNG_DIR, NAME + '.png')
    print('MINIMAP_PNG', os.path.join(PNG_DIR, NAME + '.png'))


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
                capture(world)
            except Exception as ex:
                u.log_error('MINIMAP_ERROR %r' % (ex,))
        if state['t'] > 14:
            u.unregister_slate_post_tick_callback(state['handle'])
            u.SystemLibrary.execute_console_command(world, 'quit')
    state['handle'] = u.register_slate_post_tick_callback(tick)
