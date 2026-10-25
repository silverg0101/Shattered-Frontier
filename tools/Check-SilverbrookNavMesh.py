"""Check saved Silverbrook navigation in a freshly loaded editor map.

Run through tools/Run-InEditor.py or UnrealEditor-Cmd -run=pythonscript.
"""
import json
from pathlib import Path

import unreal as u


MAP = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
RECORD = Path(__file__).resolve().parents[1] / 'content/world/zones/greenlands_silverbrook.json'


def check():
    editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
    if MAP not in editor.get_editor_world().get_path_name():
        u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(MAP)
    world = editor.get_editor_world()
    actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
    by_label = {actor.get_actor_label(): actor for actor in actors}
    bounds = by_label.get('NavMeshBounds_SilverbrookPlayable')
    assert bounds and isinstance(bounds, u.NavMeshBoundsVolume), 'Silverbrook nav bounds missing'
    _, extent = bounds.get_actor_bounds(False)
    assert extent.x >= 75000 and extent.y >= 75000, f'Playable extent too small: {extent}'
    recast = [actor for actor in actors if actor.get_class().get_name() == 'RecastNavMesh']
    assert len(recast) == 1, f'Expected one RecastNavMesh, found {len(recast)}'
    runtime = str(recast[0].get_editor_property('runtime_generation'))
    assert 'STATIC' in runtime.upper(), f'Dedicated-server navmesh must remain static: {runtime}'
    u.log(f'SILVERBROOK_NAV_RUNTIME Generation={runtime}')
    record = json.loads(RECORD.read_text(encoding='utf-8'))
    for group in record['spawn_groups']:
        if group['group_id'] not in ('boar_farm_edge', 'boar_forest_edge'):
            continue
        points = group['points']
        assert len(points) >= 2
        markers = [by_label.get('BoarSpawn_' + point['point_id']) for point in points[:2]]
        assert all(markers), f'Missing {group["group_id"]} spawn markers'
        path = u.NavigationSystemV1.find_path_to_location_synchronously(
            world, markers[0].get_actor_location(), markers[1].get_actor_location())
        assert path and path.is_valid() and not path.is_partial() and len(path.path_points) >= 2, (
            f'No complete Recast path at {group["group_id"]}')
        u.log(f'SILVERBROOK_NAV_PATH_PASS Group={group["group_id"]} Points={len(path.path_points)}')
    u.log('SILVERBROOK_NAV_CHECK_PASS Paths=2 Bounds=1')


if __name__ == '__main__':
    check()
