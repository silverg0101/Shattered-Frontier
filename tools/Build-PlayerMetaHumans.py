"""Build the player MetaHumans from MetaHuman Creator presets (UE 5.8 editor Python).

MetaHuman assembly bakes textures with Texture Graph, which needs the full editor with rendering (a -nullrhi or commandlet run
fails with "Couldn't find baked texture"). Run it from PowerShell:
  UnrealEditor.exe <uproject> -RenderOffscreen -unattended -nosplash -ExecCmds="py <abs path of this file>"
It starts on the editor's first tick, builds, then quits the editor.

Reads GameClient/ArtSource/Characters/Player/faces.json ({"faces": [{"id", "preset", "gender", "hair"}]}).
Each face's preset is copied to /Game/Frontier/Characters/Player/Faces/MHC_<id> and given its hair (a WI_Hair_* wardrobe
item). MetaHuman's cloud auto-rig and texture download run only when the copy cannot build yet (they use the editor's
signed-in Epic account). The optimized, medium-quality assembly goes to /Game/Frontier/Characters/Player/Built/MHC_<id>,
with shared assets in .../Built/Common. Env SF_FACES=<id,id> limits the run. Logs PLAYER_METAHUMAN_BUILT / _FAILED.
"""
import json
import os
from pathlib import Path

import unreal

ROOT = '/Game/Frontier/Characters/Player'
PRESETS = '/MetaHumanCharacter/Optional/Presets'
HAIR = '/MetaHumanCharacter/Optional/Grooms/Bindings/Hair'


def build_face(face, subsystem, library):
    path = f"{ROOT}/Faces/MHC_{face['id']}"
    fresh = not library.does_asset_exist(path)
    character = library.duplicate_asset(f"{PRESETS}/{face['preset']}", path) if fresh else library.load_asset(path)
    if character is None:
        raise RuntimeError(f"Could not copy preset {face['preset']} to {path}")
    instance = character.internal_collection.default_instance
    if fresh:
        # The face's own grooms: its hair replaces the preset's, and facial hair is cleared unless the face lists it.
        for slot in ('Beard', 'Mustache'):
            if slot in [str(s) for s in character.internal_collection.get_slot_names()] and not face.get(slot.lower()):
                instance.set_single_slot_selection(slot, unreal.MetaHumanPaletteItemKey())
        if face.get('hair'):
            item = library.load_asset(f"{HAIR}/{face['hair']}")
            key = character.internal_collection.try_add_item_from_wardrobe_item('Hair', item)
            instance.set_single_slot_selection('Hair', key)
    if not subsystem.try_add_object_to_edit(character=character):
        raise RuntimeError(f'{path} is open for edit elsewhere')
    try:
        if not subsystem.can_build_meta_human(character=character):
            rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
            rig.blocking = True; rig.report_progress = False
            rig.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY  # no face blend shapes: cheaper for a crowd of players
            subsystem.request_auto_rigging(character=character, params=rig)
            textures = unreal.MetaHumanCharacterTextureRequestParams()
            textures.blocking = True; textures.report_progress = False
            subsystem.request_texture_sources(character=character, params=textures)
        if not subsystem.can_build_meta_human(character=character, log_error=True):
            raise RuntimeError(f'{path} is not ready to build after rigging and textures')
        build = unreal.MetaHumanCharacterEditorBuildParameters()
        build.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        build.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
        build.absolute_build_path = f"{ROOT}/Built"
        build.common_folder_path = f"{ROOT}/Built/Common"
        build.enable_wardrobe_item_validation = False
        try:
            subsystem.build_meta_human(character=character, params=build)
        except Exception as error:
            # Epic's face control rigs log "Cannot break link" while being copied into Common; Python raises on those
            # logs although the assembly succeeds. The assets on disk decide.
            unreal.log_warning(f"PLAYER_METAHUMAN_BUILD_LOGGED {face['id']}: {str(error)[:200]}")
        if not library.list_assets(f"{ROOT}/Built/MHC_{face['id']}", recursive=True):
            raise RuntimeError(f"{path} assembly produced no assets")
        unsaved = []
        for asset in library.list_assets(f"{ROOT}/Built", recursive=True):
            try:
                library.save_asset(asset, only_if_is_dirty=True)
            except Exception:  # Epic's ARKit mapping asset logs a skeleton mismatch while saving; keep going
                unsaved.append(asset.rsplit('/', 1)[-1])
        if unsaved:
            unreal.log_warning(f"PLAYER_METAHUMAN_SAVE_LOGGED {face['id']}: {', '.join(unsaved)}")
        try:
            library.save_loaded_asset(character)
        except Exception as error:  # the ARKit mapping's skeleton warning can surface here too; the package still saves
            unreal.log_warning(f"PLAYER_METAHUMAN_SAVE_LOGGED {face['id']}: {str(error)[:200]}")
    finally:
        if subsystem.is_object_added_for_editing(character):
            subsystem.remove_object_to_edit(character)


def run():
    faces = json.loads((Path(unreal.Paths.project_dir()).parent / 'GameClient/ArtSource/Characters/Player/faces.json').read_text())['faces']
    only = {f for f in os.environ.get('SF_FACES', '').split(',') if f}
    subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    library = unreal.EditorAssetLibrary
    # Plugin content is not in the registry of a fresh run until scanned; duplicate_asset needs it there.
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([PRESETS, HAIR], True)
    for face in faces:
        if only and face['id'] not in only:
            continue
        try:
            build_face(face, subsystem, library)
            unreal.log(f"PLAYER_METAHUMAN_BUILT id={face['id']} preset={face['preset']}")
        except Exception as error:  # report every face, then carry on with the next
            unreal.log_error(f"PLAYER_METAHUMAN_FAILED id={face['id']} {error}")


def _first_tick(_delta):
    unreal.unregister_slate_post_tick_callback(_handle)  # the build pumps Slate; re-entering this callback crashes Python
    try:
        run()
    finally:
        unreal.SystemLibrary.quit_editor()


_handle = unreal.register_slate_post_tick_callback(_first_tick)
