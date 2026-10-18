"""Opt-in UE editor script: create a NEW local development sandbox map.

Not run by a normal E1 launch; no default map or cook-list edits.
"""
import unreal

MAP = '/Game/Frontier/Maps/Greenlands_ConstructionSandbox'
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mode = unreal.load_class(None, '/Script/ShatteredFrontier.SFConstructionSandboxGameMode')
if mode is None:
    raise RuntimeError('Build the sandbox adapter before generating its map.')
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError('Sandbox map already exists; refusing overwrite.')
if not levels.new_level(MAP):
    raise RuntimeError('Could not create sandbox fixture map.')
unreal.EditorLevelLibrary.get_editor_world().get_world_settings().set_editor_property('default_game_mode', mode)
floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
floor.set_actor_label('Construction_GeometryPilot_Floor')
floor.tags = ['SFConstructionSandboxFoundationV1']
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.static_mesh_component.set_collision_profile_name('BlockAll')
floor.set_actor_scale3d(unreal.Vector(20, 20, 1))
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(100, -350, 110))
actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(pitch=-50, yaw=-30))
actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 500))
label = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(0, 100, 100), unreal.Rotator(yaw=-90))
label.text_render.set_text('CONSTRUCTION DEVELOPMENT SANDBOX | UNLIMITED TEST STOCK | See HUD for controls and incomplete features.')
label.text_render.set_world_size(12)
if not levels.save_current_level():
    raise RuntimeError('Could not save sandbox fixture map.')
unreal.log('CONSTRUCTION_GEOMETRY_PILOT_MAP_CREATED ' + MAP)
