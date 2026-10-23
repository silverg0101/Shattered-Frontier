# Shared menu UI kit v7

New redraws of the owner references. No pixel from a reference image is copied into a part. Every part is a separate RGBA PNG at 2× display resolution. Live labels, item art, numbers, button captions and menu layout belong to Widget Blueprints.

Use window_shell for every menu. Its central 9-slice rails are uniform, with the ornament contained in the 80 px corners. Put one 96 px icon_window_* over the empty header_banner_shield; the stag is not a window emblem. Use the Recipes tab treatment on all tabbed views and the shared gold button treatment on all actions. Draw button_hold_fill below a button face and clip it by the hold fraction. For a minus flash, clip stepper_fill from the right.

## Parts

Margins are source pixels in L,T,R,B order; a dash means no slicing. A zero margin on a separator axis means that axis is never stretched. The intended middle remains uniform.

| File | Size | 9-slice margins | Intended use | Reference |
| --- | ---: | --- | --- | --- |
| `window_shell.png` | 1600x1200 | 80,80,80,80 | Shared expandable slate shell, thin ornamental brass frame | reference-separator.png; reference-market.webp |
| `header_banner_shield.png` | 160x192 | — | Empty header backing; layer emblem over it | reference-market.webp; imagegen redraw |
| `icon_window_inventory.png` | 96x96 | — | Inventory gold line emblem | reference-market.webp; reference-recipes.webp |
| `icon_window_market.png` | 96x96 | — | Market gold line emblem | reference-market.webp; reference-recipes.webp |
| `icon_window_recipes.png` | 96x96 | — | Recipes gold line emblem | reference-market.webp; reference-recipes.webp |
| `icon_window_trade.png` | 96x96 | — | Trade gold line emblem | reference-market.webp; reference-recipes.webp |
| `icon_window_quests.png` | 96x96 | — | Quests gold line emblem | reference-market.webp; reference-recipes.webp |
| `icon_window_settings.png` | 96x96 | — | Settings gold line emblem | reference-market.webp; reference-recipes.webp |
| `tab_normal.png` | 420x84 | 18,16,18,16 | tab normal category tab | reference-recipes.webp |
| `tab_hover.png` | 420x84 | 18,16,18,16 | tab hover category tab | reference-recipes.webp |
| `tab_selected.png` | 420x84 | 18,16,18,16 | tab selected category tab | reference-recipes.webp |
| `button_normal.png` | 440x96 | 20,18,20,18 | Translucent button face; over hold progress fill | reference-market.webp; reference-recipes.webp; imagegen button material |
| `button_hover.png` | 440x96 | 20,18,20,18 | Translucent button face; over hold progress fill | reference-market.webp; reference-recipes.webp; imagegen button material |
| `button_pressed.png` | 440x96 | 20,18,20,18 | Translucent button face; over hold progress fill | reference-market.webp; reference-recipes.webp; imagegen button material |
| `button_disabled.png` | 440x96 | 20,18,20,18 | Translucent button face; over hold progress fill | reference-market.webp; reference-recipes.webp; imagegen button material |
| `button_hold_fill.png` | 440x96 | 18,18,18,18 | Gold progress fill; clip by hold fraction | reference-market.webp |
| `stepper_button_normal.png` | 84x84 | 12,12,12,12 | Square stepper face | reference-recipes.webp |
| `stepper_button_hover.png` | 84x84 | 12,12,12,12 | Square stepper face | reference-recipes.webp |
| `glyph_minus.png` | 48x48 | — | Separate stepper sign | reference-recipes.webp |
| `glyph_plus.png` | 48x48 | — | Separate stepper sign | reference-recipes.webp |
| `stepper_fill.png` | 84x84 | 10,10,10,10 | Click flash; clip left to right on plus and reverse on minus | reference-recipes.webp |
| `number_field.png` | 180x84 | 12,12,12,12 | Value between steppers | reference-recipes.webp |
| `row_back.png` | 1600x120 | 16,16,16,16 | Market and recipes plain list row | reference-market.webp; reference-recipes.webp |
| `ingredient_row.png` | 1000x112 | 14,14,14,14 | Ingredient row without gold selection | reference-recipes.webp |
| `slot_back.png` | 208x208 | 18,18,18,18 | Square item icon backing | reference-market.webp; reference-recipes.webp |
| `select_frame.png` | 1600x120 | 18,18,18,18 | Transparent center selection overlay | reference-market.webp; Inventory-v6 slot_selected |
| `hover_frame.png` | 1600x120 | 18,18,18,18 | Transparent center selection overlay | reference-market.webp; Inventory-v6 slot_selected |
| `section_panel.png` | 800x900 | 22,22,22,22 | List area or side panel backing | reference-market.webp; reference-recipes.webp |
| `footer_gold_row.png` | 1600x88 | 20,18,20,18 | Wallet footer backing | reference-market.webp |
| `separator.png` | 1200x24 | 40,0,40,0 | Thin brass rule with pointed fixed ends | reference-separator.png |
| `separator_vertical.png` | 24x1000 | 0,40,0,40 | Thin brass rule with pointed fixed ends | reference-separator.png |
| `close_button_normal.png` | 72x72 | 10,10,10,10 | Window close control | reference-market.webp |
| `close_button_hover.png` | 72x72 | 10,10,10,10 | Window close control | reference-market.webp |
| `search_field.png` | 560x84 | 14,14,14,14 | Search backing; live text/glyph separate | reference-market.webp |
| `dropdown_field.png` | 440x84 | 14,14,14,14 | Sort/options backing; live text/glyph separate | reference-market.webp |
| `input_field.png` | 520x84 | 14,14,14,14 | Plain text/numeric backing; live text/glyph separate | reference-market.webp |
| `icon_search.png` | 48x48 | — | Search magnifier | reference-market.webp |
| `icon_chevron_down.png` | 48x48 | — | Dropdown glyph | reference-market.webp |
| `spin_arrows.png` | 48x72 | — | Number input arrows | reference-market.webp |
| `icon_gold_coin.png` | 64x64 | — | Price coin | reference-market.webp |
| `icon_gold_stack.png` | 80x80 | — | Wallet stack | reference-market.webp |
| `plus_slot.png` | 208x208 | 18,18,18,18 | Empty sell slot with plus | reference-market.webp |

## Generation prompts and method

Image generation used the following exact prompts with the owner references attached. The generated shield backing is the source of header_banner_shield.png. A generated gold button provided subtle material variation for the four button faces; their matching silhouettes, state colours, alpha and 9-slice geometry were redrawn with Pillow. All other parts were freshly drawn with Pillow geometry under the part briefs in the table above so borders and glyphs stay crisp and aligned. Two imagegen experiments for anvil and scroll were discarded because their filled illustration style did not match the required thin gold line icons. No part was cropped from an owner reference.

1. **Header banner-shield** (reference-market.webp, reference-separator.png): “Use case: ui-mockup. Generate one BRAND NEW REDRAW of a single isolated empty heraldic banner-shield backing for a fantasy game menu header, matching the dark slate and thin antique-brass line-work in the supplied references. The shield is empty: absolutely NO stag, animal, emblem, letters, symbols, text, scenery or surrounding frame. Symmetrical front-on 2D sprite, crisp edge, dark desaturated blue-slate inset with narrow aged brass outline and tiny understated top mounts. Centered and filling most of a square canvas. Genuinely transparent RGBA everywhere outside the shield, including all four corners. No cast shadow or halo. This is a standalone UI sprite, not a screenshot.”
2. **Gold button material** (reference-market.webp, reference-recipes.webp): “Use case: ui-mockup. Create ONE isolated empty horizontal button face sprite as a NEW REDRAW, matching the antique brass rectangular Buy / List item / Craft button visual language in the supplied game-menu screenshots. FRONT ON flat 2D, 4.6:1 wide aspect, subtly rounded 3 px corners, bright fine gold outer border, fine inner highlight, partly translucent warm slate-gold face. Absolutely no words, letters, glyphs, highlights resembling text, scene, cast shadow, halo or mockup. Centered on genuine transparent RGBA canvas with all four corners transparent. Preserve simple straight stretchable middle for 9-slice use. Do not crop or extract either reference image.”
3. **Discarded icon trial, anvil** (reference-recipes.webp): “Use case: ui-mockup. Redraw ONE isolated front-on fantasy game UI icon: a small anvil with a hammer, matching the reference’s thin warm gold line style. Consistent clean strokes, legible at 48x48 display size, no text, no other objects. Transparent RGBA square canvas, all corners fully transparent, no outside shadow, no background.”
4. **Discarded icon trial, scroll** (reference-recipes.webp): “Use case: ui-mockup. Redraw ONE isolated front-on fantasy game UI icon: an empty scroll with quill, matching thin warm antique-gold line style in the reference. Single clean emblem, legible at 48x48 display size, no text, no other objects. Transparent RGBA square canvas, all corners fully transparent, no outside shadow, no background.”

For the Pillow redraws, the brief for each part is the Intended use cell above, with this shared prompt: “Redraw one isolated front-on flat 2D menu sprite; dark slate, thin antique brass, restrained green only for selected tabs, no baked text, no outside shadow, crisp transparent edges; preserve a simple stretchable center and the listed fixed margins.”

## Handoff

Artifacts/kit7-sheet.png is a local contact sheet on mid-grey and is intentionally not committed. The generated PNGs are art sources only; there is no Unreal import or runtime binding in this change. A separate in-engine visual review and owner sign-off remain for implementation.
