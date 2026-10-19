"""Create the two raw-material gathering pilot item definitions in the UE editor.

Run only after MinimalInventoryRuntime is installed and compiled. Existing assets
are treated as a hard error so this script never overwrites user content.
"""
import unreal


OUTPUT_PATH = '/Game/Frontier/Gameplay/Pilot'
ITEM_CLASS_PATH = '/Script/MinimalInventoryRuntime.InventoryItemDefinition'
ITEMS = (
    ('DA_OakLog', 'item/oak_log', 'Oak Log'),
    ('DA_IronOre', 'item/iron_ore', 'Iron Ore'),
)
settings = unreal.get_default_object(unreal.SFGatheringPilotSettings)
MAX_STACK_SIZE = settings.get_editor_property('material_stack_units')
WEIGHT = settings.get_editor_property('material_unit_weight_kg')
if not 1 <= MAX_STACK_SIZE <= 100 or not 0.01 <= WEIGHT <= 50:
    raise RuntimeError('Pilot material configuration outside declared bounds.')

asset_library = unreal.EditorAssetLibrary
asset_paths = [OUTPUT_PATH + '/' + name for name, _, _ in ITEMS]
existing = [path for path in asset_paths if asset_library.does_asset_exist(path)]
if existing:
    raise RuntimeError('Gathering pilot asset(s) already exist; refusing overwrite: ' + ', '.join(existing))

item_class = unreal.load_class(None, ITEM_CLASS_PATH)
if item_class is None:
    raise RuntimeError('Could not load ' + ITEM_CLASS_PATH + '; enable and compile MinimalInventoryRuntime first.')

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
    asset.set_editor_property('max_stack_size', MAX_STACK_SIZE)
    asset.set_editor_property('weight', WEIGHT)
    if not asset_library.save_loaded_asset(asset):
        raise RuntimeError('Could not save ' + OUTPUT_PATH + '/' + asset_name)
    created.append(asset.get_path_name())

unreal.log('GATHERING_PILOT_ITEMS_CREATED ' + ', '.join(created))
