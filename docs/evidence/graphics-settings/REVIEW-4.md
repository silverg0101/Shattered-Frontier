# Settings > Graphics: independent review 4 (Fable, 2026-10-22)

Judged against `docs/Game-Design/concept-art/UI/v5/14b-settings-graphics.png` and `14c-settings-keep-revert.png` (LANCZOS-resized to 1920x1080 as `tools/ui_pixel_diff.py` does), DEC-05, `docs/blueprints/menus.md` sections 1, 2, 5 and `docs/visual-review.md`. Every frame in `captures/` (commit 6656a041) was read at 1920x1080 and in 2x/3x crops; `diff/*/report.md` was used for geometry, and every type, art and colour number below was re-measured by me (glyph ink = min channel > 150; brass = R > 120 and R-B > 40; dE2000 on 20-40 px mean samples). All numbers are 1080p px, live minus concept. REVIEW-1/2/3 were background only.

Result: 2 PASS, 2 FAIL.

| Screen | Verdict |
| --- | --- |
| Graphics menu (High) | FAIL |
| Preset states (Low-Ultra) + Custom | PASS (behaviour; inherits the menu's open items) |
| Keep prompt | FAIL |
| Scene captures (Low / High / Ultra) | PASS |

What review 3's list achieved, verified in the images: all 13 chevrons now measure 9x16 at the concept's position (912,488 vs 912,489); the Display and Quality row labels, slider captions, stepper values, preset labels, legend, Auto-Detect and Preset status are all within +-1 px cap height and +-4 px width (Resolution 95x16 vs 95x17, View Distance 114x14 vs 120x14, 100% 39x13 vs 40x14); the Quality column has a hairline under each row; the crest is an arch and the medallion is gone; the title text alone is 157x50 at 884,145 vs 156x53 at 886,136 (the concept's box includes the rule diamonds; the S cap is the real measure, below). Geometry of every panel, tab, plate, well, stepper, toggle, slider and footer button is within +-2 px per the diff reports (31/33 and 5/7 elements, the misses all type or colour). What still fails is below.

## Graphics menu (High) - FAIL

Most visible first.

1. **Stepper chevrons are thin and dim (all 13 steppers).** Concept Resolution right chevron: ink 9x16, mid-row profile 87/228/253/173/39 (a 3 px full-ivory core with hard edges). Live: ink 9x16 at threshold 120 but only 6x12 at 150; mid-row profile 55/127/198/126/60 (a 1 px core at 198 and soft falloff). Cause: `Kit-v5/stepper_chevron.png` is 32x32 with an 11x20 opaque glyph and a ~3 px stroke at 2x, drawn at 26 px, so the stroke lands at 1.2 px and bilinear sampling smears it.
   Fix type: ART (Codex). Regenerate `GameClient/ArtSource/UI/Kit-v5/stepper_chevron.png` at 52x52 (2x of the 26 px draw size), transparent background, no nine-slice. Prompt: "UI stepper chevron pointing right, a single ivory #E8E1CE angle bracket '>' made of two straight strokes 8 px wide with square ends, ink box 18x32 centred in a 52x52 transparent canvas, crisp hard edges, no glow, no box; reference crop: 14b-settings-graphics.png x 1135-1160, y 420-445 (the View Distance right chevron)". Then `py -3 tools/Process-KitV5.py` and `python tools/Run-InEditor.py tools/Import-HudKit.py --save`. `SFGraphicsMenu.cpp` line 198 stays at `26.f, 26.f`. Verify: Resolution right chevron ink 9x16 +-1 at 912,489; mid-row min channel >= 220 on 3 consecutive px.

2. **Crest arch is a narrow spike and the stag is 60 % size.** Brass x-extent per row, concept vs live: y 78 943-985 (42 wide) vs 950-977 (27); y 85 928-1001 (73) vs 940-987 (47); y 95 914-1015 (101) vs 932-995 (63); y 105 903-1024 (121) vs 926-1001 (75); y 112 890-1040 (150) vs 889-1038 (149). The feet match, the body is half the concept's width at every row above them. Stag: concept ivory ink ~40x48 (antlers x 945-985, tips y 87, muzzle y 135, crossing the frame line at y 117); live 26x32 at 951,83, wholly inside the arch. The live piece also draws a brass pendant chevron below the frame line (y 118-128, x 945-975) that the concept does not have.
   Fix type: ART + CODE. ART (Codex): regenerate `GameClient/ArtSource/UI/Kit-v5/window_crest.png` at 300x120 (150x60 at 1x), transparent, no nine-slice. Prompt: "Window-frame crest for a dark slate fantasy UI, 150x60 at 1x: a wide low bell arch, open along its bottom, whose brass #A98A55 double outline (2 px outer, 1 px inner hairline) measures 42 px wide at 2 px below the apex, 73 wide 9 px down, 101 wide 19 px down, 121 wide 29 px down and 150 wide at the bottom edge of the arch at 42 px down, dark slate #0E171C fill inside the arch; an ivory #E8E1CE stag head with antlers, 40 px wide and 48 px tall, centred horizontally, antler tips 12 px below the canvas top, muzzle at the canvas bottom (it hangs below the arch's open bottom edge); nothing else below the arch, no pendant, no circle, no medallion; reference crop: 14b-settings-graphics.png x 770-920, y 60-120". Reprocess and reimport as in item 1. CODE (Engineer): `SFGraphicsMenu.cpp` line 133 `Put(Stage, KitImage(WidgetTree, TEXT("window_crest"), 150.f, 42.f), 774.f, 65.f, 150.f / S, 42.f / S);` -> `Put(Stage, KitImage(WidgetTree, TEXT("window_crest"), 150.f, 60.f), 774.f, 65.f, 150.f / S, 60.f / S);`. Verify: brass extents at y 85 928-1001 +-3 and y 105 903-1024 +-3; stag ink 40x48 +-3 with bottom at y 135 +-2 and 10 px clear of the title's cap top (y 145); no brass below y 118 except the frame line.

3. **Render Scale fill is a pale highlighted band; the concept is a dark moss band.** Concept fill column at x 500: 76/96/70, 70/89/64, 67/85/61, 73/90/68 (mean #465A41, flat); live: 154/172/144, 157/187/148, 111/141/103, 99/125/92 (a lit gradient from `bar_fill_moss`). Mean fill dE2000 29.5. Thumb: concept 25x25 brass at 659,734; live 22x22 (dw -3).
   Fix type: CODE (Engineer). `SFGraphicsMenu.cpp` line 295 `FProgressBarStyle FillStyle = ScaleFill->GetWidgetStyle(); FillStyle.BackgroundImage = FSlateNoResource(); ScaleFill->SetWidgetStyle(FillStyle);` -> `FProgressBarStyle FillStyle = ScaleFill->GetWidgetStyle(); FillStyle.BackgroundImage = FSlateNoResource(); FillStyle.FillImage = Frame(FLinearColor::White, FLinearColor::Transparent, 2.f); ScaleFill->SetWidgetStyle(FillStyle); ScaleFill->SetFillColorAndOpacity(Hex(TEXT("465A41")));` (the keep bar on line 411-412 already uses this pattern). Line 288 `ThumbBrush.ImageSize = FVector2D(22.f, 22.f)` -> `FVector2D(25.f, 25.f)`. Verify: fill sample at x 470-500, y 742-750 within dE 3 of (70,90,65); thumb brass ink 25x25 +-1.

4. **Disabled Apply is a dark well with a near-invisible label; the concept dims the moss plate.** Concept plate (40,50,41) with label ink (78,85,78) (found at ink threshold 110); live plate (9,19,22), dE2000 12.2, label (30,38,39) (found only at threshold 90). Cause: `ApplyButtonStyle(..., bToggledOn=true)` sets `Style.Disabled = ButtonBrush(Kind, Disabled)`, the normal well at tint 0.5, so a toggled-on button loses its plate when disabled.
   Fix type: CODE (Engineer, shared helper). `SFHudStyle.h` line 378 `Style.Disabled = ButtonBrush(Kind, EHudState::Disabled);` -> `FSlateBrush DimOn = TabBrush(EHudState::Selected); DimOn.TintColor = FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f, 1.f)); Style.Disabled = bToggledOn ? DimOn : ButtonBrush(Kind, EHudState::Disabled);` and add to menus.md section 1 Button row: "toggled-on + disabled = moss plate at tint 0.8". Label: add `TWeakObjectPtr<UTextBlock> ApplyLabel;` to `SFGraphicsMenu.h` line 76, set `ApplyLabel = Apply.Label;` after line 380, and in the refresh at line 521 `if (ApplyButton.IsValid()) { ApplyButton->SetIsEnabled(EditSession->IsDirty()); }` -> `const bool bDirty = EditSession->IsDirty(); if (ApplyButton.IsValid()) { ApplyButton->SetIsEnabled(bDirty); } if (ApplyLabel.IsValid()) { ApplyLabel->SetColorAndOpacity(FSlateColor(FLinearColor(Ivory().R, Ivory().G, Ivory().B, bDirty ? 1.f : 0.45f))); }`. Verify on the High frame: plate sample at 700-740, 875-905 within dE 4 of (40,50,41) (the remaining gap is the kit moss, item 10); "Apply" ink found at threshold 110 with width 68 +-3.

5. **Tab labels are one to two points large.** Concept "Audio" A cap 16, ink 56x17; live cap 18, ink 66x19. "Graphics" ink 84x23 vs 93x27 (G cap 17 both, the descender and width give it away). Section 1 puts tab text at 18-20 px; the concept draws ~25 px; the live is 27 (line 154).
   Fix type: CODE (Engineer). `SFGraphicsMenu.cpp` line 154 `Text(WidgetTree, TabNames[Index], 27, Ivory())` -> `Text(WidgetTree, TabNames[Index], 25, Ivory())` (19 pt). Verify: Audio A cap 16-17, ink width 56-62; Graphics ink width 84-89.

6. **Close X glyph is 2 px thick and dim; the concept is 4 px and full ivory.** Concept ink 17x18 at 1528,153, mid-row 4 px > 150. Live ink 14x16 at 1530,154 (12x16 at threshold 150), mid-row 2 px. Cause: `close_x.png` is 80x80 with a 56x56 glyph drawn at 24 px (3.3x downscale, no mips), so its strokes thin and grey out. The Off cross reuses it at 14 px (line 236) and shows the same: concept "x" 12x12 bright, live ~9x9 dim (the "x Off" ink is 27 vs 37 wide).
   Fix type: ART + CODE. ART (Codex): regenerate `GameClient/ArtSource/UI/Kit-v5/close_x.png` at 48x48 (2x of the 24 px draw), transparent, no nine-slice. Prompt: "UI close glyph, a single ivory #E8E1CE X of two straight diagonal strokes 8 px wide with square ends, ink box 34x34 centred in a 48x48 transparent canvas, crisp hard edges, no box, no glow; reference crop: 14b-settings-graphics.png x 1322-1352, y 125-155". Reprocess and reimport. CODE (Engineer): line 142 stays `24.f`; line 236 `KitImage(WidgetTree, TEXT("close_x"), 14.f)` -> `16.f`. Verify: close X ink 17x17 +-1 at 1528,153 with 4 px > 200 on the mid row; Off cross ink 12x12 +-1.

7. **Title cap height +2.** "Settings" S cap: concept 28x33 at 886,145; live 28x35 at 884,145 (width 157 vs 156 passes). Heading is 53 px = 40 pt.
   Fix type: CODE (Engineer). `SFGraphicsMenu.cpp` line 134 `Heading(WidgetTree, TEXT("Settings"), 53, Ivory())` -> `52` (39 pt). Verify: S cap 33-34, ink width 153-157.

Reported, not counted (kit-wide or owner calls; section 1 wins over the concept, so they are listed once at the end):
- `tab_selected`: Graphics tab (43,59,44) vs concept (59,75,55), dE 5.5; High preset (43,59,44) vs (50,65,47), dE 2.3. The concept's selected plate also carries a bright double brass frame with corner brackets and a bevel; the kit's is a 1 px hairline. The concept joins the five tabs into one strip; the kit draws separate plates with 4 px gaps.
- `button_normal` well: Revert (14,26,28) vs concept charcoal (42,40,38), dE 9.3; close box (14,23,29) vs (27,35,38), dE 4.3. The concept's footer buttons have an ornate double brass frame with corner brackets.
- `button_destructive`: Off plate (110,27,24) vs concept (80,33,34), dE 8.9; canonical #A34842 (163,72,66) matches neither.
- `keycap_lb/rb`: concept is a grey pill (light (160,170,175) outline) with bold white sans "LB" ~26x18; live is a dark pill with a brass outline and a 16x15 serif label. 24 keycap pieces share the style.
- High preset content: concept shows V-Sync On, 85 %, Fullscreen; live High is Off, 75 %, Windowed. The On-selected state with its tick is proven on Epic/Ultra Motion Blur.
- "Quality" heading ink 82 wide vs 87 and "View Distance" 114 vs 120 at matching cap height: the concept face is wider; not a size fault.
- The concept's scrollbar beside the Quality list is absent; the live list fits. Correct.

## Preset states (Low, Medium, High, Epic, Ultra) + Custom - PASS (behaviour)

`menu-presets-sheet.png` and each capture: exactly one preset plate selected and `Preset: <name>` matching; the Quality legend underline follows; all ten steppers read the level (Low: Nanite Tessellation Off); Render Scale 67/70/75/85/100 with the fill ending at the thumb and the thumb inside the track at both ends; frame cap 60/60/60/120/Unlimited; Motion Blur ticks On from Epic; Apply enabled (moss) only when dirty, so disabled only on the High frame. Custom: Shadows stepped to Ultra, no plate selected, no legend underline, `Preset: Custom` ends at x 1537 inside the frame, Apply enabled. Nothing clipped or overlapping on any frame. No fix items of its own; every frame carries menu items 1-7 and is accepted when the menu is.

## Keep prompt - FAIL

Modal 824x341 at 548,379 (dx/dy 0), opaque, brass hairline frame with the corner ornament and top diamond; HUD dimmed as in the concept; bar frame at 630,586 660x18 (+1 wide), fill square-ended, 91 % at 14 s; buttons 350x67 at 597/976, 629 (-2 dy). Detail "Resolution 2560 x 1440, Fullscreen" R cap 19 vs 18, ink 387 vs 391 wide; countdown R cap 17 vs 17, 244 vs 243 wide: pass.

1. **Button labels are a third too small for their plates.** "Keep Changes": concept ink 179x30, K cap 21; live 128x24, K cap 16 (-5). "Revert": concept 81x21, R cap 21; live 60x16, R cap 16. The plates were sized from the concept (67 tall) but the labels stayed at the secondary default (24 px); the concept draws ~32 px.
   Fix type: CODE (Engineer). `SFGraphicsMenu.cpp` after line 419 (`const FSFHudButton Keep = Button(...)`) add `Keep.Label->SetFont(RoleFont(TextFamily(), 32, false));` and after line 423 (`const FSFHudButton Undo = Button(...)`) add `Undo.Label->SetFont(RoleFont(TextFamily(), 32, false));` (same pattern as the footer's lines 380 and 384). Verify: K cap 20-22, "Keep Changes" ink 175-183 wide; R cap 20-22, "Revert" ink 78-84 wide.

2. **Title cap height +2.** "Keep these display settings?" K cap: concept 30x31 at 701,447; live 30x33 at 697,445 (ink 524 wide both). Heading is 50 px = 38 pt.
   Fix type: CODE (Engineer). Line 399 `Heading(WidgetTree, TEXT("Keep these display settings?"), 50, Ivory())` -> `48` (36 pt). Verify: K cap 31-32, ink width 505-528.

3. **Bar fill colour.** Concept (85,127,62) saturated green; live #7FA07A (127,160,122), dE2000 13.2.
   Fix type: CODE (Engineer). Line 412 `Bar->SetFillColorAndOpacity(Hex(TEXT("7FA07A")));` -> `Hex(TEXT("557F3E"))`. Verify: fill sample at 700-760, 590-600 within dE 3 of (85,127,62). (Section 1 names #7FA07A only for the hold ring arc; if the owner wants one progress moss kit-wide, that is item 14.)

Reported, not counted: Keep Changes plate (43,59,44) vs (38,51,35) dE 2.7 passes colour but lacks the concept's double brass frame with corner brackets (`tab_selected`, item 10); Revert well (14,26,28) vs charcoal (29,29,28), dE 6.6 (`button_normal`, item 11). Countdown 14 s vs 12 s is timing.

## Scene captures (Low / High / Ultra) - PASS

`scene-low.png`, `scene-high.png`, `scene-ultra.png` (Greenlands_Silverbrook, same view, 1920x1080) and the sheet, compared on the same crops (820-1460 x 180-720 and 1100-1700 x 150-450). Low: no cast shadows under the mannequin, barrels, stalls or awning, flat walls, bare hill. High: hard shadows under everything, bounce on the timber, same bare hill. Ultra: as High plus grass tufts across the hillside (x 1100-1500, y 240-400) and a crisper shadow edge on the paving. The ladder reads Low < High < Ultra as DEC-05 asks. No missing materials, no floating props, the street dressing sits where a market street would have it. The yellow dev station labels in frame are a development fixture, not part of this change; aim the next capture past them.

## Fix list

Blocking for this menu (shared fixes first):

1. **Engineer** - `SFHudStyle.h` line 378: toggled-on buttons keep the moss plate at tint 0.8 when disabled (menu item 4); note the state in menus.md section 1.
2. **Engineer** - `SFGraphicsMenu.cpp`: Apply label alpha 0.45 when not dirty via a stored `ApplyLabel` (menu item 4).
3. **Codex** - Regenerate `Kit-v5/stepper_chevron.png` 52x52, 18x32 glyph, 8 px strokes (prompt in menu item 1); `py -3 tools/Process-KitV5.py`; `python tools/Run-InEditor.py tools/Import-HudKit.py --save`. Code stays at 26 px.
4. **Codex + Engineer** - Regenerate `Kit-v5/close_x.png` 48x48, 34x34 glyph, 8 px strokes (prompt in menu item 6); reprocess and reimport; line 236 Off cross `14.f` -> `16.f`.
5. **Codex + Engineer** - Regenerate `Kit-v5/window_crest.png` 300x120 as the wide bell arch with a 40x48 stag crossing the frame line, no pendant (prompt and row widths in menu item 2); reprocess and reimport; line 133 -> `KitImage(..., 150.f, 60.f), 774.f, 65.f, 150.f / S, 60.f / S`.
6. **Engineer** - Render Scale: line 295 flat `Frame` fill tinted `465A41`; line 288 thumb 22 -> 25 (menu item 3).
7. **Engineer** - Type: line 154 tab labels 27 -> 25; line 134 title 53 -> 52; line 399 keep title 50 -> 48; keep buttons' labels 32 px after lines 419 and 423 (menu items 5, 7; keep items 1, 2).
8. **Engineer** - Line 412 keep bar fill `7FA07A` -> `557F3E` (keep item 3).
9. After 1-8: `pwsh tools/Check-HudStyle.ps1`, recapture the seven menu frames and the keep prompt, `py -3 tools/ui_pixel_diff.py --all`, new compare images, fresh Fable reviewer.

Kit-wide or owner call (not blocking this menu; decide once for every menu):

10. **Owner** - `tab_selected`: keep the flat (43,59,44) hairline plate, or regenerate it to the concept's moss with a bright double brass frame, corner brackets and bevel, and join the tab strip. Changes every selected tab, row, preset, Apply and Keep plate (dE 5.5 on the tab today).
11. **Owner** - `button_normal` well: slate (14,26,28) today vs the concept's charcoal (42,40,38) with an ornate double frame; Revert dE 9.3, close box dE 4.3, keep Revert dE 6.6.
12. **Owner** - `button_destructive`: (110,27,24) today vs concept (80,33,34) vs canonical #A34842; pick one and regenerate (dE 8.9).
13. **Owner** - Keycaps: brass-outlined dark serif pills today vs the concept's grey controller glyphs with bold sans labels; 24 pieces share the style.
14. **Owner** - One progress moss: after items 6 and 8 the menu matches its concept (slider #465A41, bar #557F3E) while section 1's hold arc is #7FA07A; unify if wanted.
15. **Owner** - High preset table: V-Sync On / 85 % / Fullscreen as drawn, or the current Off / 75 % / Windowed.
