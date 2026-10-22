"""GFX-02 J1b: fail unless every authoring/debug actor is hidden in game.

python tools/Run-InEditor.py GameClient/ArtSource/Greenlands/check_hidden_in_game.py   (open Greenlands_Silverbrook)
Checks: folders Labels, Townsfolk/Lanes; graybox Marker boxes in Places; classes TextRenderActor, ZoneShape, ZoneGraphData.
"""
import unreal as u

actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
CLASSES = {'TextRenderActor', 'ZoneShape', 'ZoneGraphData'}
FOLDERS = {'Labels', 'Townsfolk/Lanes'}

def debug(a):
    if a.get_class().get_name() in CLASSES or str(a.get_folder_path()) in FOLDERS:
        return True
    if isinstance(a, u.StaticMeshActor) and str(a.get_folder_path()) == 'Places':
        m = a.static_mesh_component.get_material(0)
        return m is not None and m.get_name() == 'MI_Graybox_Marker'
    return False

checked = [a for a in actors if debug(a)]
bad = [a.get_actor_label() for a in checked if not a.get_editor_property('hidden')]
print('HIDDEN_IN_GAME_CHECK', 'checked', len(checked), 'visible_in_game', len(bad), bad[:10])
assert checked and not bad, bad
