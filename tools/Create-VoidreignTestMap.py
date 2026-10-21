"""Isolated viewing/test map for the Voidreign: floor, sun, sky light, a fixed side camera and BP_Voidreign with the
ability demo running (Editor Python, headless; the map is new, so the open editor holds nothing it writes).

    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -nullrhi -unattended -script="<project>/tools/Create-VoidreignTestMap.py"

Open it in the editor and press Play to watch the horse alone, or run it as -game for automated traces/captures.
"""
import unreal

MAP = '/Game/Frontier/Creatures/Voidreign/V1/Test/VoidreignTest'
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(MAP): unreal.EditorAssetLibrary.delete_asset(MAP)
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.set_actor_scale3d(unreal.Vector(80, 80, 1)); floor.set_actor_label('Floor')
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(roll=0, pitch=-45, yaw=30))
sun.set_actor_label('Sun')
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label('SkyAtmosphere')
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 800)); sky.set_actor_label('SkyLight')
sky.light_component.set_editor_property('real_time_capture', True)  # ambient from the atmosphere
horse = actors.spawn_actor_from_class(unreal.load_asset('/Game/Frontier/Creatures/Voidreign/V1/BP_Voidreign').generated_class(), unreal.Vector(0, 0, 150))
horse.set_editor_property('DemoAbilityInterval', 3.0); horse.set_actor_label('Voidreign')
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-1500, -2500, 150)).set_actor_label('PlayerStart')  # player pawn clear of the horse
# Close side view of the horse at rest; the Rift charge leaves frame and the recall brings it back.
cam = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, -700, 220), unreal.Rotator(roll=0, pitch=-8, yaw=90))
cam.set_actor_label('SideCamera'); cam.set_editor_property('auto_activate_for_player', unreal.AutoReceiveInput.PLAYER0)
if not unreal.EditorLoadingAndSavingUtils.save_map(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), MAP):
    raise RuntimeError('Could not save ' + MAP)
unreal.log('VOIDREIGN_TEST_MAP ' + MAP)
