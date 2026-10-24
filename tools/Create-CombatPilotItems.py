"""Create the combat pilot's boar-hide item definition in the UE editor.

Run after the ShatteredFrontier editor (SFItemDefinition, gathering pilot settings) is built:
    python tools/Run-InEditor.py tools/Create-CombatPilotItems.py
An existing asset is a hard error, so this script never overwrites.
"""
import math

import unreal


OUTPUT_PATH = '/Game/Frontier/Gameplay/Pilot'
ITEM_CLASS_PATH = '/Script/ShatteredFrontier.SFItemDefinition'
ITEMS = (
    ('DA_BoarHide', 'item/boar_hide', 'Boar Hide'),
)

# Hides stack and weigh like the other pilot materials (one development bag).
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
existing = [path for path in asset_paths if asset_library.does_asset_exist(path)]
if existing:
    raise RuntimeError('Combat pilot asset(s) already exist; refusing overwrite: ' + ', '.join(existing))

item_class = unreal.load_class(None, ITEM_CLASS_PATH)
if item_class is None:
    raise RuntimeError('Could not load ' + ITEM_CLASS_PATH + '; build the ShatteredFrontier editor first.')

factory = unreal.DataAssetFactory()
factory.set_editor_property('data_asset_class', item_class)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
created = []
for asset_name, unique_id, display_name in ITEMS:
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

unreal.log('COMBAT_PILOT_ITEMS_CREATED ' + ', '.join(created))
