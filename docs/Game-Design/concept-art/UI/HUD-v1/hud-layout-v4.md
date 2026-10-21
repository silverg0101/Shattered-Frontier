# HUD-v1 canonical layout (v4)
Source: Fable UX review, 2026-10-21. All screens follow this; a window that needs space shrinks, it never relocates the HUD.

| Element | X % | Y % | Anchor |
|---|---|---|---|
| Vitals (stag hex badge + level, HP crimson, stamina amber, numerals inside) | 2-27 | 86-96 | bottom-left |
| Hotbar, 6 slots, number top-left, count bottom-right | 33-67 | 86-96 | bottom-center |
| Action/prompt bar (build Place/Rotate/Remove/Cancel; interact; gamepad glyph swap) | 32-68 | 79-85 | bottom-center |
| Target frame | 38-62 | 4-11 | top-center |
| Minimap (round, N up, location + distance beneath) | 84-99 | 2-24 | top-right |
| Quest tracker, max 3 objectives | 84-99 | 25-40 | top-right |
| Toasts, newest on top, max 3, 5% tall, icon + two lines | 84-99 | 42-62 | top-right |
| Gathering ring | world-projected | - | - |
| Left context panel (build catalogue, left-docked stations) | 2-26 | 8-72 | left |
| Build cost/status (have/need + valid state) | 2-26 | 73-84 | bottom-left |
| Inventory | 6-50 | 7-83 | left |
| Right station panel (sawmill, recipes) | 30-82 | 18-85 | center-right |
| Center windows (trade, defeated) | 26-74 | 14-78 | center |
| Modal boards (market): dim scene, right column hidden | 12-88 | 16-87 | center |

2% safe inset; UMG canvas anchors; DPI scaled by height, 1080 reference.

Rules
- Right column (84-99%) is reserved for minimap, quests and toasts; only modal boards may cover it.
- Stamina is always amber; never blue. No compass strip; no top-left title/zone text (zone goes under the minimap).
- Buttons are brass 9-slice; moss is a state colour, not a fill.
- Have/need shows "have / need" with tick or cross glyph and a crimson underline on the short number; never colour alone. Placement validity is always tick/cross plus text.
- Hide/show toggles live in Settings, not on-screen chevrons.
- Controller: tab rows carry LB/RB glyphs, action bar A/B/X/Y; Buy, Accept trade and Respawn are hold-to-confirm.
- Art kit: slate 9-slice frame + chamfer corner overlays, one tiling engraved divider, stag badge brush, 3-slice hex end-caps (target, vitals), circular minimap mask material.

Screen verdicts
- hud-gameplay: superseded by hud-gameplay-v4 (adds level badge, toast stack).
- hud-inventory: use; recenter hotbar to 33-67%, keep right column visible.
- hud-gathering-crafting: use with changes; restore canonical minimap/toast positions, no compass, brass Craft button, station panel ends at 82%.
- hud-construction / hud-build-*-v3: superseded by hud-construction-v4 (v3 catalogue content in the left column). Blueprint tab and collapsed states reuse that shell. Drop v3's top nav bar, top-left vitals and right-side catalogue.
- hud-trade: use the window; render with the canonical HUD (6-slot hotbar, amber stamina, no compass/title).
- hud-market: use.
- hud-toast: superseded by hud-gameplay-v4.
- hud-recipe: use with changes; panel 30-82%, have/need glyphs, amber stamina, no title overlay.
- hud-defeated: use the panel; dim the canonical HUD behind it.

Top UX issues (ranked)
1. Persistent elements wander between screens (vitals, minimap, hotbar size, stamina colour).
2. Toasts had no home.
3. Have/need and placement validity were colour-only.
4. v3 build mode costs too much view (nav bar, three side panels, small grid).
5. No controller affordances or hold-to-confirm on irreversible actions.

## v4 prompts

### hud-gameplay-v4.png

Use case: ui-mockup. Create exactly one 16:9 full-screen in-game screenshot mockup for Legends: Shattered Frontier, a shipped-quality fantasy survival MMO. Third-person camera behind a leather-and-cloth adventurer on a Greenlands forest trail near Silverbrook, pine and broadleaf trees, mossy stone, river town and stone bridge in the distance, natural daylight, AAA realistic rendering. HUD family: translucent deep slate-navy #17232A panels, fine aged-brass #A98A55 frames with chamfered corners, warm ivory #E8E1CE text, moss #688267 state accent, deep crimson #A34842 health, muted amber #B69A55 stamina, tiny pale-stag mark, fantasy serif headings, clean sans numerals. Lower-left: hex stag badge with level "18", health bar "424 / 424", stamina bar "150 / 150". Bottom center: six numbered hotbar slots 1-6 with axe, pickaxe, sword, bandage x5, potion x2, campfire x3. Directly above the hotbar a slim prompt bar "E Interact". Top center: target frame "Wild Boar" level 6 with health "320 / 320", boar on the trail. Upper right: round minimap with N/E/S/W and "Silverbrook · 227 m" beneath. Below it a quest tracker "Silverbrook" with three objectives "Speak to the Millwarden", "Gather 3 Iron Ore 0/3", "Clear the Boar Den 0/1". Below the quest tracker a compact stack of three narrow toasts the same width as the tracker, each a square icon plus two short lines: "Item gained / Oak Logs x4", "Skill gain / Woodcutting +1", "Warning / Inventory nearly full" with amber triangle icon, newest brightest. Nothing else on screen, world dominant, text short and correctly spelled. No compass strip, no title text, no giant ornament, cartoon, neon, bloom, watermark or concept-sheet framing.

### hud-construction-v4.png

Use case: ui-mockup. Create exactly one 16:9 full-screen in-game screenshot mockup for Legends: Shattered Frontier, a shipped-quality fantasy survival MMO. Third-person camera behind a leather-and-cloth adventurer in a riverside clearing outside Silverbrook, Greenlands forest, timber-and-stone town across the river, natural daylight, AAA realistic rendering. HUD family: translucent deep slate-navy #17232A panels, aged-brass #A98A55 chamfered frames, warm ivory #E8E1CE text, moss #688267 valid accent, crimson #A34842 and amber #B69A55 vitals, pale-stag mark, fantasy serif titles, clean sans numerals. Left side, from 8% to 72% of screen height and about a quarter of screen width: panel titled "Build Catalogue" with a search field "Search objects…", two tabs "Voxel Objects" (selected) and "Blueprints", a slim vertical category rail "Structures, Decor, Terrain, Favourites", then a 3x4 grid of thumbnails with labels "Timber Block, Half Slab, Stone Block, Wall, Floor, Door, Stairs, Roof, Pillar, Fence, Gate, Window", "Wall" selected with brighter brass edge, scrollbar at right, and a "Material: Rough Timber" dropdown at the bottom. Below the catalogue, above the lower-left vitals, a compact cost box: "Rough Timber 8 / 12" with a small cross glyph and crimson underline on 8, "Iron Ingot 2 / 2" with a moss tick, and a status line with tick "Valid placement". In the world a translucent pale-green timber wall ghost snapped to a rough-timber foundation with faint grid and snap points, human scale. Bottom center: a slim action bar with glyphs "LMB Place · R Rotate · X Remove · Esc Cancel" directly above six numbered hotbar slots 1-6. Lower-left hex stag badge level "18", health "320 / 320", stamina "140 / 140". Upper right: round minimap with "Silverbrook · 227 m" beneath, quest tracker "Silverbrook" with three short objectives, and one narrow toast beneath it "Item gained / Rough Timber x4". No top navigation bar, no compass strip, no right-side catalogue. World dominant, text short and correctly spelled. No giant ornament, cartoon, neon, bloom, watermark or concept-sheet framing.
