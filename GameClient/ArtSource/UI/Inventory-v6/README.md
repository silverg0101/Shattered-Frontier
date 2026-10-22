# Inventory v6 source parts

Owner target: `reference-owner.png`. These are new redraws, never crops of the owner image. Each PNG is a separate transparent RGBA sprite at 2× intended display size. Text, numbers, item art, and layout are supplied by the UI. No Unreal import is included.

| Part | Pixels | Slice margins at source resolution (L,T,R,B px) |
| --- | ---: | --- |
| `panel_frame.png` | 1360×1600 | 80,80,80,80 |
| `header_divider.png` | 1200×20 | horizontal 3-slice: L/R 36 |
| `crest_stag.png` | 96×96 | — |
| `tab_bar_back.png` | 1200×130 | 40,32,40,32 |
| `tab_normal.png` | 216×120 | 28,28,28,24 |
| `tab_selected.png` | 216×120 | 28,28,28,24 |
| `tab_divider.png` | 8×110 | vertical 3-slice: T/B 12 |
| `icon_tab_materials.png` | 48×48 | — |
| `icon_tab_gear.png` | 48×48 | — |
| `icon_tab_consumables.png` | 48×48 | — |
| `icon_tab_misc.png` | 48×48 | — |
| `slot_normal.png` | 208×208 | 30,30,30,30 |
| `slot_hover.png` | 208×208 | 30,30,30,30 |
| `slot_selected.png` | 208×208 | 34,34,34,34 |
| `tooltip_frame.png` | 760×430 | 34,34,34,34 |
| `tooltip_divider.png` | 680×12 | horizontal 3-slice: L/R 28 |
| `icon_weight.png` | 40×40 | — |
| `progress_track.png` | 680×26 | 18,8,18,8 |
| `progress_fill_green.png` | 680×22 | 16,7,16,7 |
| `progress_fill_mask.png` | 680×22 | use fill alpha; no slice |
| `scrollbar_track.png` | 12×1100 | vertical 3-slice: T/B 18 |
| `scrollbar_thumb.png` | 10×130 | vertical 3-slice: T/B 18 |

Use the 9-slice center as a uniform fill and preserve the corner and border pixels. For the thin rules, preserve the end caps. The progress fill mask is scripted from the generated fill alpha and can clip a rectangular green fill to rounded ends. `Artifacts/invart-sheet.png` is the local review contact sheet and is intentionally uncommitted.

## Generation prompts

All imagegen calls used `reference-owner.png` as a style reference only. Shared instruction: “BRAND NEW REDRAW, never crop or extract. One isolated front-on flat 2D inventory UI sprite, crisp edges, dark slate, aged brass, muted moss green and ivory as appropriate, no text, no scene or shadow, genuinely transparent RGBA outside the object, all canvas corners transparent, no halo.” The individual subject prompts were:

| Part | Subject prompt |
| --- | --- |
| `panel_frame` | Dark slate rectangular panel, thin brass border and restrained ornate corners; plain stretchable middle. |
| `header_divider` | Thin horizontal aged-brass rule with small ornamental end caps and uniform middle. |
| `crest_stag` | Symmetric frontal heraldic stag head with branched antlers in pale ivory and antique brass. |
| `tab_bar_back` | Long dark slate category strip with faint border and plain stretchable middle. |
| `tab_normal` | One dark slate tab with subtle brass perimeter and empty center. |
| `tab_selected` | One moss-green selected tab with brass edge and empty center. |
| `tab_divider` | Very thin vertical muted brass separator with tiny terminals. |
| `icon_tab_materials` | Small tied pouch monochrome outline pictogram, thin consistent ivory/brass strokes only. |
| `icon_tab_gear` | Two crossed swords monochrome outline pictogram, thin consistent ivory/brass strokes only. |
| `icon_tab_consumables` | Single flask monochrome outline pictogram, thin consistent ivory/brass strokes only. |
| `icon_tab_misc` | Parchment scroll monochrome outline pictogram, thin consistent ivory/brass strokes only. |
| `slot_normal` | Empty square rounded near-black slate slot, faint brass hairline edge. |
| `slot_hover` | Same empty slot with only a slightly brighter muted edge and slate fill. |
| `slot_selected` | Same empty slot with bright gold edge and tightly contained glow. |
| `tooltip_frame` | Simpler wide dark slate tooltip panel, fine aged-brass edge and small corner marks. |
| `tooltip_divider` | Thin horizontal muted-brass rule with small end caps. |
| `icon_weight` | Small hanging weight silhouette with loop handle, warm ivory/brass. |
| `progress_track` | Long narrow empty dark slate weight bar with faint edge and rounded ends. |
| `progress_fill_green` | Long narrow muted moss-green fill with rounded ends and uniform center. |
| `scrollbar_track` | Extremely thin dark slate vertical scroll track, faint brass edge and rounded caps. |
| `scrollbar_thumb` | Thin aged-brass/slate vertical thumb, slightly brighter edge and rounded ends. |

Generated assets were isolated by alpha, resized with Lanczos to the table dimensions, and placed on transparent canvases. `tab_selected.png` uses the generated selected moss palette with the generated normal tab silhouette so the two states align exactly. `progress_fill_mask.png` is derived from `progress_fill_green.png`. The slots and icons were reviewed at intended display scale; the first pictorial icon variants and an overly bright hover variant were regenerated.

