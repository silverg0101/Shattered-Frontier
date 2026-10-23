"""Create the shared, HUD and unit-frame Widget Blueprints (WBP_*) under /Game/Frontier/UI/Widgets/.

Run in the open editor:   py -3 tools/Run-InEditor.py tools/ui/create_widgets.py [--force] [--only WBP_Name]
Or headless:   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -unattended -nosplash (no argv there: set env SF_UI_FORCE=1 and SF_UI_ONLY=WBP_Name instead of the flags)
Shared menu kit v7 (Common: WBP_WindowFrame, WBP_Button, WBP_Tab, WBP_TabBar, WBP_ItemSlot, WBP_Separator, WBP_ScrollList, WBP_GoldDisplay,
WBP_SearchField, WBP_InputField, WBP_Dropdown, WBP_Stepper) and WBP_Inventory (Inventory/, built on the kit) need tools/ui/import_kit_v7.py and
tools/ui/import_inventory.py (tooltip and weight-bar art) run first. Rebuild the kit with
SF_UI_FORCE=1 SF_UI_ONLY=WBP_Separator,WBP_Button,WBP_Tab,WBP_TabBar,WBP_ItemSlot,WBP_ItemTile,WBP_ScrollList,WBP_GoldDisplay,WBP_SearchField,WBP_InputField,WBP_Dropdown,WBP_Stepper,WBP_WindowFrame,WBP_KitRow,WBP_Toggle,WBP_SettingRow,WBP_Inventory,WBP_MarketBoard,WBP_TradeWindow,WBP_StationPanel,WBP_BuildCatalogue,WBP_QuestBoard,WBP_Dialogue,WBP_GatheringSkills,WBP_Settings,WBP_GameMenu,WBP_Defeated
(SF_UI_ONLY takes one name or a comma list; a forced rebuild discards Designer edits of those assets).

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
only = sys.argv[sys.argv.index('--only') + 1] if '--only' in sys.argv else os.environ.get('SF_UI_ONLY', '')  # one WBP name or a comma list
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
