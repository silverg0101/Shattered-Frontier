"""Create missing Greenlands gear and material definitions in this checkout's editor.

Run with tools/Run-InEditor.py or UnrealEditor-Cmd -run=pythonscript.
UniqueId is the link to content/items/gear_greenlands_starter.json stats in SFGear.
Existing definitions are validated and preserved, except generated fallback names
which are updated to handoff display names. No mesh or size field exists on
USFItemDefinition, so forge-handoff.json size_cm remains a model handoff only.
"""
import json
from pathlib import Path

import unreal


root = Path(unreal.Paths.project_dir()).parent
gear = json.loads((root / 'content/items/gear_greenlands_starter.json').read_text())['gear']
materials = json.loads((root / 'content/items/materials_greenlands_drops.json').read_text())['materials']
handoff = {item['item_id']: item['display_name'] for item in
           json.loads((root / 'content/items/forge-handoff.json').read_text())['items']}
folder = '/Game/Frontier/Gameplay/Pilot'
library = unreal.EditorAssetLibrary
klass = unreal.load_class(None, '/Script/ShatteredFrontier.SFItemDefinition')
if klass is None:
    raise RuntimeError('SFItemDefinition class unavailable; build the editor first')
factory = unreal.DataAssetFactory()
factory.set_editor_property('data_asset_class', klass)
tools = unreal.AssetToolsHelpers.get_asset_tools()


def asset_name(item_id):
    if not item_id.startswith('item/') or '/' in item_id[5:]:
        raise RuntimeError('Unexpected Greenlands item id: ' + item_id)
    return 'DA_' + ''.join(word.title() for word in item_id[5:].split('_'))


def placeholder(item_id, slot):
    stem = {
        'Head': 'hide_cap', 'Chest': 'hide_jerkin', 'Hands': 'hide_gloves',
        'Legs': 'hide_leggings', 'Feet': 'hide_boots', 'Back': 'hide_cloak',
        'Ring': 'iron_ring', 'OffHand': 'iron_buckler', 'MainHand': 'iron_shortsword',
    }.get(slot, 'boar_hide')
    icon = library.load_asset('/Game/Frontier/UI/Icons/T_Icon_' + stem)
    if icon is None:
        raise RuntimeError('Missing existing icon placeholder: ' + stem)
    return icon


items = {entry['definition_id']: (entry, True) for entry in gear}
for entry in materials:
    item_id = entry['definition_id']
    if item_id in items:
        raise RuntimeError('Duplicate item id: ' + item_id)
    items[item_id] = (entry, False)

# Validate the complete input and placeholders before creating any assets.
for item_id, (entry, is_gear) in items.items():
    path = folder + '/' + asset_name(item_id)
    if library.does_asset_exist(path):
        existing = library.load_asset(path)
        if existing is None or existing.get_editor_property('UniqueId') != item_id:
            raise RuntimeError('Existing definition id mismatch: ' + path)
    else:
        placeholder(item_id, entry['slot'] if is_gear else None)

created = []
renamed = []
for item_id, (entry, is_gear) in sorted(items.items()):
    name = asset_name(item_id)
    path = folder + '/' + name
    if library.does_asset_exist(path):
        if item_id in handoff:
            asset = library.load_asset(path)
            fallback = item_id[5:].replace('_', ' ').title()
            if str(asset.get_editor_property('DisplayName')) == fallback and handoff[item_id] != fallback:
                asset.set_editor_property('DisplayName', unreal.Text(handoff[item_id]))
                if not library.save_loaded_asset(asset):
                    raise RuntimeError('Could not save ' + path)
                renamed.append(path)
        if is_gear:
            asset = library.load_asset(path)
            category = getattr(unreal.SFEquipmentCategory, entry['category'].upper())
            stats = [f"+{entry[key]} {key.title()}" for key in ('armor', 'damage') if entry[key]]
            description = ', '.join(stats) + '.'
            if asset.get_editor_property('EquipmentCategory') != category or str(asset.get_editor_property('Description')) != description:
                asset.set_editor_property('EquipmentCategory', category)
                asset.set_editor_property('Description', unreal.Text(description))
                if not library.save_loaded_asset(asset):
                    raise RuntimeError('Could not save ' + path)
                renamed.append(path)
        continue
    asset = tools.create_asset(name, folder, klass, factory)
    if asset is None:
        raise RuntimeError('Could not create ' + path)
    asset.set_editor_property('UniqueId', item_id)
    asset.set_editor_property('DisplayName', unreal.Text(entry.get('display_name', handoff.get(item_id, item_id[5:].replace('_', ' ').title()))))
    asset.set_editor_property('MaxStackSize', 1 if is_gear else entry['max_stack'])
    asset.set_editor_property('Weight', entry['weight_kg'])
    asset.set_editor_property('Icon', placeholder(item_id, entry['slot'] if is_gear else None))
    if is_gear:
        asset.set_editor_property('UseType', unreal.SFItemUseType.EQUIP)
        asset.set_editor_property('EquipmentSlot', entry['slot'])
        asset.set_editor_property('EquipmentCategory', getattr(unreal.SFEquipmentCategory, entry['category'].upper()))
        stats = [f"+{entry[key]} {key.title()}" for key in ('armor', 'damage') if entry[key]]
        asset.set_editor_property('Description', unreal.Text(', '.join(stats) + '.'))
    if not library.save_loaded_asset(asset):
        raise RuntimeError('Could not save ' + path)
    created.append(path)

for item_id, (entry, is_gear) in sorted(items.items()):
    path = folder + '/' + asset_name(item_id)
    asset = library.load_asset(path)
    if (asset is None or asset.get_editor_property('UniqueId') != item_id or
            not str(asset.get_editor_property('DisplayName')) or
            str(asset.get_editor_property('Icon')) in ('None', '')):
        raise RuntimeError('Incomplete item definition: ' + path)
    if is_gear and str(asset.get_editor_property('EquipmentSlot')) != entry['slot']:
        raise RuntimeError('Equipment slot mismatch: ' + path)
    if is_gear and asset.get_editor_property('EquipmentCategory') != getattr(unreal.SFEquipmentCategory, entry['category'].upper()):
        raise RuntimeError('Equipment category mismatch: ' + path)

unreal.log(f'GREENLANDS_DROP_DEFINITIONS Total={len(items)} Created={len(created)} Renamed={len(renamed)} Paths={",".join(created + renamed)}')
