"""Place the Golden Stag Inn rest volume in the open Silverbrook editor.

Run: py -3 tools/Run-InEditor.py tools/Place-RestAreaVolumes.py
The building itself is located from the generated map, so rebuilt terrain height and
town yaw are respected. Repeated runs replace the one owned volume.
"""
import unreal as u

ZONE = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
BUILDING = '03_TheGoldenStagInn'
LABEL = 'RestArea_GoldenStagInn'


def place(save=True):
    editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
    if ZONE not in editor.get_editor_world().get_path_name():
        u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(ZONE)
    world = editor.get_editor_world()
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    existing = list(actors.get_all_level_actors())
    buildings = [a for a in existing if a.get_actor_label() == BUILDING]
    assert len(buildings) == 1, f'Expected one {BUILDING}, found {len(buildings)}'
    building = buildings[0]
    mesh = building.static_mesh_component.get_editor_property('static_mesh')
    assert mesh and mesh.get_name() == 'SM_SilverbrookGoldenStagInn', mesh
    rest_class = u.load_class(None, '/Script/ShatteredFrontier.SFRestAreaVolume')
    assert rest_class, 'SFRestAreaVolume class unavailable; build the Editor module'
    for actor in existing:
        if actor.get_actor_label() == LABEL:
            actors.destroy_actor(actor)
    at = building.get_actor_location() + u.Vector(0, 0, 450)
    volume = actors.spawn_actor_from_class(rest_class, at, building.get_actor_rotation())
    assert volume, 'Could not spawn inn rest area'
    volume.set_actor_label(LABEL)
    volume.set_folder_path('Gameplay/RestAreas')
    # Golden Stag masonry walls are at local X +/-5.6 m and Y +/-4.0 m;
    # the box is inset from those walls and spans ground and upper rooms.
    volume.set_editor_property('kind', u.SFRestAreaKind.INN)
    volume.set_editor_property('half_extent_metres', u.Vector(5.2, 3.6, 4.5))
    if save:
        assert u.EditorLoadingAndSavingUtils.save_map(world, ZONE), 'Could not save Silverbrook'
    u.log(f'REST_AREA_PLACED Map={ZONE} Label={LABEL} Building={BUILDING} '
          f'At={at} Yaw={building.get_actor_rotation().yaw} HalfExtentMetres=(5.2,3.6,4.5)')


if __name__ == '__main__':
    place()
