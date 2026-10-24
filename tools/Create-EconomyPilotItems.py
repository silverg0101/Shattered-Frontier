"""Create the INT-04 economy pilot item definitions (nails, wall bundle) in the UE editor.

Run after the ShatteredFrontier editor (gathering settings, SFItemDefinition) is built.
Creates only missing assets and never overwrites; ids and names follow content/crafting.
"""
import math

import unreal


OUTPUT_PATH = '/Game/Frontier/Gameplay/Pilot'
ITEM_CLASS_PATH = '/Script/ShatteredFrontier.SFItemDefinition'
ITEMS = (
    ('DA_IronNails', 'item/iron_nails', 'Iron Nails'),
    ('DA_WallMaterialBundle', 'item/wall_material_bundle', 'Wall Material Bundle'),
)

settings_class = unreal.load_class(None, '/Script/ShatteredFrontier.SFGatheringPilotSettings')
if settings_class is None:
    raise RuntimeError('Build the gathering pilot settings before generating item assets.')
settings = unreal.get_default_object(settings_class)
max_stack_size = settings.get_editor_property('MaterialStackUnits')
weight = settings.get_editor_property('MaterialUnitWeightKg')
if not 1 <= max_stack_size <= 100 or not math.isfinite(weight) or not 0.01 <= weight <= 50:
    raise RuntimeError('Pilot material configuration outside declared bounds.')

asset_library = unreal.EditorAssetLibrary
asset_paths = [OUTPUT_PATH + '/' + name for name, _, _ in ITEMS]
missing = [item for item, path in zip(ITEMS, asset_paths) if not asset_library.does_asset_exist(path)]

item_class = unreal.load_class(None, ITEM_CLASS_PATH)
if item_class is None:
    raise RuntimeError('Could not load ' + ITEM_CLASS_PATH + '; build the ShatteredFrontier editor first.')

factory = unreal.DataAssetFactory()
factory.set_editor_property('data_asset_class', item_class)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
created = []
for asset_name, unique_id, display_name in missing:
    asset = asset_tools.create_asset(asset_name, OUTPUT_PATH, item_class, factory)
    if asset is None:
        raise RuntimeError('Could not create ' + OUTPUT_PATH + '/' + asset_name)
    asset.set_editor_property('unique_id', unique_id)
    asset.set_editor_property('display_name', unreal.Text(display_name))
    asset.set_editor_property('max_stack_size', max_stack_size)
    asset.set_editor_property('weight', weight)
    if not asset_library.save_loaded_asset(asset):
        raise RuntimeError('Could not save ' + OUTPUT_PATH + '/' + asset_name)
    created.append(asset.get_path_name())

unreal.log('ECONOMY_PILOT_ITEMS_CREATED ' + ', '.join(created))
