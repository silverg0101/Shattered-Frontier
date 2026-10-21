# UI concept pass v5 — UX review

Reviewer: Fable UX, 2026-10-21. Judged against `../HUD-v1/hud-layout-v4.md` and shipped-MMO polish (WoW Dragonflight, ESO, New World). All images are 1672x941; px below are at 1080p (x1.148).

Result: 16 APPROVED. Re-review 2026-10-21 of the three regenerated images (`trade`, `10-npc-dialogue`, `13-character-creator`, prompts in `prompts-C.md`): all three approved with positional implementation notes; no further generation.

Top cross-screen issue: the persistent HUD drifts between screens (quest tracker format, minimap footer one vs two lines, hotbar numeral placement, level 12 vs 18, hex vs diamond badge, HP 424 vs 1,240, E vs F interact key, hotbar contents). Fix once: `gameplay-hud.png` is the master for every persistent element; every other screen's HUD is built from that widget set, not re-drawn from its own image (family rule 1).

## Verdicts

| Screen | Verdict | Top issues |
|---|---|---|
| gameplay-hud | APPROVED | Master HUD. Minimap lacks "N" label; add. |
| build-mode | APPROVED | Catalogue runs to 82% with cost box inside the panel (accept; one panel is simpler). Quest tracker/hotbar differ from master (rule 1). Slots 1-2 show a durability bar: keep as the durability treatment. |
| inventory | APPROVED | Panel is x4-34% not 6-50% (accept; world stays visible). Tooltip overlaps panel edge; render tooltips as a floating layer. No sort/filter row: add `Sort` + `All/Tools/Materials/Food` tabs under the heading in the same tab style. |
| gathering | APPROVED | Prompt "E Chop" vs master "F Interact": the key cap is driven by input mapping. Skill XP bars use moss fill; allowed as a progress fill (rule 7). |
| crafting-station | APPROVED | "Hold Craft" lacks the hold ring; craft is not irreversible, so build it as a plain brass "Craft" button. Minimap footer is two lines; use master one-line footer. |
| trade | APPROVED (re-review) | Cancel Trade + Esc cap, Offer Locked toggle with tick and unlock hint, status plate, warning line and hold-to-accept are all present; hotbar numerals now inside slots. Window measures x26-74% y13-85% (7% too tall at the bottom, nearly touching the hotbar): implementation corrects, see notes. Padlock on "Reviewing offer" is closed; use the open padlock glyph. |
| market-board | APPROVED | Tab row lacks LB/RB caps and the board lacks a close X (rule 10/11). HUD not visibly dimmed; dim 40%. |
| defeated | APPROVED | Clean. Stamina 286/326 while dead: implementation shows real values. |
| 9-loading-screen | APPROVED | Clean. Spinner is small; 40 px ring. |
| 10-npc-dialogue | APPROVED (re-review) | Accept is a single-label brass button with no ring; Decline outline; numbered key-cap response rows with moss-tint selection; XP roundel + coin stack; no duplicated dialogue line; hex badge and master hotbar. Board measures x26-75% y20-78% (top 6% low, board shorter than the centre window): implementation corrects, see notes. HUD not dimmed (rule 1). |
| 11-title-menu | APPROVED | Logo ornament is heavier than the family; keep the stag + wordmark, drop the laurel sprigs when slicing. No focused item shown: Play is default focus. |
| 12-character-select | APPROVED | No Back to title: add outline "Back" (Esc/B) bottom-left under the pane. Realm plate at x80-98% is fine (no gameplay HUD here). |
| 13-character-creator | APPROVED (re-review) | 2% inset respected, no logo, LB/RB caps, People step holds only presets + two sliders, hint row has mouse and RS glyphs, "Step 1 of 7" plate. Left rail measures x3-17% (3% wider than the 14% spec): implementation corrects, see notes. "Back" lacks a B glyph when a pad is active. |
| 14-settings | APPROVED | Best controller affordances in the set (LB/RB caps, close X). "Apply" is moss-filled: primary buttons are brass (rule 8). Motion Blur "Off" is crimson-filled: use outline + cross only. |
| 15-world-map | APPROVED | Clean. Add RS/LT-RT zoom glyphs next to +/- when a pad is active. |
| 16-quest-journal | APPROVED | "Hold to abandon" is a 12 px side label: build Abandon as the standard hold button (ring inside, label "Hold to Abandon"). Add close X top-right and LB/RB caps if group tabs are added. HUD behind (empty slots, no numerals, no badge) is replaced by the master HUD. |

## Implementation notes (all screens)

Existing kit: `GameClient/Content/Frontier/UI/Kit/` (`T_Kit_panel_frame`, `T_Kit_button_*`, `T_Kit_tab_*`, `T_Kit_slot_*`, `T_Kit_bar_*`, `T_Kit_toast_card`, `T_Kit_tooltip_frame`, `T_Kit_hold_confirm_ring`, `T_Kit_status_tick/cross`, `T_Kit_gamepad_*`, `T_Kit_keycap_blank`, `T_Kit_mouse_*`), source `GameClient/ArtSource/UI/HUD-Kit-v1/`. Font: `FontFace_Cinzel` (display only). Reuse before generating; regenerate a kit piece only where the approved image clearly differs.

- gameplay-hud: hex stag badge (badge brush 116 px, level plate), 3-slice hex-end bar frame + health/stamina fills, 88 px slot frame (empty/selected), key cap, target frame (name plate + level hex cap), circular minimap mask + brass ring + N/E/S/W tick, footer plate, quest tracker frame + 18 px checkbox, toast card (48 px icon well), amber warning triangle. Icons: axe, pickaxe, sword, bandage, potion, campfire (existing `T_Icon_*` where present).
- build-mode: left panel frame (chamfer corners), search field, two-tab strip, 56 px category rail buttons with glyphs (structures/decor/terrain/favourites), 100 px thumbnail tile + selected brass tile, dropdown field, cost row (tick/cross/underline), action bar with LMB/R/X/Esc caps and pad swap A/B/X/Y, 12 catalogue thumbnails, pale-green ghost material.
- inventory: wide panel frame, 9 equipment slot frames (72 px), paperdoll render target, 8x5 bag grid (64 px slots), footer weight/gold line, tooltip frame with engraved divider, ~40 item icons (generate as a sheet).
- gathering: skills panel frame, 4 skill rows (icon well 72 px, level numeral, slender XP bar with moss fill), "Next:" unlock icons (log, ore, herb, trout), world-projected ring material (brass ring, moss progress, centre % plate, label plate), target frame reuse.
- crafting-station: center-right panel frame with internal vertical divider, search, three-tab strip, 56 px recipe rows (icon + name, selected moss tint + brass edge + left arrow notch), ingredient rows, stepper (- value +), brass "Craft" button, queue rows with progress bars, recipe icons (planks, beam, pegs, board, nails).
- market-board: modal board frame with stag header, three-tab strip + LB/RB caps, category rail rows with glyphs, search, table header with sort arrow, 56 px rows with selected state, detail card frame, coin glyph, hold-to-buy button (ring left inside), close X.
- defeated: center window frame, crimson engraved divider with stag, 2 respawn cards (image well 280x170, selected moss edge + tick roundel, clock roundel), hold-to-respawn button, desaturation post-process.
- 9-loading-screen: full-bleed art, 220 px stag emblem with brass diamond frame, 72 px Cinzel zone title with engraved underline rule, tip text, 3-slice brass progress bar 620x14, 40 px ring spinner.
- 11-title-menu: 400x92 px menu buttons (brass 3-slice, chamfer notches, 44 px glyph well), logo (stag + wordmark, two sizes), news panel frame with image well, realm status pill (moss dot + tick), version text.
- 12-character-select: left pane frame, 420x116 slot rows (portrait well 96 px, selected moss edge, faction stag mark), empty-slot row (+ roundel), "Create New" outline button, "Delete" crimson outline button, 440x92 "Enter World" brass button, realm plate.
- 14-settings: modal frame with stag crest tab, five-tab strip + LB/RB caps, close X, preset segmented buttons (3), slider track + 28 px brass thumb + value field, On/Off segmented toggle with tick/cross, inset panel frame, key caps W/A/S/D/E/M, Apply (brass) + Revert (outline) 300x60.
- 15-world-map: parchment map art with brass frame, title banner (stag + Cinzel + subtitle rule), legend panel, player arrow, quest diamond, label plates, +/- 60 px buttons, scale bar, close X.
- 16-quest-journal: two-page book frame with hinge, 56 px quest rows (diamond glyph, selected brass edge + chevron), section rules, 24 px objective checkbox, XP roundel + coin stack, Track (outline) + Abandon (hold button, crimson outline).
- trade: centre window frame anchored exactly x26-74% y14-78% (rule 12), not the image's y13-85%; this leaves the required gap above the hotbar (hotbar top y86%). To fit the 64% height, offer grids use the 64 px bag slot with 12 px gaps (rule 11), 4x3 per column, names 16 px beneath the top row; title flourishes, portrait header 72 px, Gold Offered field 44 px, Offer Locked toggle (brass outline, pressed = moss tint + tick, hint 16 px), status plate (open padlock, amber text), warning line, footer with Cancel Trade (outline + Esc cap) and Hold to Accept Trade (primary, 36 px ring). Dim the whole HUD 40% (rule 1).
- 10-npc-dialogue: dialogue board is the centre window frame at x26-74% y14-78% (rule 12), not the image's x26-75% y20-78%; the extra 6% height goes to the quest art well (image 280x170 as the respawn cards) and row spacing, not to larger type. Left half: 120 px portrait frame, name plate, speech line, engraved divider, four 56 px response rows with 36 px numeral key caps (selected = moss tint + brass edge; pad shows A/B/X/Y). Right half: inset quest card frame, Objectives/Rewards section rules, 24 px checkboxes, XP roundel + coin stack, Accept (primary, no ring) + Decline (outline). Dim the HUD 40%; tracker switches to the offered quest while the board is open.
- 13-character-creator: left rail anchored x2-14% y8-92% (not the image's x3-17%); seven 64 px rows (56 px glyph well + 20 px label), active = moss tint + brass edge, LB/RB caps 40x26 above and below. Right panel x66-98% y8-92%: 140 px preset cards (selected = brass edge + moss tick roundel), 20 px description line, two slider rows (track + 28 px brass thumb + value field, as settings), footer hint row with mouse + RS glyphs, Randomize/Back (outline, Back shows B cap on pad) + Next (primary). "Step 1 of 7" plate bottom-centre, bottom at y96%. No gameplay HUD.

## Family rules (all implementations)

Measured from the approved images at 1080p; UMG canvas anchors, DPI curve scaled by height.

1. `gameplay-hud.png` is the master for vitals, hotbar, prompt bar, target frame, minimap, quest tracker and toasts. Other screens never re-draw these; they dim them (40% black overlay) when a window is open. Level badge is the hex shape; HP/stamina always show numerals inside; interact key comes from the input mapping.
2. Safe inset 2% (38 px). Nothing touches a screen edge.
3. Colours: panel slate #17232A at 85% alpha; brass #A98A55; ivory #E8E1CE; moss #688267 (state only: selected, valid, online); crimson #A34842 (health, danger, short-number underline); amber #B69A55 (stamina, warnings). Never colour alone: every state carries a glyph or word.
4. Frames: 2 px brass 9-slice with 10 px chamfer notches, 24 px corner overlays, 2 px engraved divider. Panel corner radius 0.
5. Type roles: Display serif (Cinzel) 72 px defeat title, 48 px window/modal titles, 36 px panel titles, 26 px section headings; Text serif (add an OFL text serif, e.g. EB Garamond) 24 px button labels, 20 px body, 18 px rows and footers, 16 px toast labels and tile captions; Sans numerals (add an OFL sans, e.g. Source Sans 3) 18-20 px for counts, have/need and bar values. Minimum on-screen size 16 px.
6. Hotbar: 6 slots 88 px square, 12 px gap, centred, bottom at y96%; slot number top-left inside, count bottom-right inside, 18 px. Vitals: badge 116 px at x2%; bars 380x32 px, 10 px apart, numerals centred. Prompt bar 52 px tall, 36 px key cap, bottom at y85%. Target frame 410x70 px top-centre. Minimap 250 px round, 32 px footer "Place · 000 m" one line. Quest tracker and toasts 280 px wide at x84-99%; toast 66 px tall, 12 px gap, max 3, newest on top.
7. Bars: brass 3-slice frame; health crimson, stamina amber, XP/progress moss. Moss is never a panel or button fill.
8. Buttons: brass 3-slice, 56 px tall, 24 px label. Primary = filled brass; secondary = brass outline on slate; destructive = crimson outline, never crimson fill. Hold-to-confirm (Buy, Accept Trade, Respawn, Abandon) = primary button with a 36 px ring inside its left end and label "Hold to …"; nothing else uses a ring.
9. Have/need: "have / need" sans 18 px, tick (moss) or cross (crimson) glyph at row end, 2 px crimson underline under the short number. Placement validity: glyph + text.
10. Tabs: 44 px tall, selected = moss tint + brass bottom rule + ivory text; LB/RB caps (40x26) outside both ends when a pad is active. Action bars swap key caps for A/B/X/Y glyphs. Every window has a close X (40 px) top-right and answers Esc/B.
11. Slots and tiles: 64 px bag slot, 72 px equipment slot, 88 px hotbar slot, 100 px catalogue tile, 12 px gaps. Selected = brass edge (not moss) for tiles and slots; moss edge is reserved for cards and rows.
12. Panels: left column x2-27%, left inventory x4-34%, station x30-82%, centre window x26-74% y14-78%, modal board x12-88% y16-87% (right column hidden only for modals). Zone/location text lives under the minimap, never top-left. No compass strip, no on-screen hide chevrons.

## Revised prompts (REGENERATE)

None outstanding. The prompts that produced the approved `trade`, `10-npc-dialogue` and `13-character-creator` images are recorded in `prompts-C.md`; remaining deviations are corrected in implementation (see notes above).
