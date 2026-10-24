"""Create the raw-material gathering pilot item definitions in the UE editor.

Run after the ShatteredFrontier editor is built. Existing assets
are skipped and never overwritten; only missing definitions are created.
"""
import unreal


OUTPUT_PATH = '/Game/Frontier/Gameplay/Pilot'
ITEM_CLASS_PATH = '/Script/ShatteredFrontier.SFItemDefinition'
ITEMS = (
    ('DA_OakLog', 'item/oak_log', 'Oak Log'),
    ('DA_IronOre', 'item/iron_ore', 'Iron Ore'),
    ('DA_RoughStone', 'item/rough_stone', 'Rough Stone'),  # tier-one mining stone (Crafting Gathering, mining row)
)
settings_class = unreal.load_class(None, '/Script/ShatteredFrontier.SFGatheringPilotSettings')
if settings_class is None:
    raise RuntimeError('Build the gathering pilot settings before generating item assets.')
settings = unreal.get_default_object(settings_class)
MAX_STACK_SIZE = settings.get_editor_property('MaterialStackUnits')
WEIGHT = settings.get_editor_property('MaterialUnitWeightKg')
if not 1 <= MAX_STACK_SIZE <= 100 or not 0.01 <= WEIGHT <= 50:
    raise RuntimeError('Pilot material configuration outside declared bounds.')

asset_library = unreal.EditorAssetLibrary
asset_paths = [OUTPUT_PATH + '/' + name for name, _, _ in ITEMS]
existing = [path for path in asset_paths if asset_library.does_asset_exist(path)]
missing = [item for item, path in zip(ITEMS, asset_paths) if path not in existing]

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
    asset.set_editor_property('max_stack_size', MAX_STACK_SIZE)
    asset.set_editor_property('weight', WEIGHT)
    if not asset_library.save_loaded_asset(asset):
        raise RuntimeError('Could not save ' + OUTPUT_PATH + '/' + asset_name)
    created.append(asset.get_path_name())

unreal.log('GATHERING_PILOT_ITEMS_CREATED ' + (', '.join(created) or 'none') + '; kept ' + (', '.join(existing) or 'none'))
