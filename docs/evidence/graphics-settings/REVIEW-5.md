# Graphics settings menu - review 5 (independent, Fable)

Judged from the images only: concepts `docs/Game-Design/concept-art/UI/v5/14b-settings-graphics.png` and `14c-settings-keep-revert.png` (1672x941, scaled to 1920x1080 for every number below), the live frames in `captures/`, the compare sheets and the `diff/*/report.md` tables. Text was re-measured by glyph ink (cap height of the first letter, same threshold on both images), so the diff tool's "title Settings" line-box row (which contains the crest) is replaced by the numbers here. Tolerances: geometry +-2 px, cap height +-1 px, dE2000 <= 3, art at 1:1 crop.

Type check (concept cap / live cap, px): Settings 33/35; Graphics Preset 21/22; Display 20/21; Quality 27/28 (box); tab Graphics 17/16; Resolution 16/16; preset High 16/16; View Distance 14/13; Auto-Detect 13/13; Revert 19/19; render-scale value 16/19; 67% 14/13; keep title 31/32; keep detail 18/19; countdown 17/17; Keep Changes 21/21. Everything but the title (+2) and the render-scale value (+3) is within tolerance, and the family and weight match (EB Garamond regular).

Result: 1 PASS, 3 FAIL.

## Graphics menu (High) - FAIL

Layout, proportions and element set are the concept's: every box in the diff table except the type rows sits within +-2 px (33 of 33 geometry rows), the crest, close box, tab strip, preset row, both columns, slider, legend and footer are where the concept puts them. What fails is a handful of measurable details, listed most visible first.

1. **Render-scale value box frame.** Concept: an 88x43 well (x 857-945, y 724-767) with a 2 px bright brass outer line, a 1 px inner line 3 px inside and 8 px chamfered corners. Live: a 1 px dim hairline (no pixel at y=730 or x=900 passes the brass test the concept passes). Art at 1:1 does not match.
   - Fix type: ART. `GameClient/ArtSource/UI/Kit-v5/field_well.png`, 220x76 (2x of 110x38), nine-slice 16/16/16/16 unchanged. Prompt: "Dark-slate UI text field, 220x76 px, transparent outside. Outer frame a 4 px antique-brass line (#C9A961 with a 1 px darker #8A6E3C edge), corners chamfered 16 px, a second 2 px brass line 6 px inside the outer one, interior flat #0E171C. Flat, no glow. Reference: concept 14b-settings-graphics.png crop x 746-823, y 630-668 (the 85 % box)." Then `py -3 tools/Process-KitV5.py` and `python tools/Run-InEditor.py tools/Import-HudKit.py --save`. The steppers (line 190) share this piece; the concept's stepper fields carry the same inner line, so one piece serves both.

2. **Render-scale value type.** Concept "85%" cap 16 px, live "75%" cap 19 px (+3).
   - Fix type: CODE. `SFGraphicsMenu.cpp` line 305: `Num(WidgetTree, TEXT("75%"), 27, Ivory())` -> `Num(WidgetTree, TEXT("75%"), 23, Ivory())`.

3. **Slider track ends.** Concept: the track is a pointed bar; both ends taper to a 1 px point over 10 px (right tip at x 782, y 746). Live: the right end is a 9x10 px knot of doubled line at x 766-774 (8-10 bright pixels per column vs the concept's 3-6) and the track stops 8 px short of the concept's tip; the left end is the same knot behind the 67 % label.
   - Fix type: ART + CODE. ART: regenerate `Kit-v5/slider_track.png` 600x24 (2x), nine-slice 36/6/36/6 unchanged: "Thin horizontal UI slider track, 600x24 px, transparent outside, both ends tapered to a sharp point over the outer 36 px, 2 px antique-brass #A98A55 outline, interior #0E171C, no knobs, no loops, no highlight. Reference: concept 14b crop x 383-683, y 640-653 (1672 space)." CODE: line 296 `Put(Stage, KitPanel(... TEXT("slider_track") ...), 389.f, 644.f, 286.f, 12.f)` -> width `292.f` so the tip lands at x 782.

4. **Title cap height.** Concept "Settings" S cap 33 px, live 35 px (+2); the ink box is 156x53 vs 152x50.
   - Fix type: CODE. Line 134: `Heading(WidgetTree, TEXT("Settings"), 52, Ivory())` -> `49`.

5. **Tab labels sit low.** Concept "Graphics" cap top y 223 in a tab spanning 207-262 (cap centre 3 px above the tab centre); live cap top 228 (1.5 px below centre): the label is 5 px lower than the concept's in a tab that is itself within 0 px. Same on all five tabs.
   - Fix type: CODE. Lines 154-157, after `Tab->SetContent(Label)`: add `Tab->SetPadding(FMargin(0.f, 0.f, 0.f, 9.f));` (concept-space units; the centred label moves up 4.5 units = 5 px).

6. **Quality list scrollbar missing.** Concept: a 6 px wide track at x 1350-1356 from y 366 to 728, dim slate with brass end caps and a brass thumb. Live: nothing (the ScrollBox hides its bar because ten rows fit).
   - Fix type: CODE. After line 351 (`List->SetScrollbarThickness(FVector2D(6.f, 6.f));`): `List->SetAlwaysShowScrollbar(true);` and in the bar style set `BarStyle.VerticalBackgroundImage = Frame(Hairline(), Hairline(), 3.f);` (keep the amber thumb), so the track shows at the concept's position.

7. **Title rules.** Concept: one plain rule each side (x 735-855 and 1010-1135, 1 px) ending in a single 10 px diamond beside the word. Live: each side shows three diamonds (a 5 px cap at the far end x 736-740 / 1180-1184, a 5 px cap at 839-843 / 1077-1081 from `section_rule`'s own art, plus the 8 px `quest_diamond` at 857-864 / 1064-1071).
   - Fix type: CODE. Lines 137 and 140: replace the two `KitImage(WidgetTree, TEXT("section_rule"), 110.f, 4.f)` with a plain brass hairline, `Hairline1(WidgetTree)` sized 110x2, at the same positions, so only the `quest_diamond` beside the title remains.

8. **Apply/Revert divider.** Concept: line-diamond-line, 42 px wide (x 940-982, y 889). Live: the 16 px diamond alone.
   - Fix type: CODE. Line 388: keep the `quest_diamond` and add `Hairline1` 12x2 at concept x 816 and 846, y 774 (units as the file uses), i.e. `Put(Stage, Hairline1(WidgetTree), 814.f, 774.f, 12.f, 2.f)` and `Put(Stage, Hairline1(WidgetTree), 846.f, 774.f, 12.f, 2.f)`.

Kit-wide, weighed and not counted against this menu (section 1 makes the kit piece canonical; the concept disagrees with it on every menu):

- `tab_selected` (Graphics tab and the High preset): live flat (43,59,44) with a thin edge vs concept (68,86,62) with a bright double brass frame and corner brackets; dE 7.7 on the tab. Also the concept joins the five tabs into one strip with 1 px dividers; live leaves 3-4 px gaps.
- `button_normal` (Revert, keep Revert, close box): live slate (15,26,29) vs concept charcoal (34,34,34) with the ornate double frame; dE 9.2.
- `button_destructive` (x Off): live (110,27,24) vs concept (80,33,34).
- Keycaps LB/RB: brass-outlined dark serif pills vs the concept's grey controller glyphs.
- `window_frame`: concept has a second inner line, corner filigree (26x26 at each corner), a side diamond at the footer rule and a pendant diamond under the bottom edge; live has a single line with a chamfered bracket.

Behaviour not visible in a frame (presets as templates, Custom on any change, 15 s revert, auto-detect) is taken from the four passing automation tests and is not graded here.

## Preset states + Custom - PASS

`menu-presets-sheet.png` and `menu-custom.png`: each preset frame selects exactly one plate, moves the Quality legend underline to the matching word, sets every quality stepper to the preset word (Low shows Nanite Tessellation Off, Ultra shows Unlimited frame rate and 100 % scale), and "Preset: <name>" follows. Custom (Shadows raised to Ultra from High) deselects all five plates, drops the legend underline, reads "Preset: Custom" and lights Apply; clean High shows the dimmed Apply label on its moss plate as review 4 asked. The On toggles show the tick on moss (Epic and Ultra motion blur), the Off toggles the cross on crimson. No clipping, overlap or missing text in any of the six frames. The kit-wide plate notes above apply to every frame equally.

## Keep prompt - FAIL

Geometry: frame, bar and both buttons within +-2 px; title cap 31/32, detail 18/19, countdown 17/17, Keep Changes 21/21 (the diff tool's "REVIEW" countdown row is `TextBlock_65`, a full-width block; measured by ink it passes). Bar fill (85,127,62) vs (80,122,60), well and plates within dE 3 except the kit-wide Revert plate.

1. **Top ornament missing.** Concept: a hairline rule across the modal body at y 408 from x 760 to 1160 with a 16 px brass diamond at the centre (x 952-968), 36 px below the frame's top edge. Live: nothing between the frame edge and the title.
   - Fix type: CODE. `SFGraphicsMenu.cpp` after line 397 (`Modal->SetContent(Body);`): insert as the first child of `Body` a `UHorizontalBox` with `Hairline1` 170x2 (VAlign_Center), `KitImage(WidgetTree, TEXT("quest_diamond"), 14.f)` and `Hairline1` 170x2, HAlign_Center, padding `FMargin(0, 0, 0, 14.f)`; reduce the modal top padding at line 396 from `41.f` to `27.f` so the title stays at y 445.

Kit-wide: `modal_frame` lacks the concept's inner second line; `tab_selected` (Keep Changes) and `button_normal` (Revert) as above.

## Scene captures - FAIL

`scene-low.png` vs `scene-high.png` show the preset change: Low has no mannequin shadow, flat grass without blades and softer distant houses; High adds the shadow, grass and sharper facades (22 % of pixels differ by more than 20/255). `scene-ultra.png` is not distinguishable from `scene-high.png`: mean luminance difference 3.0/255 (TAA noise level), 0.5 % of pixels over 40, no region of the frame changes (the far street x 1250-1900, y 60-330 and the shadow x 700-1200, y 560-900 are the same). The Ultra frame therefore does not show Ultra.

1. **Ultra evidence.** Fix type: CODE (capture). `tools/Capture-GraphicsMenu.py` line 26: after `SFGfx apply Ultra` wait for the scalability change to land (the 6 s may precede `sg.*` taking effect; log `sg.ShadowQuality`, `sg.ViewDistanceQuality`, `sg.FoliageQuality` into the frame with `stat none` + a `DisplayAll`-style text or write them to `scene-<preset>.json` next to the png), and pick the shot so a shadow-cascade edge or a foliage cull distance is in view (a view down the street with the far field at x > 1400). A frame where Ultra and High differ, or a readout proving the cvars differ, closes this item.
2. Note, not graded: the yellow mirrored "Forge / Workbench / E: craft" prompts at x 300-900, y 380-520 are world text components seen from behind; keep them out of evidence frames (`showflag` or a different bookmark) so a reader does not take them for menu text.

## Fix list

Blocking for this menu:

1. **Codex** - Regenerate `Kit-v5/field_well.png` 220x76, margins 16, to the concept's 85 % box (prompt in menu item 1); `py -3 tools/Process-KitV5.py`; `python tools/Run-InEditor.py tools/Import-HudKit.py --save`.
2. **Codex** - Regenerate `Kit-v5/slider_track.png` 600x24, margins 36/6/36/6, pointed ends (prompt in menu item 3); reprocess and reimport.
3. **Engineer** - `SFGraphicsMenu.cpp` line 296 track width 286 -> 292 (menu item 3).
4. **Engineer** - Line 305 value `27` -> `23`; line 134 title `52` -> `49` (menu items 2, 4).
5. **Engineer** - Lines 154-157 `Tab->SetPadding(FMargin(0.f, 0.f, 0.f, 9.f))` (menu item 5).
6. **Engineer** - After line 351 `List->SetAlwaysShowScrollbar(true)` with a hairline track background (menu item 6).
7. **Engineer** - Lines 137/140 section_rule -> plain 110x2 hairline; line 388 add the two 12x2 hairlines beside the diamond (menu items 7, 8).
8. **Engineer** - Keep prompt: ornament row at the top of `Body`, modal top padding 41 -> 27 (keep item 1).
9. **Engineer** - `tools/Capture-GraphicsMenu.py`: Ultra scene shot that shows Ultra, or a cvar readout per preset frame (scene item 1); keep the mirrored world prompts out of frame.
10. After 1-9: `pwsh tools/Check-HudStyle.ps1`, recapture the seven menu frames, the keep prompt and the three scenes, `py -3 tools/ui_pixel_diff.py --all`, new compare images, fresh Fable reviewer.

Kit-wide / owner call (decide once for every menu; not blocking here):

11. **Owner** - `tab_selected`: flat (43,59,44) hairline plate vs the concept's (68,86,62) with double brass frame and brackets; and whether tabs join into one strip (every selected tab, row, preset, Apply and Keep plate).
12. **Owner** - `button_normal`: slate well vs the concept's charcoal with an ornate double frame (Revert, keep Revert, close box).
13. **Owner** - `button_destructive`: (110,27,24) vs concept (80,33,34) vs canonical #A34842.
14. **Owner** - Keycaps: brass serif pills vs grey controller glyphs (24 pieces).
15. **Owner** - `window_frame` and `modal_frame`: single line + bracket vs the concept's double line, corner filigree and pendant diamond.
