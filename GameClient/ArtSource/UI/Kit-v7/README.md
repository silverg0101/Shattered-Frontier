# Shared menu UI kit v7

Painted 2x art sources for all menu windows. Every visible pixel in every part comes from the built-in `image_gen` tool. Pillow was used only to split source sheets, trim/clean alpha, resize, cap the button and tab center alpha, and assemble the uncommitted contact sheet. The owner reference images are style inputs; no reference pixels were extracted. Labels, item art, quantities, button text and menu layout remain live Widget Blueprint content.

`Esc` closes windows. There are no close-button sprites. Place a window emblem above the **empty** `header_banner_shield`; no stag is baked into the kit. Overlay `button_hold_fill` beneath a button face and clip it to the hold fraction. Use the same scrollbar pieces in every menu. Use separators by default; `section_panel` is an optional subdued backing.

The split frame composes as: `frame_fill` first, edges second, corners last. Corners are **160x160 px**, edge strips are **24 px thick**. Put each corner at the corresponding window corner with **0 px outward offset**; its flourish reaches inward beyond the rail. Start each rail **136 px** from its window corner, giving **24 px overlap** under each 160 px corner. Stretch/tile the straight middle of each rail. Keep `window_shell` as the 9-slice fallback.

All sizes and 9-slice margins are source pixels. Margins are L,T,R,B; `—` means no 9-slice. A zero on the separator's narrow axis means stretch only along its length. Source reference abbreviations: market = `reference-market.webp`, recipes = `reference-recipes.webp`, build catalogue = `reference-build-catalogue.png`, separator = `reference-separator.png`. The `image_gen` column names the prompt below.

| File | Size | 9-slice margins | Use | Source reference | image_gen |
| --- | ---: | --- | --- | --- | --- |
| `window_shell.png` | 1600x1200 | 80,80,80,80 | Fallback complete 9-slice window | separator; market | shell |
| `frame_fill.png` | 256x256 | — | Seamless slate window interior | market; recipes | fill |
| `frame_corner_tl.png` | 160x160 | — | Ornate split-frame corner | separator; market | frame |
| `frame_corner_tr.png` | 160x160 | — | Ornate split-frame corner | separator; market | frame |
| `frame_corner_bl.png` | 160x160 | — | Ornate split-frame corner | separator; market | frame |
| `frame_corner_br.png` | 160x160 | — | Ornate split-frame corner | separator; market | frame |
| `frame_edge_top.png` | 640x24 | — | Seamless split-frame brass rail | separator; market | frame |
| `frame_edge_bottom.png` | 640x24 | — | Seamless split-frame brass rail | separator; market | frame |
| `frame_edge_left.png` | 24x640 | — | Seamless split-frame brass rail | separator; market | frame |
| `frame_edge_right.png` | 24x640 | — | Seamless split-frame brass rail | separator; market | frame |
| `header_banner_shield.png` | 160x192 | — | Empty heraldic mount; layer emblem above | market | panels |
| `icon_window_inventory.png` | 96x96 | — | Leather backpack window emblem | market; recipes; build catalogue | icons |
| `icon_window_market.png` | 96x96 | — | Balance scales window emblem | market; recipes; build catalogue | icons |
| `icon_window_recipes.png` | 96x96 | — | Anvil and hammer window emblem | market; recipes; build catalogue | icons |
| `icon_window_trade.png` | 96x96 | — | Handshake window emblem | market; recipes; build catalogue | icons |
| `icon_window_quests.png` | 96x96 | — | Scroll and quill window emblem | market; recipes; build catalogue | icons |
| `icon_window_settings.png` | 96x96 | — | Cog window emblem | market; recipes; build catalogue | icons |
| `icon_window_build.png` | 96x96 | — | House window emblem | market; recipes; build catalogue | icons |
| `icon_tab_voxel.png` | 96x96 | — | Voxel cube tab emblem | market; recipes; build catalogue | icons |
| `icon_tab_blueprints.png` | 96x96 | — | Stacked plans tab emblem | market; recipes; build catalogue | icons |
| `icon_favorite_star.png` | 96x96 | — | Favorite outline star | market; recipes; build catalogue | icons |
| `icon_favorite_star_filled.png` | 96x96 | — | Favorite filled star | market; recipes; build catalogue | icons |
| `icon_material_log.png` | 96x96 | — | Rough log material emblem | market; recipes; build catalogue | icons |
| `tab_normal.png` | 420x84 | 18,16,18,16 | tab normal face | recipes | controls |
| `tab_hover.png` | 420x84 | 18,16,18,16 | tab hover face | recipes | controls |
| `tab_selected.png` | 420x84 | 18,16,18,16 | tab selected face | recipes | controls |
| `button_normal.png` | 440x96 | 20,18,20,18 | button normal face | market; recipes | controls |
| `button_hover.png` | 440x96 | 20,18,20,18 | button hover face | market; recipes | controls |
| `button_pressed.png` | 440x96 | 20,18,20,18 | button pressed face | market; recipes | controls |
| `button_disabled.png` | 440x96 | 20,18,20,18 | button disabled face | market; recipes | controls |
| `button_hold_fill.png` | 440x96 | 18,18,18,18 | Gold hold progress strip | market; recipes | controls |
| `stepper_button_normal.png` | 84x84 | 12,12,12,12 | stepper button normal face | recipes | controls |
| `stepper_button_hover.png` | 84x84 | 12,12,12,12 | stepper button hover face | recipes | controls |
| `stepper_fill.png` | 84x84 | 10,10,10,10 | stepper fill face | recipes | controls |
| `row_back.png` | 1600x120 | 16,16,16,16 | Plain list row | market; recipes | panels |
| `ingredient_row.png` | 1000x112 | 14,14,14,14 | Ingredient list row | market; recipes | panels |
| `section_panel.png` | 800x900 | 22,22,22,22 | Subtle optional list panel | market; recipes | panels |
| `slot_back.png` | 208x208 | 18,18,18,18 | Item icon backing | market; recipes | panels |
| `footer_gold_row.png` | 1600x88 | 20,18,20,18 | Wallet footer | market; recipes | panels |
| `plus_slot.png` | 208x208 | 18,18,18,18 | Empty sell slot with gold plus | market | fields |
| `separator.png` | 1200x24 | 40,0,40,0 | Horizontal thin brass rule | separator; market; build catalogue | overlays |
| `separator_vertical.png` | 24x1000 | 0,40,0,40 | Vertical thin brass rule | separator; market; build catalogue | overlays |
| `select_frame.png` | 1600x120 | 18,18,18,18 | Bright gold selected-row outline | separator; market; build catalogue | overlays |
| `hover_frame.png` | 1600x120 | 18,18,18,18 | Dim gold hovered-row outline | separator; market; build catalogue | overlays |
| `scrollbar_track.png` | 36x600 | 10,20,10,20 | Shared menu scrollbar track | separator; market; build catalogue | overlays |
| `scrollbar_thumb.png` | 32x160 | 10,20,10,20 | Shared menu scrollbar thumb | separator; market; build catalogue | overlays |
| `scrollbar_arrow_up.png` | 48x48 | — | Shared scrollbar up arrow | separator; market; build catalogue | overlays |
| `scrollbar_arrow_down.png` | 48x48 | — | Shared scrollbar down arrow | separator; market; build catalogue | overlays |
| `glyph_minus.png` | 48x48 | — | Stepper minus sign | market; recipes | glyphs |
| `glyph_plus.png` | 48x48 | — | Stepper plus sign | market; recipes | glyphs |
| `icon_search.png` | 48x48 | — | Search magnifier | market; recipes | glyphs |
| `icon_chevron_down.png` | 48x48 | — | Dropdown chevron | market; recipes | glyphs |
| `spin_arrows.png` | 48x72 | — | Number input arrows | market; recipes | glyphs |
| `icon_gold_stack.png` | 80x80 | — | Plain unnumbered coin stack | market; recipes | glyphs |
| `icon_gold_coin.png` | 64x64 | — | Plain unnumbered price coin | market | coin |
| `icon_check_ok.png` | 96x96 | — | Green ready check | build catalogue | fields |
| `search_field.png` | 560x84 | 14,14,14,14 | Empty search field; overlay glyph | market; recipes | fields |
| `dropdown_field.png` | 440x84 | 14,14,14,14 | Empty dropdown field; overlay chevron | market; recipes | fields |
| `input_field.png` | 520x84 | 14,14,14,14 | Empty text or number field | market; recipes | fields |
| `number_field.png` | 180x84 | 12,12,12,12 | Quantity value field | market; recipes | fields |

## Built-in image_gen prompts

Each source was generated with the reference images named in its rows as style input. All listed final parts are crops/resizes of these outputs. The prompts are reproduced exactly:

### frame

> Use case: ui-mockup. Create a NEW transparent RGBA source sprite sheet for a painted fantasy MMO shared menu frame, closely matching the supplied references' aged brass with real metallic glints, scratches and dark blue-green slate. Eight separate cells in a precise 4-column by 2-row grid with equal square cells and generous transparent gutters. Row 1: ornate top-left brass corner, mirrored top-right brass corner, ornate bottom-left brass corner, mirrored bottom-right brass corner. Row 2: straight seamless horizontal top brass border strip, straight seamless horizontal bottom brass border strip, straight seamless vertical left brass border strip, straight seamless vertical right brass border strip. Each component isolated on transparent background, front-on, no outside drop shadows. Brass border is fine, roughly 5% of a cell wide, with corner flourishes overhanging inward. No lettering, icons, scenery, entire window, or labels. Painted detailed material quality, consistent thickness, no flat vector outlines. Keep all eight parts cleanly separated for cropping.

### controls

> Use case: ui-mockup. Paint a transparent RGBA sprite sheet of 12 distinct empty fantasy MMO UI controls in an exact 3-column 4-row grid, each item isolated with wide transparent gutters. No text or symbols. Match references: genuine worn antique brass highlights and blue-green dark slate texture, painted dimensional material, front-on orthographic. Row 1: wide tab normal with a very thin dim brass outline and transparent dark fill; wide tab hover with lighter brass outline and transparent dark fill; wide tab selected with bright green border and luminous translucent green fill. Row 2: wide gold action button normal; wide gold action button hover; wide gold action button pressed. All button faces should be partially transparent to reveal fill below, with opaque aged brass border, short radius corners, no outside shadow. Row 3: wide gold action button disabled, bright gold button hold fill strip, square stepper normal. Row 4: square stepper hover, square stepper gold click fill, wide number field. Keep horizontal controls approximately 4.5:1 and square controls truly square, each centered within its grid cell. Uniform border thickness and straight stretchable centers, standalone sprites, no background, no lettering, no mockup.

### icons

> Use case: ui-mockup. Paint a sprite atlas of twelve isolated gold engraved fantasy MMO emblems, exact 4 columns x 3 rows, equal square cells, wide transparent gutter. Each icon has aged gold metallic highlights, shadows, engraved depth, worn hand-painted finish like the house, voxel cube, stacked sheets and stag in the supplied UI references. No flat vector line icons, no text, no circles around icons, no cast shadow, genuinely transparent RGBA outside each icon. Row 1 left to right: leather backpack, balance scales, anvil with hammer, handshake. Row 2: scroll with quill, cog wheel, small medieval house, wood voxel cube. Row 3: two stacked blueprint sheets, outlined favorite star, filled favorite star, one natural rough oak log. Center and isolate each motif so each cell can be cropped. Keep silhouette legible at 48 px display size.

### panels

> Use case: ui-mockup. Paint a transparent RGBA sprite atlas of eight separate empty fantasy MMO menu panels in an exact 2-column x 4-row grid, large transparent gutters, no text, no icons, no outside shadows. Worn aged brass trim and near-black blue-green slate with fine texture matching the references, front-on. Row 1: broad dark slate window shell with thin ornate brass corners and fine edge, 4:3 ratio; seamless square of the same slate texture, no border. Row 2: broad plain list row with very subtle slate translucent fill and hairline brass divider; smaller ingredient row of same treatment. Row 3: large section panel with subdued thin border and dark slate fill; square item slot with clear fine brass border and dark slate fill. Row 4: wide wallet footer row with fine aged brass top and bottom lines, subtle slate fill; empty heraldic dark slate shield/banner with aged brass edge matching the stag mount in market reference but absolutely no emblem. Each component centered in its own cell for cropping, no unrelated composition.

### overlays

> Use case: ui-mockup. Transparent RGBA sprite atlas, precise 2 columns x 4 rows, wide transparent gutters. Eight separate fantasy MMO UI ornament parts, close to references' very THIN antique brass linework and restrained worn highlights, no text or outside shadow. Row 1: long hairline brass horizontal separator with small pointed diamond end caps exactly like Inventory reference; matching hairline vertical separator with diamond end caps. Row 2: large wide bright-gold selection outline for a market row, EMPTY fully transparent center, faint inward glow, thin not neon; matching dimmer hover row outline, EMPTY transparent center. Row 3: slim vertical scrollbar track of dark slate and aged brass; slim rounded brass scrollbar thumb. Row 4: small engraved brass triangle arrow pointing up; small engraved brass triangle arrow pointing down. Front-on isolated shapes, clean alpha background including inside empty outline frames. Fine metal texture, polished detail.

### fields

> Use case: ui-mockup. Create a transparent RGBA sprite atlas of twelve separate fantasy MMO control parts, arranged in EXACTLY four columns and three rows of equal cells, with generous transparent gutters. Antique aged gold brass and dark blue-green slate, finely painted worn metallic depth matching references. No baked text, no numbers, no outside shadow. Row 1: wide empty search text field, wide empty dropdown field, wide empty plain input field, square empty sell slot with engraved brass plus sign. Row 2: engraved brass magnifying glass glyph, engraved brass chevron down glyph, paired up/down tiny input spinner arrows, plain modest stack of three gold coins with NO numeral. Row 3: engraved brass minus glyph, engraved brass plus glyph, green circular check emblem like 'placement ready' in build catalogue, empty square item slot in subtle brass. All front-on and isolated, no mockup.

### glyphs

> Use case: ui-mockup. One transparent RGBA sprite sheet of exactly SIX isolated small engraved gold metal glyphs in two rows of three equal square cells with generous gaps: top row minus sign, plus sign, magnifying glass; bottom row small downward chevron, paired tiny up/down spin arrows, plain stack of three gold coins WITHOUT any symbol, letter or number. Match the references' worn antique brass highlights and true shaded metal, no dark backing square, no circular button, no text, no shadows or glow outside motifs, fully transparent background. Each glyph centered and separate for cropping.

### shell

> Use case: ui-mockup. One standalone empty fantasy MMO window shell asset, front-on, exact 4:3 landscape aspect, fills most of transparent RGBA canvas. Match references closely: nearly black blue-green SLATE textured interior, very thin aged brass rails, restrained ornate tiny corner flourishes with bright metal highlights and scratches, no giant rim, no stag, no emblem, no text, no controls, no scenery, no exterior shadow. Center uniform and straight edges suitable for 9-slice scaling; visible real painted material, not flat vector geometry. No extra objects.

### coin

> Use case: ui-mockup. Generate one isolated plain unnumbered aged gold coin as a painted fantasy MMO UI sprite. Transparent RGBA background. No text, no numerals, no outside shadow. Match the coin in the market reference.

### fill

> Use case: ui-mockup. Generate a single seamless tileable texture square for the INTERIOR FILL of fantasy MMO menu windows. Match the owner references: nearly black desaturated blue-green slate, extremely fine irregular stone and worn pigment grain, very low contrast, no border, no lighting falloff or vignette, no icons or marks. Opposite edges must join imperceptibly when tiled horizontally and vertically. Fully opaque RGBA square material asset, front-on.

## Verification and handoff

`Artifacts/kit7-sheet.png` is a local mid-grey contact sheet and is intentionally not committed. All 60 delivered sprites are RGBA PNGs at the listed sizes. Every border, emblem and control has genuine transparent pixels; `frame_fill` alone is intentionally fully opaque for tiled window coverage. Button-face interiors are capped at alpha 150–175 (with opaque generated brass borders), allowing the gold hold fill to show beneath. The parts are art sources; Unreal import, in-engine visual review and owner sign-off are separate work.
