"""Author the playable Silverbrook Recast bounds without rebuilding the town.

Run with tools/Run-InEditor.py in an open editor for this lane. Unreal's
Python commandlet locks navigation building, so a persistent editor is needed.
Only the owned volume and /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook
are saved. Re-run after regenerating the map.
"""
import time

import unreal as u


MAP = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
LABEL = 'NavMeshBounds_SilverbrookPlayable'
PLAYABLE_RADIUS_M = 750  # Settlement definition section 5; includes both boar territories.


def place():
    assert '-run=pythonscript' not in u.SystemLibrary.get_command_line().lower(), (
        'Open this lane in Unreal Editor and use tools/Run-InEditor.py; commandlet nav build is locked')
    editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
    if MAP not in editor.get_editor_world().get_path_name():
        u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(MAP)
    world = editor.get_editor_world()
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    existing = list(actors.get_all_level_actors())
    terrain = [actor for actor in existing if actor.get_actor_label() == 'Terrain']
    assert len(terrain) == 1, f'Expected one Silverbrook terrain, found {len(terrain)}'
    center, extent = terrain[0].get_actor_bounds(False)
    assert extent.x >= PLAYABLE_RADIUS_M * 100 and extent.y >= PLAYABLE_RADIUS_M * 100
    assert extent.z > 0, 'Terrain bounds have no height'
    for actor in existing:
        if actor.get_actor_label() == LABEL:
            assert isinstance(actor, u.NavMeshBoundsVolume), 'Owned label belongs to another actor'
            assert actors.destroy_actor(actor)
    started = time.monotonic()
    volume = actors.spawn_actor_from_class(u.NavMeshBoundsVolume, center)
    assert volume, 'Could not spawn NavMeshBoundsVolume'
    volume.set_actor_label(LABEL)
    volume.set_folder_path('Gameplay/Navigation')
    # Spawned brush is a 2 m cube. Leave 20 m above and below the entire terrain.
    volume.set_actor_scale3d(u.Vector(PLAYABLE_RADIUS_M, PLAYABLE_RADIUS_M, extent.z / 100 + 20))
    vcenter, vextent = volume.get_actor_bounds(False)
    assert abs(vcenter.x) < 1 and abs(vcenter.y) < 1
    assert vextent.x >= PLAYABLE_RADIUS_M * 100 and vextent.y >= PLAYABLE_RADIUS_M * 100
    assert vextent.z >= extent.z + 2000
    u.log(f'SILVERBROOK_NAV_BOUNDS Center={vcenter} Extent={vextent} '
          f'RuntimeGeneration=ProjectDefaultStatic')
    u.log('SILVERBROOK_NAV_BUILD_START')
    u.SystemLibrary.execute_console_command(world, 'BUILDPATHS')
    u.log(f'SILVERBROOK_NAV_BUILD_RETURNED Seconds={time.monotonic() - started:.1f}')
    assert u.EditorLoadingAndSavingUtils.save_map(world, MAP), 'Could not save Silverbrook'
    u.log(f'SILVERBROOK_NAV_SAVED Seconds={time.monotonic() - started:.1f}')


if __name__ == '__main__':
    place()
