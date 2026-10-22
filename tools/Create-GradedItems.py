"""Create missing CRF-01 definition assets in this worktree's open editor.

Run with tools/Run-InEditor.py. Existing assets are read as templates and never overwritten.
The common definition retains its ungraded id; all other grades are sibling ids.
"""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).parent
recipes = json.loads((root / 'content/crafting/recipes_greenlands_starter.json').read_text())['recipes']
outputs = {line['definition_id'] for recipe in recipes for line in recipe['outputs']}
scrolls = {'item/recipe_scroll_' + recipe['id'].split('/')[1]
           for recipe in recipes if 'scroll' in recipe['learn']['sources']}
library = unreal.EditorAssetLibrary
folder = '/Game/Frontier/Gameplay/Pilot'
klass = unreal.load_class(None, '/Script/MinimalInventoryRuntime.InventoryItemDefinition')
if klass is None:
    raise RuntimeError('MinimalInventoryRuntime item class unavailable')
factory = unreal.DataAssetFactory()
factory.set_editor_property('data_asset_class', klass)
tools = unreal.AssetToolsHelpers.get_asset_tools()

def name(item_id):
    parts = item_id[5:].split('/')
    return 'DA_' + '_'.join(''.join(w.title() for w in part.split('_')) for part in parts)

def display(item_id):
    parts = item_id[5:].split('/')
    return parts[0].replace('_', ' ').title() + (f' ({parts[1].title()})' if len(parts) > 1 else '')

settings_class = unreal.load_class(None, '/Script/ShatteredFrontier.SFGatheringPilotSettings')
settings = unreal.get_default_object(settings_class)
stack = settings.get_editor_property('MaterialStackUnits')
weight = settings.get_editor_property('MaterialUnitWeightKg')
created = []
for item_id in sorted(outputs | scrolls):
    ids = ([item_id] if item_id in scrolls else
           [item_id] + [item_id + '/' + grade for grade in ('crude', 'fine', 'superior', 'masterwork')])
    base_path = folder + '/' + name(item_id)
    base = library.load_asset(base_path) if library.does_asset_exist(base_path) else None
    for identity in ids:
        asset_name = name(identity)
        path = folder + '/' + asset_name
        if library.does_asset_exist(path):
            existing = library.load_asset(path)
            if existing.get_editor_property('UniqueId') != identity:
                raise RuntimeError(f'Existing id mismatch at {path}')
            continue
        asset = tools.create_asset(asset_name, folder, klass, factory)
        if asset is None:
            raise RuntimeError(f'Could not create {path}')
        asset.set_editor_property('UniqueId', identity)
        asset.set_editor_property('DisplayName', unreal.Text(display(identity)))
        asset.set_editor_property('MaxStackSize', stack)
        asset.set_editor_property('Weight', weight)
        if base:
            asset.set_editor_property('Icon', base.get_editor_property('Icon'))
        if identity in scrolls:
            asset.set_editor_property('UseType', unreal.InventoryItemUseType.CONSUME)
            asset.set_editor_property('Description', unreal.Text('Use to learn a crafting recipe.'))
        if not library.save_loaded_asset(asset):
            raise RuntimeError(f'Could not save {path}')
        created.append(path)
unreal.log('CRF01_ITEMS_CREATED ' + ', '.join(created))
