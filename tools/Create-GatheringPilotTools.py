"""Create session-only starter gathering tools in the UE editor."""
import math

import unreal


OUTPUT_PATH = '/Game/Frontier/Gameplay/Pilot'
ITEM_CLASS_PATH = '/Script/ShatteredFrontier.SFItemDefinition'
ITEMS = (
    ('DA_IronAxe', 'item/iron_axe', 'Iron Axe (pilot)'),
    ('DA_IronPickaxe', 'item/iron_pickaxe', 'Iron Pickaxe (pilot)'),
)

asset_library = unreal.EditorAssetLibrary
asset_paths = [OUTPUT_PATH + '/' + name for name, _, _ in ITEMS]
existing = [path for path in asset_paths if asset_library.does_asset_exist(path)]
if existing:
    raise RuntimeError('Gathering pilot tool asset(s) already exist; refusing overwrite: ' + ', '.join(existing))

settings_class = unreal.load_class(None, '/Script/ShatteredFrontier.SFGatheringPilotSettings')
if settings_class is None:
    raise RuntimeError('Build the gathering pilot settings before generating tool assets.')
weight = unreal.get_default_object(settings_class).get_editor_property('ToolUnitWeightKg')
if not math.isfinite(weight) or not 0.01 <= weight <= 50:
    raise RuntimeError('Pilot tool weight outside declared bounds.')

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
    asset.set_editor_property('UniqueId', unique_id)
    asset.set_editor_property('DisplayName', unreal.Text(display_name))
    asset.set_editor_property('Description', unreal.Text(
        'Development starter tool for the gathering pilot. Session-only; no durable progression, stats, wear, or repair behavior is implied.'
    ))
    asset.set_editor_property('MaxStackSize', 1)
    asset.set_editor_property('Weight', weight)
    asset.set_editor_property('UseType', unreal.SFItemUseType.EQUIP)
    asset.set_editor_property('EquipmentSlot', 'MainHand')  # tools are main-hand items (Equipment window)
    if not asset_library.save_loaded_asset(asset):
        raise RuntimeError('Could not save ' + OUTPUT_PATH + '/' + asset_name)
    created.append(asset.get_path_name())

unreal.log('GATHERING_PILOT_TOOLS_CREATED ' + ', '.join(created))
