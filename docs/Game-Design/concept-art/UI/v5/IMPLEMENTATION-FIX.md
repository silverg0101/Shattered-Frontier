# UI v5 — implementation fix spec

Approver: Fable UX, 2026-10-21. Inputs: the five approved concepts in this folder, `REVIEW.md` family rules, `kit-v5-hud-mock.png`, `GameClient/ArtSource/UI/Kit-v5/kit.json`, the live captures in `docs/evidence/ui-v5/`, and the widget code under `GameClient/Source/ShatteredFrontier`. All px are 1080p design px (concept images are 1672x941, x1.148). "2x px" means pixels of the Kit-v5 PNGs.

## Root cause

1. **9-slice corners draw at 2x.** Kit-v5 PNGs are 2x; `tools/Import-HudKit.py` imports them unscaled and sets `DrawScale = 0.5`. `SFHudStyle::KitBrush` applies that scale only to `ImageSize`. Slate's box batcher ignores `ImageSize`: `Engine/Source/Runtime/SlateCore/Private/Rendering/ElementBatcher.cpp:857` computes corner size as `ActualSize * Margin`, so every corner, edge band and inset is drawn at twice its design size. A 56 px button gets 28+28 px of top/bottom corner and no middle band; a 44 px palette button is corner art only, which is why its labels are illegible. The hotbar slot's 17.5 px studs draw at 35 px.
2. **The margins in `kit.json` are smaller than the baked ornaments.** `panel_frame`, `window_frame`, `modal_frame` and the card/tracker frames have a corner flourish that trails 70-103 px (1x) down the left and right edges, but `nine_slice_px` is `[36,28,36,28]`. Everything beyond the margin lives in the stretch band, so the flourish is scaled with the panel height. Combined with fix 1 this produces the 90 px smeared corners on the live BUILDING panel. Slot studs (22-25 px at 1x) also exceed their 17-18 px margins.
3. **Wrong pieces on the wrong widgets.** `slot_empty` (bag slot with four studs) is the market row and the text-field background; `tooltip_frame` is the prompt bar, the trade columns and the sell panel; the window frames carry ornaments meant only for windows. Hotbar cells, tabs and buttons are `Fill`-sized instead of fixed px.
4. **The widgets are earlier designs, not the v5 concepts.** `USFConstructionSandboxPalette` is the dev palette (BUILDING / Development palette / Single-Line-Demolish...), not the Build Catalogue. `USFMarketBoard` and `USFTradeWindow` are the HUD-v1 demo layouts (filter buttons + Buy rows; text-only offer columns). `USFInventoryPanel` is forced into a 480x560 viewport slot that clips the bag grid under the panel. `USFQuestTrackerHud` and the vitals plate are procedural boxes. Titles use Cinzel (caps) at 24-28 px; body text runs 11-15 px against a 16 px minimum.
5. **The capture is the wrong session.** `gameplay-hud-live.png` is the construction-network client (dev palette open, mannequin pawn, flat test plot, harsh low sun), so there is no E1 character (no vitals), no quest pilot (no tracker), no gathering node (no target frame), and the minimap ring was not yet in `DA_HudKit`. The OS window (1937x1119 with title bar) was screenshotted instead of the 1920x1080 viewport.

Why `kit-v5-hud-mock.png` matches: `preview.py` resizes every piece to its intended size with Pillow (`asset(name,(w,h))`), i.e. a near-uniform 0.5x scale with no 9-slice. It proves the art at design size; it never exercised Slate's margins.

## Global fixes

### G1. Import the kit at 1x (`tools/Import-HudKit.py`, Kit-v5 block)

- Before `AssetImportTask`, resample each Kit-v5 PNG to `intended_1080p_px` (= PNG size / 2) with `Image.LANCZOS` into `GameClient/ArtSource/UI/Kit-v5/processed/<name>.png` (git-ignored like HUD-Kit-v1/processed). Import that file.
- `margin = nine_slice_px / 2 / texture_1x_size` (numerically the same fraction as today). `inset_pixels = nine_slice_px.left / 2`. `draw_scale = 1.0`.
- `SFHudStyle::KitBrush`: `ImageSize = texture size` (DrawScale stays as a field but is 1). Pieces drawn at a fixed size (`hex_level_badge`, `minimap_ring`, `keycap_*`, `checkbox`, `close_x`, `status_tick/cross`, `hold_confirm_ring`, `icon_*`, `portrait_well`, `compass_*`) use `DrawAs = Image` via `SFScreenArt::KitImage`, never `Box`.
- Scene art (`paperdoll_*`, `ghost_*`) and icons keep the 2x texture: they are drawn by `KitImage(size)`, which honours the size.
- Keep the 2x PNGs as source for a later 4K kit; Slate has no per-brush resolution scale for box brushes, so a 1x texture is the only correct input.

### G2. Margins that cover the ornament (`kit.json` `nine_slice_px`, 2x px; measured 2026-10-21)

| piece | current | set to | 1x corner | min widget size (1x) |
|---|---|---|---|---|
| panel_frame | 36,28,36,28 | 36,140,36,140 | 18 x 70 | 36 x 140 |
| window_frame | 36,28,36,28 | 36,176,36,176 | 18 x 88 | 36 x 176 |
| modal_frame | 36,28,36,28 | 36,206,36,206 | 18 x 103 | 36 x 206 |
| tooltip_frame | 36,28,36,28 | 36,86,36,86 | 18 x 43 | 36 x 86 |
| detail_card_frame | 36,28,36,28 | 36,142,36,142 | 18 x 71 | 36 x 142 |
| quest_tracker_frame | 36,28,36,28 | 36,76,36,76 | 18 x 38 | 36 x 76 |
| target_frame | 36,28,36,28 | 36,44,36,44 | 18 x 22 | 36 x 44 |
| toast_frame | 54,28,36,28 | 68,28,36,28 | 34 x 14 | always 280 x 66 |
| button_normal / hover / pressed / disabled / destructive | 40,28,40,28 | 40,40,40,40 | 20 x 20 | 40 x 40 |
| button_primary_brass | 40,28,40,28 | 56,48,56,48 | 28 x 24 | 56 x 48 |
| tab_normal / tab_selected | 36,22,36,22 | 36,30,36,30 | 18 x 15 | 36 x 30 |
| search_field | 36,18,36,18 | 36,26,36,26 | 18 x 13 | 36 x 26 |
| dropdown_field | 36,20,36,20 | 36,28,36,28 | 18 x 14 | 36 x 28 |
| action_bar | 48,32,48,32 | 52,44,52,44 | 26 x 22 | 52 x 44 |
| slot_hotbar_* | 35 | 44 | 22 | 44 |
| slot_catalogue_* | 36 | 50 | 25 | 50 |
| slot_bag_* | 25 | 32 | 16 | 32 |
| slot_equipment_* | 28 | 36 | 18 | 36 |
| minimap_footer | 36,16,36,16 | 36,22,36,22 | 18 x 11 | 36 x 22 |

Rule: a box brush is never drawn smaller than the sum of its margins; widgets size themselves with `USizeBox`, never `ESlateSizeRule::Fill` on a kit brush.

### G3. One button helper (`SFHudStyle`)

Add `SFHudStyle::Button(Tree, Label, EKind {Secondary, Primary, Destructive}, Width)` and use it from `USFConstructionSandboxPalette::StyleButton`, `ExchangeMakeButton` and the defeated/respawn button. Spec: height 56, min width 160, horizontal padding 24, label EB Garamond 24 ivory (on brass: ivory with a 1 px `2A1E10` shadow). Brushes: Secondary = `button_normal` / `button_hover` / `button_pressed` / `button_disabled`; Primary = `button_primary_brass`; Destructive = `button_destructive`. Hold variant: Primary + `hold_confirm_ring` 36 px at x 20 inside the left end, label "Hold to …", arc painted as `USFE1VitalsHud::NativePaint` does (move that arc into a shared `SFHudStyle::PaintHoldArc`). Nothing under 44 px tall is a text button: icon controls use `close_x`, `plus`, `minus`, `keycap_*`. Frames (`panel_`, `window_`, `modal_`, `tooltip_`) are never button backgrounds.

### G4. Type roles (`SFHudStyle::Heading/Text/Num`)

The approved images set titles in a mixed-case serif, not Cinzel capitals. In-game panel, window and modal titles and section headings use EB Garamond Bold at rule-5 sizes (48 window/modal, 36 panel, 26 section); Cinzel stays for the loading-screen zone title and "Defeated" only (both approved with it). Body 20, rows/footers 18, captions and toast labels 16, counts and bar values Source Sans Bold 18-20. Minimum 16 px: every 11-15 px call in the five widgets moves to its role size.

### G5. Layout rules

Anchors in % with fixed px sizes (`SetAnchorsInViewport` + `SetDesiredSizeInViewport`, or a `UCanvasPanel` root with anchored slots), DPI curve by height. Dim overlay behind any window is black 40% (`USFMarketBoard` uses 55%). Every window has `close_x` 40 px top-right inside the frame and closes on Esc/B. Secondary widgets never re-draw HUD elements; the gameplay HUD stays underneath, dimmed.

## Screen tables

Columns: concept spec (position, size, piece, type, colour) | live problem | fix.

### 1. gameplay-hud (master)

| element | concept spec | live problem | fix |
|---|---|---|---|
| Level badge | x2% (38), bottom y96% (1037); 116x116 `hex_level_badge` (Image); numeral Source Sans Bold 22 ivory centred in the lower plate | 58 px box inside a procedural slate plate anchored 2-27% x 86-96%; not shown at all in the capture (no E1 character) | `USFE1VitalsHud::NativeOnInitialized`: drop the Plate border; canvas slot (0.02, 0.96) alignment (0,1), SizeBox 116; level text from the character when it exists |
| HP bar | x 164-544, y 936-968; 380x32 `bar_hp_back/_fill/_frame` (3-slice 27 px ends); numerals Source Sans Bold 20 ivory centred | `MakeFramedBar(380)` exists but is inside the plate row; text 14 px right-aligned | same function: HorizontalBox badge + VerticalBox of bars, bars 10 px apart; text 20 centred over the fill |
| Stamina bar | x 164-544, y 978-1010; 380x32 `bar_stamina_*`, amber fill; numerals as HP | missing | add a second `MakeFramedBar(TEXT("bar_stamina"), 380)`; bind to the character's stamina; until a stamina stat exists draw it full with the settings max and note it in the capture README |
| Target frame | top-centre x 755-1165, y 43-113; 410x70 `target_frame`; left hex level cap 44 px (`hex_level_badge` Image) with Source Sans 18; name EB Garamond 24 ivory; inner HP bar 300x22 `bar_hp_*` with Source Sans 18 numerals | anchored 38-62% x 4-11% (fills 461x76), Cinzel 18 name, 15 px charges, 10 px progress bar; hidden unless a gathering node is aimed | `USFHudLayout::NativeOnInitialized`: SizeBox 410x70, anchor (0.5, 0.04) alignment (0.5, 0); name Text 24; `MakeFramedBar("bar_hp", 300)` for charges/health; show for aimed boars too (same frame, `SFCombatPilot` target) |
| Prompt bar | centred, bottom y85% (918), 52 px tall, auto width (~240); `button_normal` plate; `keycap_f` 40x36 Image + label EB Garamond 24 ivory | `tooltip_frame` stretched to 32-68% x 79-85% (691x65); keycap_blank + 14 px text; 15 px label | `USFHudLayout`: PromptFrame = `KitPanel("button_normal")`, autosize, anchor (0.5, 0.85) alignment (0.5, 1); `SetPrompts`: `keycap_<key>` piece when present (f, esc, r, x, 1-4, lb, rb) else `keycap_blank` + Text 20; action label Text 24 |
| Hotbar | 6 x 88 px `slot_hotbar_normal` (`_selected` for the equipped tool), 12 px gap, 588 wide centred (x 666-1254), bottom y96% (1037); icon 64 centred; slot number top-left inside Source Sans 18; count bottom-right inside 18 | cells `Fill`-sized in a 33-67% x 86-96% box (~108 px, 6 px gap), studs at 2x, number 16 px, no counts, icons `Fill` with 6 px padding | `USFHudLayout::BuildHotbar`: SizeBox 88 per cell, slot padding 6, row anchor (0.5, 0.96) alignment (0.5, 1) autosize; icon 64 via `SFScreenArt::Icon`; add count text; no `action_bar` tray behind the gameplay hotbar (the concept has none) |
| Minimap | 250 px disc, right edge x99% (1901), top y2% (22); `minimap_mask` clips the map render, `minimap_ring` 250 Image over it (ring bakes the N cap), `player_arrow` centre 18 px; disc fill `0E171C` 80% until a map renders | disc 0.19 x H (203 px), ring image in a top-left overlay slot, "N" text 14 px, 9 px diamond marker; ring absent in the capture | `USFHudLayout::PlaceMinimap`: fixed 250; ring slot HAlign/VAlign Fill; remove the N text; marker = `player_arrow` 18 px |
| Minimap footer | `minimap_footer` 180x32 centred under the ring, top y 282; "Silverbrook · 227 m" EB Garamond 18 ivory, one line | 14 px text, 4 px gap, zone only | same column: footer SizeBox 180x32, Text 18, "<zone> · <distance> m" from the tracked objective when one exists |
| Quest tracker | x84-99% (1613-1901, 280 wide), top y 338; `quest_tracker_frame` 280 x (62 + 30/row); title EB Garamond Bold 24 ivory; rows 18 ivory with `checkbox` 18 px and right-aligned Source Sans 18 count | procedural slate box anchored 84-99% x 25-40% (overlaps the footer), Text 16/12/13, 1 px rule, "Step n of m" line; absent in the capture (no quest pilot) | `USFQuestTrackerHud::NativeOnInitialized`: `KitPanel("quest_tracker_frame")`, anchor (0.84, 0.313) alignment (0,0), width 280, autosize height; title 24 Bold; drop StepText; rows = checkbox Image 18 + Text 18 + Num 18 |
| Toasts | 280x66 `toast_frame`, 12 px gap, max 3 newest on top, first top y = tracker bottom + 12; icon 48 in the left well; label 16 brass ("Item gained"), body 20 ivory | `USFGatheringHud` toasts anchored 84-99% x 42-62%, Text 18/17, `toast_card` alias, no icon well | `USFGatheringHud::NativeOnInitialized`: VerticalBox anchored (0.84, 0.313 + tracker height) width 280; each toast `KitPanel("toast_frame")` SizeBox 280x66, icon `SFScreenArt::Icon(stem, 48)`, label Text 16 Brass, body Text 20 |

### 2. build-mode

| element | concept spec | live problem | fix |
|---|---|---|---|
| Catalogue panel | `panel_frame` x2-27% (38-518, 480 wide), y3-82% (32-886); inner padding 24 | dev palette (`USFConstructionSandboxPalette`) sized 24% x content, max 64% H, positioned 16,16; frame corners at 2x and stretched | new `USFBuildCatalogue` (or rebuild the palette class): `KitPanel("panel_frame")` anchored (0.02, 0.03)-(0.27, 0.82) |
| Title row | glyph `icon_structures` 28 + "Build Catalogue" EB Garamond Bold 36 ivory; 2 px `divider` under | Cinzel "BUILDING" 24 + "Development palette" 12 px grey | title Text 36 Bold; remove the subtitle |
| Search | `search_field` 432x36 full inner width, `icon_search` 20, hint "Search objects" 18 `A9A593` | none | `UEditableTextBox` styled with `search_field`, filters tiles by name |
| Tabs | `tab_selected` "Voxel Objects" / `tab_normal` "Blueprints", 210x44 each, 12 px gap, Text 20; LB/RB caps 40x26 when a pad is active | none | two `KitPanel("tab_*")` SizeBox 210x44; Blueprints disabled until blueprints exist |
| Category rail | 4 tiles 80x80 stacked at the panel's left (x 54-134), 12 px gap: Structures (selected = `tab_selected` tint + brass edge), Decor, Terrain, Favourites; glyph 36 (`icon_structures/decor/terrain/favourites`) + caption 16 | none | VerticalBox of `KitPanel("tab_normal"/"tab_selected")` 80x80; only Structures populated today, others disabled |
| Tile grid | 3 cols x 4 rows `slot_catalogue_normal` 100x100 (`_selected` brass edge for Wall), 12 px gap, origin x 146 y 214; thumbnail 64 centred top, caption EB Garamond 16 bottom inside | 4 tiles 56 px icon + 11 px label in a `UniformGrid` of 2x-margin buttons; selection = procedural frame | `UUniformGridPanel` of SizeBox 100 buttons styled with `slot_catalogue_*`; captions Foundation/Wall/Door/Roof (the 4 real pieces fill row 1; remaining cells stay empty slots); keys 1-4 select as today |
| Material row | `dropdown_field` 432x40, swatch 28, "Material:" 18 brass + "Rough Timber" 18 ivory, chevron right | none | `KitPanel("dropdown_field")`; single option until materials exist |
| Required materials | inset slate box (`0E171C` 85%, `3A4348` 1 px) 432 wide; heading Text 20 "Required Materials"; rows 18: icon 24 + name + "have / need" Source Sans 18 with `cost_short_underline` under the short number + `status_cross`/`status_tick` 18 at row end; "Valid placement" = `status_tick` + Text 18 | "Cost  None - unlimited dev stock" 13 px | rows from the placement cost model when it exists; until then one row "Unlimited dev stock" with a tick, and the validity line from the ghost's placement result |
| Action bar | `action_bar` 52 tall, auto width (~470), centred, bottom y85% (918): `mouse_lmb` 26 "Place" · `keycap_r` "Rotate" · `keycap_x` "Remove" · `keycap_esc` "Cancel"; caps 36, labels Text 20, separators brass "·" | `USFHudLayout` prompt bar draws this as the tooltip-frame prompt with 14 px text; the palette duplicates every action as a button | prompt bar (gameplay table) uses `action_bar` when the context is Build; the Single/Line/Demolish/Rotate/Surface/Layer/Confirm/Save/Load buttons leave the panel (their keys stay: L, X, R, P, PgUp/PgDn, F5/F9) |
| Hotbar durability | slots 1-2 show a 4 px moss bar at the bottom inside | none | optional `bar_xp_fill` strip 64x4 in the hotbar cell when the tool reports durability |
| Master HUD | vitals, minimap, tracker, toast as table 1 | none present (construction-network client) | capture from the E1 session (protocol below) |

### 3. inventory

| element | concept spec | live problem | fix |
|---|---|---|---|
| Panel | `panel_frame` x4-34% (77-653, 576 wide), y3-85% (32-918); padding 24 | 480x560 viewport slot at 6%,7% (`ToggleInventoryPanel`), bag grid and tabs overflow below the frame; overlaps the dev palette | `ASFE1PlayerController::ToggleInventoryPanel`: `SetAnchorsInViewport(0.04,0.03)`, `SetDesiredSizeInViewport(576, 886)`; root `KitPanel("panel_frame")` |
| Title row | ornament glyph 24 + "Inventory" EB Garamond Bold 36 ivory; 2 px `divider`; `close_x` 40 top-right (Esc/I close) | 22 px rotated diamond + Cinzel "INVENTORY" 28 + "I to close" 12 px | Text 36 Bold; `close_x` Image 40; drop the hint |
| Equipment | left column 5 x `slot_equipment_normal` 72 (12 gap) at x 109; right column 4 at x 485; main hand `slot_equipment_selected`; ghost icons 48 | correct pieces, studs at 2x | margins per G2; no code change beyond G1/G2 |
| Paperdoll | `paperdoll_adventurer` 220x396 centred between the columns, y 118 | correct | none |
| Sort / filter row | under the heading per REVIEW: "Sort" `button_normal` 120x44 + tabs All / Tools / Materials / Food `tab_*` 100x44, Text 18 | tabs rendered under the panel (overflow), 14 px | move the Tabs row above the grid inside the panel; `tab_*` SizeBox 100x44; Text 18 |
| Bag grid | 8 x 5 `slot_bag_normal` 64 px, 2 px gap (as drawn; rule 11's 12 px is for hotbar/catalogue/trade grids), origin x 109 y 522; icon 48 centred; count Source Sans Bold 18 bottom-right with 1 px shadow; name text only when no icon | 3 px padding, studs at 2x, count 16, name 11 px | `UUniformGridPanel` slot padding 1; Num 18; Name 16 |
| Footer | "Weight 34 / 80 kg" Text 18 left; "Gold 248" Num 18 + `icon_coin_stack` 20 right; y 870 | weight 14 + 10 px bar | Text 18 both sides; drop the weight bar |
| Tooltip | floating layer (not clipped by the panel): `tooltip_frame` 360x320 at the cursor, right of the panel in the capture (x 585, y 124); item art 96; title EB Garamond 26; type line 18 grey; stat rows label 18 grey / value Source Sans 18 ivory; `divider`; description 18 ivory | 240 wide, text 18/12/12/12 | `SetToolTip` content widths 360; Text 26/18/18; description Text 18 |
| Master HUD | dim 0% (inventory is a side panel, HUD stays live) | n/a | none |

### 4. market-board

| element | concept spec | live problem | fix |
|---|---|---|---|
| Modal | dim 40%; `modal_frame` x13-87% (250-1670, 1420 wide), y10-85% (108-918) as approved (bottom 31 px above the hotbar); padding 32 | content-sized 860x560 board centred, dim 55%, `Panel()` → panel_frame | `USFMarketBoard::NativeOnInitialized`: `UCanvasPanel` root, dim 0.40, board slot anchors (0.13,0.10)-(0.87,0.85), `KitPanel("modal_frame")` |
| Header | `icon_stag_emblem` 56 + "Market Board" EB Garamond Bold 48; `close_x` 40 top-right; Esc/B closes | Cinzel "MARKET BOARD" 26; no close | Text 48 Bold; close_x |
| Tabs | Browse (selected) / My Listings / Sell: `tab_*` 190x44, Text 20, LB/RB caps 40x26 on pad | four category buttons All/Materials/Tools/Food 2x-margin `tab_normal`, 14 px | three tabs; categories move to the rail |
| Category rail | 5 rows 190x48 at x 282, y 300: All Items (`tab_selected` tint) / Weapons / Materials / Consumables / Tools; glyph 24 + Text 20 | none | VerticalBox of `KitPanel("tab_*")` 190x48; maps to `ESFInventoryTab` (+ Weapons/Consumables when items exist) |
| Search | `search_field` 720x44 at x 520, y 296; hint "Search listings" 18 | 170 wide `slot_empty` field | `search_field` brush on the `UEditableTextBox`; width 720 |
| Table | header row 44: Item ▲ (`icon_sort_arrow`), Qty, Unit Price, Seller, Time Left, Text 18 grey; rows 56 px: icon 40 + name Text 20; qty Num 18; `icon_coin_stack` 18 + price Num 18; seller Text 18; time Text 18; selected row = `tab_selected` tint + brass edge; 1 px `3A4348` rules between rows; 720 wide, 6 rows visible then scroll | rows are `slot_empty` (bag-slot studs) stretched to 560x90 with a 2x Buy button each; 14 px cells | `Rebuild`: row = `UBorder` flat slate with bottom rule, `tab_selected` brush when selected; clicking selects (no Buy in rows) |
| Detail card | `detail_card_frame` 340x520 at x 1262, y 290: image well 280x170 (`SFScreenArt::Icon` 128 centred on a slate well), name 26, rows Qty / Unit price / Seller label 18 grey + value 18 ivory, `divider`, Market fee row, Hold to Buy = G3 hold Primary 300x56 | sell panel in `tooltip_frame` (Quantity/Price fields in bag-slot frames, 13-14 px) | new detail column; Buy = hold button; the sell form moves to the Sell tab (same fields styled with `search_field`, "List item" Primary 56) |
| Wallet | "Gold 1,240" is not on the concept board; show it in the detail card under Market fee as "You have 1,240" Text 18 grey | 16 px footer line | move to the card |
| Hotbar / vitals | master, dimmed | none | capture from the E1 session |

### 5. trade

| element | concept spec | live problem | fix |
|---|---|---|---|
| Window | dim 40%; `window_frame` x26-74% (499-1421, 922 wide), y14-78% (151-842, 691 tall) per REVIEW (not the image's y13-85%); padding 28 | content-sized ~680x500 `Panel()` centred by `PlaceCentered`; no dim | `USFTradeWindow`: `UCanvasPanel` root with dim 0.40 and the frame slot anchored (0.26,0.14)-(0.74,0.78); `ToggleTradeDemo` uses `FillViewport` |
| Title | "Direct Trade" EB Garamond Bold 48 centred with `divider` flourishes 60x8 each side; `close_x` 40 | Cinzel "TRADE" 26 | Text 48 Bold |
| Columns | two equal columns (x 522-951, 967-1398) split by a vertical 2 px `divider` (rotated) | two `tooltip_frame` boxes "YOUR OFFER" / "ROWAN'S OFFER" Cinzel 18 | plain columns, no frame; header = `portrait_well` 72 (`portrait_villager` / player portrait when present) + name Text 26 |
| Offer grids | 4 x 3 `slot_bag_normal` 64, 12 px gap, top y 325; icon 48, count Num 18 bottom-right; item name Text 16 under the top row; empty cells show the slot with a faint cross | item text lines 14 px ("Oak Log x24") | `FillSide`: `UUniformGridPanel` 4x3; icon via `SFScreenArt::Icon`; names 16 |
| Gold offered | slate inset field 300x44 (`search_field` brush), "Gold Offered" Text 18 left, `icon_coin_stack` 24 + Num 20 right; editable for mine (`UEditableTextBox`), static for theirs | `slot_empty` stud frame field; "Gold" 14 brass + 16 value | `search_field` brush; sizes above |
| Offer Locked toggle | `button_normal` 300x56 outline; pressed = moss tint (`2B3B2C`) + `status_tick` 28 + "Offer Locked" Text 24 + hint "Press again to unlock" Text 16 | "Lock in" 2x button, 14 px | G3 Secondary with the pressed state driven by `Model->Snapshot().Mine.bLocked` |
| Partner status | `status_plate` 300x46 with `icon_open_padlock` 24 + amber Text 18 "Reviewing offer" (open padlock per REVIEW) | "Locked in" / "Waiting" Text 14 moss | plate + glyph; moss `status_tick` when locked |
| Warning line | `warning` 24 + amber Text 18 "Review both offers before accepting." centred above the footer, y 806 | none | add |
| Footer | left: "Cancel Trade" Secondary 300x56 with `keycap_esc` 36 inside; right: "Hold to Accept Trade" G3 hold Primary 420x56; countdown "Expires 01:41" Text 16 grey under Cancel | three 2x `button_normal` buttons 14 px + countdown 14 | G3 buttons; Accept is hold-to-confirm (ring + arc over 1.0 s) |
| Hotbar / vitals / tracker | master, dimmed 40%; hotbar numerals inside slots | none | capture from the E1 session |

## Capture protocol (docs/evidence/ui-v5)

1. Session: the E1 play session on `/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook` (`GameDefaultMap`) with `ASFE1PlayerController`, gathering + combat + quest pilots enabled, as the normal game starts. Never the construction sandbox/network client and never a mannequin pawn. Launch: `UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -windowed -ResX=1920 -ResY=1080 -ForceRes -nosound` and capture with the console `HighResShot 1920x1080 filename=<abs path>` (1x, DPI scale 1) — never an OS screenshot of the window. Record the exact command and console lines in `docs/evidence/ui-v5/README.md`.
2. Scene and light: the GFX-01 WP2 Silverbrook lighting (`silverbrook-lighting.json`: sun pitch -55°, yaw 135°, 10 lux, 5300 K, bright hazy daylight) on the river trail facing the bridge and mill, player lit from the front-left as in `gameplay-hud.png`; one of the fixed reference camera positions in `review_silverbrook_in_game.py` is acceptable if it shows river, bridge and trees. Reject any capture where the player stands in deep shadow or on the flat test plot.
3. gameplay-hud: play mode, a boar or gathering node aimed (target frame), one harvest just completed (toast), tracker showing the active quest, no window open.
4. build-mode: enter build mode from the same E1 session; the panel must be the Build Catalogue of table 2 (never the dev palette) with the `action_bar` prompt bar. If the E1 controller cannot enter build mode yet (GM-01), report the capture as blocked; do not substitute the construction-network client.
5. inventory: press I in play mode, hover the main-hand axe so the tooltip shows. market-board: F7 demo listing selected (Iron Axe), dim visible. trade: F6 demo with my offer locked and the partner "Reviewing offer".
6. Each capture is 1920x1080 PNG (JPEG q2 if over 3 MB), named `<screen>-live.png`, with a `<screen>-compare.png` side by side with the concept for review.

## Kit regeneration

None required. The ornament overreach is handled by the G2 margins; the 1x resample is a build step. Two optional later regenerations, not blocking: slim slot frames without corner studs (the concepts show plain slots; the kit studs are accepted for now at 1x), and a flat `market_row` piece if the tinted border proves too plain.
