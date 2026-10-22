"""Run in this worktree's editor to verify CRF-01 item definition ids."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).parent
recipes = json.loads((root / 'content/crafting/recipes_greenlands_starter.json').read_text())['recipes']
base = {line['definition_id'] for recipe in recipes for line in recipe['outputs']}
expected = set(base)
for item in base:
    expected.update(item + '/' + grade for grade in ('crude', 'fine', 'superior', 'masterwork'))
expected.update('item/recipe_scroll_' + recipe['id'].split('/')[1]
                for recipe in recipes if 'scroll' in recipe['learn']['sources'])

def asset_name(item_id):
    return 'DA_' + '_'.join(''.join(word.title() for word in part.split('_')) for part in item_id[5:].split('/'))

for item_id in sorted(expected):
    path = '/Game/Frontier/Gameplay/Pilot/' + asset_name(item_id)
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None or asset.get_editor_property('UniqueId') != item_id:
        raise RuntimeError(f'CRF-01 item id missing or mismatched: {item_id} at {path}')
unreal.log(f'CRF01_ITEM_IDS_OK count={len(expected)}')
