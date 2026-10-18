"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>."""
import unreal

MAP = "/Game/Frontier/Maps/Greenlands_E1"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError("E1 map already exists; inspect/edit it instead of overwriting content.")
if not levels.new_level(MAP):
    raise RuntimeError("Could not create E1 map")
cube = unreal.load_asset("/Engine/BasicShapes/Cube")


def block(label, position, scale, rotation=unreal.Rotator()):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*position), rotation)
    actor.set_actor_label(label)
    mesh = actor.static_mesh_component
    mesh.set_static_mesh(cube)
    mesh.set_collision_profile_name("BlockAll")
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


# Centimetres and versioned fixture geometry; this is not biome production content.
block("E1_Floor_60m", (0, 0, -50), (60, 60, 1))
block("E1_Wall", (1000, 0, 150), (1, 20, 3))
block("E1_Corner", (500, 950, 150), (10, 1, 3))
block("E1_JumpStep", (-500, 500, 30), (3, 3, .6))
block("E1_Ramp", (-900, -600, 110), (8, 4, .3), unreal.Rotator(pitch=15))
block("E1_ArrivalLandmark", (0, 1800, 300), (2, 2, 6))
for index, x in enumerate((-300, 300)):
    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, -1000, 110))
    start.set_actor_label("E1_PlayerStart_" + str(index + 1))
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(pitch=-50, yaw=-30))
sun.set_actor_label("E1_Sun")
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 500))
sky.set_actor_label("E1_SkyLight")
assert len(actors.get_all_level_actors()) >= 10
if not levels.save_current_level():
    raise RuntimeError("Could not save E1 map")
unreal.log("E1_MAP_CREATED " + MAP + " Starts=2 Blocks=6")
