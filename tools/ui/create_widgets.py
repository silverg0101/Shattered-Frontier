"""Create the shared, HUD and unit-frame Widget Blueprints (WBP_*) under /Game/Frontier/UI/Widgets/.

Run in the open editor:   py -3 tools/Run-InEditor.py tools/ui/create_widgets.py [--force] [--only WBP_Name]
Or headless:   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -unattended -nosplash (no argv there: set env SF_UI_FORCE=1 and SF_UI_ONLY=WBP_Name instead of the flags)
Inventory (UI/Widgets/Inventory: WBP_InventoryTab, WBP_InventorySlot, WBP_Inventory) needs tools/ui/import_inventory.py run first; rebuild the panel with SF_UI_FORCE=1 SF_UI_ONLY=WBP_Inventory.

Each WBP gets a first-draft layout and the BindWidget names its C++ base expects (Public/UI/SFUiWidgets.h, SFHudWidgets.h;
the layouts are in Private/UI/SFWidgetAuthoring.cpp). Existing assets are skipped: the owner rearranges them in the UMG
Designer, and --force rebuilds them from the draft and DISCARDS those edits (--only limits the run to one WBP).
Unit frames (UI/Widgets/Unit): WBP_UnitFrame is the owner's hand-laid layout (duplicated once from the retired WBP_HudVitals, then given
the BadgeRoot/StaminaRow/NameText parts); WBP_Player/Target/Party/RaidFrame are its children and differ only in the part toggles.
WBP_Hud is migrated in place (Vitals -> WBP_PlayerFrame in the same slot, plus Target, PartyList, RaidGrid). WBP_HudVitals is deleted
once nothing references it. Re-running changes nothing that already exists. Needs tools/ui/import_hud_vitals.py run first on a fresh
checkout (its textures and bar-fill materials) and the C++ module built (SFWidgetAuthoring).
"""
import sys

import unreal

import os
force = '--force' in sys.argv or os.environ.get('SF_UI_FORCE') == '1'  # headless runs get no argv: use SF_UI_FORCE=1 / SF_UI_ONLY=<WBP name>
only = sys.argv[sys.argv.index('--only') + 1] if '--only' in sys.argv else os.environ.get('SF_UI_ONLY', '')
lib = unreal.EditorAssetLibrary
OLD = '/Game/Frontier/UI/Widgets/Hud/WBP_HudVitals'
FRAME = '/Game/Frontier/UI/Widgets/Unit/WBP_UnitFrame'

if not lib.does_asset_exist(FRAME) and lib.does_asset_exist(OLD):
    print('create_widgets: WBP_UnitFrame <- duplicate of the owner layout WBP_HudVitals')
    if not lib.duplicate_asset(OLD, FRAME):
        raise RuntimeError('create_widgets: could not duplicate ' + OLD)
    lib.save_asset(FRAME, only_if_is_dirty=False)

report = unreal.SFWidgetAuthoring.build_all(force, only)
print(report)

if lib.does_asset_exist(OLD) and lib.does_asset_exist(FRAME) and 'FAIL' not in report:
    refs = [r for r in lib.find_package_referencers_for_asset(OLD, load_assets_to_confirm=False) if not str(r).endswith('WBP_HudVitals')]
    if refs:
        print('create_widgets: WBP_HudVitals kept, referenced by', refs)
    else:
        lib.delete_asset(OLD)
        print('create_widgets: deleted unreferenced WBP_HudVitals')
if 'FAIL' in report:
    raise RuntimeError('create_widgets: at least one widget blueprint failed (see log above)')
