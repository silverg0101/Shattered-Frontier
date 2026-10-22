"""GM-01 follow-up: create the build-mode review map /Game/Frontier/Maps/Dev/BuildMode/BuildMode_Review.

Flat floor, PlayerStart, light. No world-settings override, so the project default ASFGameMode and ASFPlayerController run;
ASFGameMode::IsAdmitted admits maps under /Game/Frontier/Maps/Dev/ as construction fixtures.
Run inside the editor: python tools/Run-InEditor.py tools/Create-BuildModeDevMap.py (refuses to overwrite).
"""
import unreal

MAP = '/Game/Frontier/Maps/Dev/BuildMode/BuildMode_Review'
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError('Build-mode review map already exists; refusing overwrite.')
if not levels.new_level(MAP):
    raise RuntimeError('Could not create the build-mode review map.')
floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
floor.set_actor_label('BuildMode_Floor')
floor.tags = ['SFConstructionSandboxFoundationV1']
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.static_mesh_component.set_collision_profile_name('BlockAll')
floor.set_actor_scale3d(unreal.Vector(40, 40, 1))
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, -600, 110), unreal.Rotator(yaw=90))
actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(pitch=-50, yaw=-30))
actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 500))
if not levels.save_current_level():
    raise RuntimeError('Could not save the build-mode review map.')
unreal.log('BUILDMODE_DEV_MAP_CREATED ' + MAP)
