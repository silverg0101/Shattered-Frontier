"""Build the player MetaHumans from MetaHuman Creator presets (UE 5.8 editor Python).

MetaHuman assembly bakes textures with Texture Graph, which needs the full editor with rendering (a -nullrhi or commandlet run
fails with "Couldn't find baked texture"). Run it from PowerShell:
  UnrealEditor.exe <uproject> -RenderOffscreen -unattended -nosplash -ExecCmds="py <abs path of this file>"
It starts on the editor's first tick, builds, then quits the editor.

Reads GameClient/ArtSource/Characters/Player/faces.json ({"faces": [{"id", "preset", "gender", "hair", "beard", "mustache", "skin", "hair_color"}]}).
Optional "skin": [u, v] sets the skin tone (the Creator's tone picker position); every build logs the face's tone as PLAYER_METAHUMAN_SKIN.
Optional "freckles": 1-3 picks a freckle mask; "freckle_strength" (0-1, Creator default 0.2) sets its strength and density.
Optional "hair_color": [melanin, redness] (0-1) dyes every groom (logs PLAYER_METAHUMAN_HAIR); "groom_colors":
{"Eyebrows"|"Beards"|"Mustaches"|"Eyelashes"|"Hair": [melanin, redness]} overrides it for the grooms in that wardrobe folder.
Optional "makeup": "none" clears the preset's foundation, eye shadow/liner, blush and lipstick, and sets the lip, under-eye
and cheek skin accents back to neutral 0.5 (logs PLAYER_METAHUMAN_MAKEUP). Optional "accents": {"<region>": [lightness, redness,
saturation]} (regions: cheeks, chin, ears, forehead, lips, nose, scalp, under_eye; 0.5 neutral) sets a region after that.
Each face's preset is copied to /Game/Frontier/Characters/Player/Faces/MHC_<id> and given its grooms (WI_* wardrobe
items). MetaHuman's cloud auto-rig and texture download run only when the copy cannot build yet (they use the editor's
signed-in Epic account). The optimized, medium-quality assembly goes to /Game/Frontier/Characters/Player/Built/MHC_<id>,
with shared assets in .../Built/Common. Env SF_FACES=<id,id> limits the run. Logs PLAYER_METAHUMAN_BUILT / _FAILED.
"""
import json
import os
from pathlib import Path

import unreal

ROOT = '/Game/Frontier/Characters/Player'
PRESETS = '/MetaHumanCharacter/Optional/Presets'
GROOMS = '/MetaHumanCharacter/Optional/Grooms/Bindings'


def build_face(face, subsystem, library):
    path = f"{ROOT}/Faces/MHC_{face['id']}"
    fresh = not library.does_asset_exist(path)
    character = library.duplicate_asset(f"{PRESETS}/{face['preset']}", path) if fresh else library.load_asset(path)
    if character is None:
        raise RuntimeError(f"Could not copy preset {face['preset']} to {path}")
    instance = character.internal_collection.default_instance
    if fresh:
        # The face's own grooms replace the preset's: "hair", "beard", "mustache" name WI_* wardrobe items; a slot not listed is cleared.
        slots = [str(s) for s in character.internal_collection.get_slot_names()]
        for slot in ('Hair', 'Beard', 'Mustache'):
            if slot not in slots:
                continue
            wanted = face.get(slot.lower())
            item = library.load_asset(f"{GROOMS}/{slot}s/{wanted}" if slot != 'Hair' else f"{GROOMS}/Hair/{wanted}") if wanted else None
            if wanted and item is None:
                raise RuntimeError(f"Unknown {slot} groom {wanted}")
            key = character.internal_collection.try_add_item_from_wardrobe_item(slot, item) if item else unreal.MetaHumanPaletteItemKey()
            instance.set_single_slot_selection(slot, key)
    if fresh and face.get('bare'):
        # "bare": no preset clothing, so the body keeps the torso and thighs the default garments hide (authoring base for tools/Export-PlayerBaseBody.py).
        for slot in ('Outfits', 'Top Garment', 'Bottom Garment'):
            if slot in [str(s) for s in character.internal_collection.get_slot_names()]:
                instance.set_single_slot_selection(slot, unreal.MetaHumanPaletteItemKey())
    if not subsystem.try_add_object_to_edit(character=character):
        raise RuntimeError(f'{path} is open for edit elsewhere')
    try:
        skin = character.get_editor_property('skin_settings')
        if face.get('skin'):
            skin.skin.u, skin.skin.v = face['skin']
        if face.get('freckles'):
            skin.freckles.mask = getattr(unreal.MetaHumanCharacterFrecklesMask, f"TYPE{face['freckles']}")
        if face.get('freckle_strength') is not None:
            skin.freckles.strength = skin.freckles.density = face['freckle_strength']
        if face.get('makeup') == 'none':
            # The preset's lip, under-eye and cheek accents read as lipstick, liner and blush; 0.5 is neutral.
            for region in ('lips', 'under_eye', 'cheeks'):
                accent = getattr(skin.accents, region)
                accent.lightness = accent.redness = accent.saturation = 0.5
                setattr(skin.accents, region, accent)
        for region, (lightness, redness, saturation) in face.get('accents', {}).items():
            accent = getattr(skin.accents, region)
            accent.lightness, accent.redness, accent.saturation = lightness, redness, saturation
            setattr(skin.accents, region, accent)
        if face.get('skin') or face.get('freckles') or face.get('makeup') == 'none' or face.get('accents'):
            subsystem.commit_skin_settings(character, skin)
        unreal.log(f"PLAYER_METAHUMAN_SKIN id={face['id']} u={skin.skin.u:.3f} v={skin.skin.v:.3f} freckles={skin.freckles.mask}")
        if face.get('hair_color'):
            # Groom colour lives in instance parameters, which exist only on the editor's built preview collection;
            # on_edit_preview_collection copies them into the character for the build. Every groom with Melanin
            # (hair, eyebrows, beard, mustache, eyelashes, peach fuzz) gets the same colour unless "groom_colors"
            # names its wardrobe folder (the item path text holds .../Grooms/Bindings/<Folder>/WI_...).
            subsystem.assemble_for_preview(character=character)
            preview = subsystem.get_preview_collection(character).default_instance
            overrides = face.get('groom_colors', {})
            grooms = []
            for item in preview.get_instance_parameter_item_paths():
                params = {str(p.name): p for p in preview.get_instance_parameters(item_path=item)}
                if 'Melanin' in params:
                    text = item.export_text()
                    folder = next((k for k in overrides if f"/{k}/" in text), None)
                    melanin, redness = overrides[folder] if folder else face['hair_color']
                    params['Melanin'].set_float(value=melanin)
                    params['Redness'].set_float(value=redness)
                    params['Whiteness'].set_float(value=0.0)
                    grooms.append(f"{folder or 'all'}={melanin:.2f}/{redness:.2f}")
            subsystem.on_edit_preview_collection(character)
            unreal.log(f"PLAYER_METAHUMAN_HAIR {face['id']} grooms={len(grooms)} {' '.join(grooms)}")
        if face.get('makeup') == 'none':
            makeup = character.get_editor_property('makeup_settings')
            makeup.foundation.apply_foundation = False
            makeup.eyes.type = unreal.MetaHumanCharacterEyeMakeupType.NONE
            makeup.eyes.opacity = 0.0
            makeup.blush.type = unreal.MetaHumanCharacterBlushMakeupType.NONE
            makeup.blush.intensity = 0.0
            makeup.lips.type = unreal.MetaHumanCharacterLipsMakeupType.NONE
            makeup.lips.opacity = 0.0
            subsystem.commit_makeup_settings(character, makeup)
            unreal.log(f"PLAYER_METAHUMAN_MAKEUP {face['id']}")
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
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([PRESETS, GROOMS], True)
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
