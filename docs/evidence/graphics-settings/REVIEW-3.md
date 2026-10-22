# Settings > Graphics: independent review 3 (Fable, 2026-10-22)

Judged against `docs/Game-Design/concept-art/UI/v5/14b-settings-graphics.png` and `14c-settings-keep-revert.png` (resized to 1920x1080, concept px x 1.1483), DEC-05, `docs/blueprints/menus.md` sections 1, 2, 5 and `docs/visual-review.md`. Every frame in `captures/` (commit b9052c90) was read at 1920x1080 and in 1:1, 2x and 3x crops; `diff/*/report.md` numbers were used where they exist, and glyph ink boxes (min channel > 140-160) were measured for the rows the diff does not cover. All measurements are at 1920x1080, live minus concept. REVIEW-1 and REVIEW-2 were read as background only; every item below was re-measured in the images.

Result: 2 PASS, 2 FAIL.

| Screen | Verdict |
| --- | --- |
| Graphics menu (High) | FAIL |
| Preset states (Low-Ultra) + Custom | FAIL (behaviour correct; every frame carries menu items 1-5) |
| Keep prompt | PASS (kit-wide notes below, none blocking) |
| Scene captures (Low / High / Ultra) | PASS |

What review 2's fixes achieved: window, tabs, preset row, steppers, toggles, slider, quality rows and footer are all within +-2 px (31 of 33 elements pass geometry); the slider fill is now a layered moss band ending at a round brass thumb; the Off cross is ivory on a straight-ended crimson plate; the stepper wells are plain brass-hairline fields with divider lines; the close X is boxed; the title has flanking rules at 36 pt (ink 160x68 vs 156x64: one point, inside tolerance); `Preset: Custom` ends at x 1537, inside the frame; the keep modal is opaque with a square-ended bar in a brass frame. What still fails is below.

## Graphics menu (High) - FAIL

Most visible first.

1. **Stepper chevrons are half size and one third the stroke (all 13 steppers).** Concept Resolution right chevron: ink 9x16 at 912,489 with a 3 px full-ivory core (171/255/252/188 across the stroke). Live: ink 5x10 at 914,491 with a 1 px core (82/239/93). Same on the left chevron and on View Distance (9x15 vs 5x10). Cause: `Kit-v5/stepper_chevron.png` is 32x32 but its opaque ink is only 11x20 with a 3 px stroke at 2x, so a 16 px `KitImage` draws a 10 px glyph.
   Fix type: CODE (one value; no art round trip). `SFGraphicsMenu.cpp` line 199: `USizeBox* Arrow = KitImage(WidgetTree, TEXT("stepper_chevron"), 16.f, 16.f);` -> `USizeBox* Arrow = KitImage(WidgetTree, TEXT("stepper_chevron"), 26.f, 26.f);` (the 36 px end cell still contains it; the mirror transform is unchanged). Verify: Resolution right chevron ink 9x16 +-1, stroke >= 2 px, centre within 1 px of the value's centre line. Alternative if the upscale looks soft: Codex regenerates `stepper_chevron.png` 32x32 with the ink filling 18x32 and a 6 px stroke, and line 199 stays at 16.

2. **Row labels and the slider captions are two to three points too large.** Concept "Resolution" ink 95x17; live 112x19 (+18% width, +2 px cap box). Concept "View Distance" 120x14; live 132x16 (+10%). Concept "100%" 40x14; live 48x17 (+20%). Headings, tab labels, preset labels, legend and stepper values are within one point and are not in this item.
   Fix type: CODE. `SFGraphicsMenu.cpp`: line 253 `Text(WidgetTree, Name, 20, Ivory())` -> `Text(WidgetTree, Name, 17, Ivory())` (all six Display labels); line 355 `Text(WidgetTree, Row.Label, 18, Ivory())` -> `Text(WidgetTree, Row.Label, 16, Ivory())` (all ten Quality labels); line 301 `Text(WidgetTree, TEXT("100%"), 18, Ivory())` -> `15` and line 283 `Text(WidgetTree, TEXT("67%"), 18, Ivory())` -> `15`. Verify ink widths: Resolution 95 +-3, View Distance 120 +-3, 100% 40 +-3; cap height within 1 px.

3. **Close X is a thin glyph in the right box.** Concept: 17x17 ink, 3 px stroke, full ivory, inside the 46 px box. Live: 16x20 ink, ~1 px stroke, dim (the box itself passes at 1515,139,46,46). Cause: `Kit-v5/close_x.png` is 80x80 with a 67 px glyph and a 5 px stroke (7% of its size; the concept's is 18%), drawn at 28 px.
   Fix type: ART + CODE (shared piece; the Off toggle cross on line 237 reuses it at 14 px and will match the concept's small thin cross). ART (Codex): regenerate `GameClient/ArtSource/UI/Kit-v5/close_x.png`, 80x80, transparent background, no nine-slice, prompt: "UI close glyph, a single ivory #E8E1CE X, two straight strokes 10 px wide at 2x with square ends, ink box 56x56 centred in an 80x80 transparent canvas, no box, no glow; reference crop: 14b-settings-graphics.png x 1322-1352, y 125-155 (the boxed X top right)". Then `py -3 tools/Process-KitV5.py` and `python tools/Run-InEditor.py tools/Import-HudKit.py --save`. CODE (Engineer): line 143 `KitImage(WidgetTree, TEXT("close_x"), 28.f)` -> `KitImage(WidgetTree, TEXT("close_x"), 24.f)`. Verify: X ink 17x17 +-1 at 1529,153, stroke 3 px, samples > 200 on all channels.

4. **Crest is a trapezoid with an extra medallion; the concept is an arch with a plain stag.** Concept: a smooth brass-edged arch 150 px wide (feet at x 895 and 1045 on the frame line y 117) rising to an apex at y 76, with an ivory stag ~60x43 inside it (ink from 909,83). Live: a chamfered trapezoid 118 wide (x 895-1013, flat top y 77 from x 925 to 980, so 32 px narrower and the wrong silhouette) plus a 40 px brass-ringed medallion (`icon_stag_emblem`, ring rows y 95-135) that straddles the frame line at y 115-116, with a 14x29 stag inside; the medallion is not in the concept.
   Fix type: ART + CODE. ART (Codex): regenerate `GameClient/ArtSource/UI/Kit-v5/window_crest.png` at 300x84 (2x of 150x42), transparent, no nine-slice, prompt: "Window-frame crest for a dark slate fantasy UI: a smooth semi-elliptical arch 150x42 at 1x, double brass #A98A55 outline (2 px outer, 1 px inner hairline) with a dark slate #0E171C fill, open along the bottom edge so it merges with a horizontal frame line, and an ivory #E8E1CE stag head with antlers about 60x42 centred inside the arch; no circle, no medallion, no text; reference crop: 14b-settings-graphics.png x 770-920, y 60-110". CODE (Engineer): line 133 -> `Put(Stage, KitImage(WidgetTree, TEXT("window_crest"), 150.f, 42.f), 779.f, 65.f, 150.f / S, 42.f / S);` and delete line 134 (the `icon_stag_emblem` Put). Verify: brass apex at y 76 +-2, feet at x 895 and 1045 +-2, no ring crossing the frame line, stag ink >= 50 px wide.

5. **Quality rows have no separators.** Concept: a hairline (31,38,42 on the 14,24,28 well) under each of the ten rows, e.g. y 506-507 across x 1005-1540; the Display column has them in both. Live Quality column: none (x 1100, y 505-515 is flat 16,26,32).
   Fix type: CODE. `SFGraphicsMenu.cpp` lines 365-366, put the row in an overlay with a bottom rule (`Hairline1` as used on line 372): replace `RowSize->SetContent(Line);` with
   `UOverlay* RowStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass()); UOverlaySlot* LineSlot = RowStack->AddChildToOverlay(Line); LineSlot->SetHorizontalAlignment(HAlign_Fill); LineSlot->SetVerticalAlignment(VAlign_Fill); UOverlaySlot* RuleSlot = RowStack->AddChildToOverlay(Hairline1(WidgetTree)); RuleSlot->SetHorizontalAlignment(HAlign_Fill); RuleSlot->SetVerticalAlignment(VAlign_Bottom); RuleSlot->SetPadding(FMargin(7.f * S, 0.f, 10.f * S, 0.f)); RowSize->SetContent(RowStack);`
   Verify: a 1 px hairline at the bottom of each row (y 507, 542, ... +-2) from x 1008 to 1527.

Reported, not counted (kit-wide or owner calls; section 1 wins over the concept where they disagree):
- `tab_selected` (Graphics tab, High preset, enabled Apply, Keep Changes) samples (62,85,67) flat with a dull 1 px edge (128,119,80); canonical is moss #2B3B2C (43,59,44) with a brass #A98A55 edge, and the concept shows a darker bevelled moss plate with a bright brass double frame. dE > 3 against both. Every passing menu uses the piece: kit decision.
- `button_normal` Revert well (14,27,29) vs the concept's charcoal (46,44,41) with an ornate brass frame (dE 9.2); disabled Apply is a dark plate (10,19,23) where the concept dims the moss plate. Section 1 states.
- `button_destructive` fill (132,36,32) vs concept (79,28,30) and canonical crimson #A34842 (163,72,66): the kit piece matches neither; shared with every destructive button.
- Slider fill (113,143,105) vs the concept's darker moss gradient (63-100, 82-123, 56-87) and the keep bar's #7FA07A: pick one moss for fills kit-wide.
- Content at High: concept shows V-Sync On, 85 %, Fullscreen; live High is V-Sync Off, 75 %, Windowed (preset table and `-windowed` capture). Owner call on the table; the On state with its tick is proven on Epic/Ultra Motion Blur.
- The concept draws a scrollbar beside the Quality list; the live list fits and shows none. Correct behaviour, not a defect.
- Tabs are separate chamfered plates (`tab_normal`) rather than the concept's joined strip; LB/RB are brass-outlined pills with a 16 px serif label (legible at 1:1) rather than the concept's grey pill with a bold sans label.

## Preset states (Low, Medium, High, Epic, Ultra) + Custom - FAIL

Behaviour is correct on every frame (`menu-presets-sheet.png`, each capture): exactly one preset plate selected and `Preset: <name>` matching; the Quality legend underline follows; all ten steppers read the level (Low: Nanite Tessellation "Off"); Render Scale 67/70/75/85/100 with the fill ending at the thumb and the thumb inside the track at 100 %; frame cap 60/60/60/120/Unlimited; Motion Blur ticks On from Epic; Apply enabled only when dirty. Custom: Shadows stepped to Ultra, no plate selected, no underline, `Preset: Custom` right edge at x 1537 (inside the frame's inner edge at 1585), Apply enabled. Every frame shows menu items 1-5 and nothing else; no fix items of its own.

## Keep prompt - PASS

Modal 824x341 at 548,379 (dx/dy 0), opaque (no menu text visible inside at 1:1; well 15,25,32), one brass hairline frame with the kit's corner ornament and a top diamond. Title "Keep these display settings?" ink 520x46 vs 524x46; detail ink -5 wide (one point); countdown "Reverting in 14 seconds" not measurable by the diff but present, centred, between the detail and the bar (y ~546-572 vs concept 549-574), ivory; bar frame +2 dy, fill #7FA07A square-ended inside the brass frame; Keep Changes and Revert at 597/976, 629, 350x67 (-2 dy). Screen dims the HUD as in the concept. Countdown ticks (12 s in the concept, 14 s live is timing, not design). Keep Changes and Revert use the kit plates noted above (kit-wide, not counted). The menu items 1-5 are visible through the dim and count once, in the menu verdict.

## Scene captures (Low / High / Ultra) - PASS

`scene-low.png`, `scene-high.png`, `scene-ultra.png` and the sheet: Greenlands_Silverbrook, same view, 1920x1080. Low: no cast shadows under the mannequin, barrels and posts, flat wall lighting, no far grass. High: hard shadows, bounce on the timber walls, grass tufts. Ultra: same as High plus denser far foliage on the hill (x 1100-1500, y 240-400) and a crisper shadow edge. The ladder reads Low < High < Ultra as DEC-05 asks. Nothing broken: no missing materials, no floating props, the street dressing is where a market street would have it. Note for the next round, not a defect of this change: the yellow dev station labels sit in frame at this angle; aim the capture camera past them.

## Fix list

Blocking for this menu (shared fixes first):

1. **Engineer** - Stepper chevrons: line 199 `KitImage("stepper_chevron", 16, 16)` -> `26, 26` (menu item 1). Covers all 13 steppers.
2. **Engineer** - Type sizes: Display labels 20 -> 17 (line 253), Quality labels 18 -> 16 (line 355), slider captions 18 -> 15 (lines 283 and 301) (menu item 2).
3. **Codex + Engineer** - Close X: regenerate `Kit-v5/close_x.png` 80x80 with a 56 px glyph and 10 px strokes (prompt in menu item 3); reprocess and reimport; line 143 `28.f` -> `24.f`.
4. **Codex + Engineer** - Crest: regenerate `Kit-v5/window_crest.png` 300x84 as an arch with the stag inside (prompt in menu item 4); reprocess and reimport; line 133 -> 150x42 at 779,65; delete line 134.
5. **Engineer** - Quality row separators: overlay a bottom `Hairline1` on each row (code in menu item 5).
6. After 1-5: `pwsh tools/Check-HudStyle.ps1`, recapture the seven menu frames and the keep prompt, `py -3 tools/ui_pixel_diff.py --all`, new compare images, fresh Fable reviewer.

Kit-wide or owner call (not blocking this menu; decide once for every menu):

7. **Owner** - `tab_selected`: keep the flat (62,85,67) plate or regenerate it to canonical moss #2B3B2C with a visible brass edge (and bevel, as the concept draws it). Changes every selected tab, row, preset and Keep/Apply plate.
8. **Owner** - `button_normal` Revert well and the dark disabled Apply versus the concept's charcoal well and dimmed moss; `button_destructive` crimson (132,36,32) versus canonical #A34842; one moss for slider and bar fills.
9. **Owner** - High preset table: V-Sync On / 85 % / Fullscreen as drawn, or the current Off / 75 %; capture fullscreen if the concept's "Fullscreen" value should show.
