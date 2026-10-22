"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file> (or in the open editor via tools/Run-InEditor.py): creates /Game/Frontier/Maps/Dev/DialogueUI (flat plot, light, PlayerStart,
one ASFDialogueNpc) and imports the quest art as /Game/Frontier/UI/Dialogue/T_Quest_MillsSupply. Refuses to overwrite the map."""
import os
import unreal

MAP = "/Game/Frontier/Maps/Dev/DialogueUI/DialogueUI"
ART_DIR = "/Game/Frontier/UI/Dialogue"
ART = os.path.join(unreal.Paths.project_dir(), "ArtSource", "UI", "Kit-v5", "scenes", "quest_mills_supply.png")

if not unreal.EditorAssetLibrary.does_asset_exist(ART_DIR + "/T_Quest_MillsSupply"):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.normpath(ART))
    task.set_editor_property("destination_path", ART_DIR)
    task.set_editor_property("destination_name", "T_Quest_MillsSupply")
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(ART_DIR + "/T_Quest_MillsSupply")
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)

if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError("Dialogue dev map already exists; edit it instead of overwriting.")
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not levels.new_level(MAP):
    raise RuntimeError("Could not create dialogue map")
cube = unreal.load_asset("/Engine/BasicShapes/Cube")
floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50), unreal.Rotator())
floor.set_actor_label("Dialogue_Floor")
floor.static_mesh_component.set_static_mesh(cube)
floor.static_mesh_component.set_collision_profile_name("BlockAll")
floor.set_actor_scale3d(unreal.Vector(40, 40, 1))
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 110), unreal.Rotator()).set_actor_label("Dialogue_PlayerStart")
npc_class = unreal.load_class(None, "/Script/ShatteredFrontier.SFDialogueNpc")
actors.spawn_actor_from_class(npc_class, unreal.Vector(250, 0, 0), unreal.Rotator(yaw=180)).set_actor_label("Dialogue_Millwarden_Oswin")
actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(pitch=-45, yaw=-30)).set_actor_label("Dialogue_Sun")
actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 500)).set_actor_label("Dialogue_SkyLight")
if not levels.save_current_level():
    raise RuntimeError("Could not save dialogue map")
