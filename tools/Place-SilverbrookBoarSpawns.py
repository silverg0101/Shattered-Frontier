"""Save editor-only point markers for the two World-authored Silverbrook boar groups.

Run: py -3 tools/Run-InEditor.py tools/Place-SilverbrookBoarSpawns.py
Repeated runs replace only these markers. Runtime slots use the zone record.
"""
import json
import sys
from pathlib import Path

import unreal as u

ZONE = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
PREFIX = 'BoarSpawn_'
RECORD = Path(__file__).resolve().parents[1] / 'content/world/zones/greenlands_silverbrook.json'
GREENLANDS = Path(__file__).resolve().parents[1] / 'GameClient/ArtSource/Greenlands'


def place(save=True, ground=None):
    editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
    if ZONE not in editor.get_editor_world().get_path_name():
        u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(ZONE)
    world = editor.get_editor_world()
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    record = json.loads(RECORD.read_text(encoding='utf-8'))
    if ground is None:
        sys.path.insert(0, str(GREENLANDS))
        from heightfield import Heightfield, zone_region
        from validate_layout import SILVERBROOK_LAYOUT
        layout = json.loads(SILVERBROOK_LAYOUT.read_text(encoding='utf-8'))
        region, _, _ = zone_region('silverbrook-town-layout.json')
        ground = Heightfield(layout, region=region).at
    groups = [g for g in record['spawn_groups'] if g['group_id'] in ('boar_farm_edge', 'boar_forest_edge')]
    assert len(groups) == 2 and all(g['spawn_kind'] == 'npc' and g['active_count'] == 5 and len(g['points']) == 12 for g in groups)
    for actor in actors.get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            actors.destroy_actor(actor)
    count = 0
    for group in groups:
        for point in group['points']:
            x, y = point['position']
            z = ground(x, y) * 100 + 65
            marker = actors.spawn_actor_from_class(u.TargetPoint, u.Vector(x * 100, y * 100, z),
                                                   u.Rotator(roll=0, pitch=0, yaw=point['facing']))
            assert marker, point['point_id']
            marker.set_actor_label(PREFIX + point['point_id'])
            marker.set_folder_path('Gameplay/BoarSpawns/' + group['group_id'])
            marker.set_editor_property('tags', [u.Name(group['group_id']), u.Name(point['point_id'])])
            count += 1
    if save:
        assert u.EditorLoadingAndSavingUtils.save_map(world, ZONE), 'Could not save Silverbrook'
    u.log(f'BOAR_SPAWNS_PLACED Map={ZONE} Groups={len(groups)} Points={count} ActiveSlots=10')


if __name__ == '__main__':
    place()
