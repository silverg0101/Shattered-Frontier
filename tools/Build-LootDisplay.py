"""Build a development-only model review display in SB_LootModels.

The map is a copy of the existing personal-loot sandbox, not a player feature.
"""
import json
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).parent
MAP = '/Game/Frontier/Maps/Sandbox/SB_LootModels'
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0] == MAP, world.get_path_name()
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actor_tools = u.get_editor_subsystem(u.EditorActorSubsystem)
for actor in u.EditorLevelLibrary.get_all_level_actors():
    if actor.get_actor_label() in ('NetworkProbe', 'PersonalLootBoar',
                                   'PersonalLootBoss_BanditCaptainBinding') or str(actor.get_folder_path()) == 'LootDisplay':
        assert actor_tools.destroy_actor(actor)

rows = json.loads((ROOT / 'Artifacts/LootItems/import-evidence.json').read_text())
cube = u.load_asset('/Engine/BasicShapes/Cube')
assert cube
for index, row in enumerate(rows):
    stem = row['item_id'][5:]
    mesh = u.load_asset(row['mesh'])
    assert isinstance(mesh, u.StaticMesh), stem
    x = (index % 4 - 1.5) * 250.0
    y = (index // 4) * 300.0
    bounds = mesh.get_bounding_box()
    z = -bounds.min.z + 0.5
    actor = actor_tools.spawn_actor_from_class(u.StaticMeshActor, u.Vector(x, y, z))
    actor.set_actor_label('Loot_' + stem)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_folder_path('LootDisplay')
    if stem == 'wolfhide_bracers':
        second = actor_tools.spawn_actor_from_class(u.StaticMeshActor, u.Vector(x + 36, y, z))
        second.set_actor_label('Loot_wolfhide_bracers_mirrored')
        second.static_mesh_component.set_static_mesh(mesh)
        second.set_actor_scale3d(u.Vector(1, -1, 1))
        second.set_folder_path('LootDisplay')
    reference_x = x + max(18.0, float(row['target_cm']['length']) * .8)
    reference = actor_tools.spawn_actor_from_class(u.StaticMeshActor, u.Vector(reference_x, y, 5))
    reference.set_actor_label('Scale10cm_' + stem)
    reference.static_mesh_component.set_static_mesh(cube)
    reference.set_actor_scale3d(u.Vector(.1, .1, .1))
    reference.set_folder_path('LootDisplay')
camera = actor_tools.spawn_actor_from_class(u.CameraActor, u.Vector(0, -400, 150))
camera.set_actor_label('LootReviewCamera')
camera.set_folder_path('LootDisplay')
assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
print('LOOT_DISPLAY_READY', len(rows), MAP)
