"""Create one Silverbrook building review map in the open editor; refuse overwrite."""
import re
import sys
from pathlib import Path

import unreal as u

name = sys.argv[1] if len(sys.argv) == 2 else ''
if not re.fullmatch(r'[A-Z][A-Za-z0-9]*', name):
    raise ValueError('Usage: Create-BuildingDevMap.py <PascalName>')
MAP = f'/Game/Frontier/Maps/Dev/{name}/{name}_Review'
mesh = u.load_asset(f'/Game/Frontier/Environment/Greenlands/Silverbrook/Meshes/SM_Silverbrook{name}')
ground = u.load_asset('/Game/Frontier/Environment/Greenlands/Nature/Materials/MI_GN_Ground')
assert mesh and ground, (mesh, ground)
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
if u.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError(f'Refusing to overwrite {MAP}')
assert levels.new_level(MAP), MAP
floor = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(0, 0, -50))
floor.set_actor_label('Terrain')
floor.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
floor.static_mesh_component.set_material(0, ground)
floor.static_mesh_component.set_collision_profile_name('BlockAll')
floor.set_actor_scale3d(u.Vector(60, 60, 1))
actors.spawn_actor_from_class(u.PlayerStart, u.Vector(0, -1500, 110), u.Rotator(yaw=90))
camera = actors.spawn_actor_from_class(u.CameraActor, u.Vector(0, 0, 200))
camera.set_actor_label('BuildingReviewCamera')
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'GameClient/ArtSource/Greenlands'))
import lighting
lighting.apply()
# Review fixture only: the town sun (yaw 135) leaves the +Y street face in shade; yaw -135 lights the +X and +Y faces.
lighting._one(u.DirectionalLight).set_actor_rotation(u.Rotator(roll=0, pitch=lighting.load_config()['sun.pitch_deg'], yaw=-135), False)
building = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(0, 0, 0), u.Rotator())
building.static_mesh_component.set_static_mesh(mesh)
building.set_actor_label(name)
building.set_folder_path('Districts/Dev')
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'GameClient/ArtSource/Silverbrook'))
import place_parts
place_parts.attach(building)
assert levels.save_current_level(), MAP
print('DEV_MAP_READY', MAP)
