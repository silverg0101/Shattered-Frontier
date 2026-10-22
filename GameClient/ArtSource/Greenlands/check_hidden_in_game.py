"""GFX-02 J1b: fail unless every authoring/debug actor is hidden in game, and no visible town mesh shows a default material.

python tools/Run-InEditor.py GameClient/ArtSource/Greenlands/check_hidden_in_game.py   (open Greenlands_Silverbrook)
Checks: folders Labels, Townsfolk/Lanes; graybox (MI_Graybox_*) boxes in Places and Plots; classes TextRenderActor, ZoneShape,
ZoneGraphData. Gate (WP6c fix 2): every StaticMeshActor not hidden inside the town wall has no empty, engine-default or graybox slot.
"""
import json
from pathlib import Path
import unreal as u
import argparse
parser=argparse.ArgumentParser();parser.add_argument('--all',action='store_true');args=parser.parse_args()

actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
CLASSES = {'TextRenderActor', 'ZoneShape', 'ZoneGraphData'}
FOLDERS = {'Labels', 'Townsfolk/Lanes'}
WALL = json.loads((Path(__file__).resolve().parent / 'silverbrook-town-layout.json').read_text(encoding='utf-8'))['wall']

def debug(a):
    if a.get_class().get_name() in CLASSES or str(a.get_folder_path()) in FOLDERS:
        return True
    if isinstance(a, u.StaticMeshActor) and str(a.get_folder_path()) in ('Places', 'Plots'):
        m = a.static_mesh_component.get_material(0)
        return m is not None and m.get_name().startswith('MI_Graybox')
    return False

def default_slots(a):
    c = a.static_mesh_component
    ms = [c.get_material(i) for i in range(c.get_num_materials())]
    return [m.get_name() if m else None for m in ms
            if not m or m.get_name() in ('WorldGridMaterial', 'BasicShapeMaterial', 'DefaultMaterial') or m.get_name().startswith('MI_Graybox')]

checked = [a for a in actors if debug(a)]
bad = [a.get_actor_label() for a in checked if not a.get_editor_property('hidden')]
print('HIDDEN_IN_GAME_CHECK', 'checked', len(checked), 'visible_in_game', len(bad), bad[:10])
assert (checked or args.all) and not bad, bad
town = [a for a in actors if isinstance(a, u.StaticMeshActor) and not a.get_editor_property('hidden') and a.static_mesh_component.static_mesh
        and (args.all or (a.get_actor_location().x / 100 / WALL['semi_axis_x']) ** 2 + (a.get_actor_location().y / 100 / WALL['semi_axis_y']) ** 2 < 1)]
grey = [(a.get_actor_label(), default_slots(a)) for a in town if default_slots(a)]
print('DEFAULT_MATERIAL_CHECK', 'visible_in_town', len(town), 'default_or_graybox', len(grey), grey[:10])
assert not grey, grey
