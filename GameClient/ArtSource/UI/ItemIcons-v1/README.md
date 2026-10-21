# Item Icons v1

Art source for the 15 currently enumerated gameplay item IDs. Each item has a centered, transparent RGBA PNG at 256 × 256 and a 64 × 64 slot preview. `manifest.json` maps each exact `item/...` definition ID to both files. The 256 px file is the intended Unreal import source; the 64 px file is a UI review/preview derivative. No Unreal assets were imported or widget bindings changed.

## Editor import list

Import these 256 px PNGs as UI textures under a chosen `/Game/Frontier/UI/ItemIcons/` path, then bind each to its manifest ID:

- `boar_hide_256.png`
- `door_material_bundle_256.png`
- `foundation_material_bundle_256.png`
- `hide_strips_256.png`
- `iron_axe_256.png`
- `iron_ingot_256.png`
- `iron_nails_256.png`
- `iron_ore_256.png`
- `iron_pickaxe_256.png`
- `oak_log_256.png`
- `roof_material_bundle_256.png`
- `rough_stone_256.png`
- `rough_timber_256.png`
- `starter_tool_repair_kit_256.png`
- `wall_material_bundle_256.png`

The eight missing items received new generated art. Seven existing `UI/Icons-v1` images were extracted from their slate background and resized here; the old source files remain intact. The inventory covers `content/crafting/recipes_greenlands_starter.json`, `content/quests/greenlands_frontier_founder.json`, `content/loot/greenlands_starter.json`, and the filename-level `Gameplay/Pilot/DA_*.uasset` set. The build-catalogue icons in `Icons-v1` are structure controls, not item definitions. No speculative bandage, potion, campfire, sword, or bow IDs were added.

## Validation

Run `py validate_icons.py` in this folder to verify all 30 RGBA exports, including an outside flood-fill assertion that no icon has enclosed transparent pixels, and rebuild `icons-on-slate.png`. The contact sheet shows every 256 px icon on the HUD's `#17232A` slate. No runtime test applies to this art-only packet.

## Matting repair

`boar_hide`, `iron_axe`, `iron_ingot`, `iron_nails`, `iron_ore`, `iron_pickaxe`, `oak_log`, and `rough_timber` were regenerated against pure magenta chroma, keyed from border-connected background, despilled, feathered by about 1 px, and exported at both sizes. Interior alpha holes were filled from the outside mask; `hide_strips_64.png` also needed a small resize-hole fill. The original art remains in `UI/Icons-v1` for provenance. The repaired 256 px icons have no visible magenta spill on slate. This is an art-source packet; editor import and HUD binding are still pending.
