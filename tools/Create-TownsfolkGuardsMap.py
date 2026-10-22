"""Dev map for townsfolk step 5 (crime and guards): a long floor, sun and sky, nothing else. With -TownsfolkCrimeScenario
USFTownsfolkSubsystem spawns the town state, a guard and three residents on it and runs the attack check
(Editor Python, headless; the map is new, so no open editor holds it).

    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -nullrhi -unattended -script="<project>/tools/Create-TownsfolkGuardsMap.py"
"""
import unreal

MAP = '/Game/Frontier/Maps/Dev/TownsfolkGuards/TownsfolkGuards'
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(MAP): unreal.EditorAssetLibrary.delete_asset(MAP)
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
# The scenario uses x from 0 to 152 m (guard at the origin, residents at 3, 51 and 151 m).
floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(7000, 0, -50))
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.set_actor_scale3d(unreal.Vector(420, 60, 1)); floor.set_actor_label('Floor')
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(roll=0, pitch=-45, yaw=30))
sun.set_actor_label('Sun')
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label('SkyAtmosphere')
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 800)); sky.set_actor_label('SkyLight')
sky.light_component.set_editor_property('real_time_capture', True)
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(300, -500, 150)).set_actor_label('PlayerStart')
if not unreal.EditorLoadingAndSavingUtils.save_map(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), MAP):
    raise RuntimeError('Could not save ' + MAP)
unreal.log('TOWNSFOLK_GUARDS_MAP ' + MAP)
