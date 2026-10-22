# Settings > Graphics: independent review (Fable, 2026-10-22)

Judged against `docs/Game-Design/concept-art/UI/v5/14b-settings-graphics.png`, `14c-settings-keep-revert.png`, DEC-05 (`docs/decisions/OWNER-2026-10-22-decisions.md`), `docs/blueprints/menus.md` sections 1, 2, 5 and `docs/visual-review.md`. Every full-size frame in `captures/` was read at 1920x1080; pixel reports in `diff/*/report.md` were weighed with the note that the AI concept's proportions are loose. All measurements below are at 1920x1080, live minus concept.

Result: 4 PASS, 7 FAIL.

| Screen | Verdict |
| --- | --- |
| Graphics menu (High) | FAIL |
| Preset Low | PASS (behaviour) |
| Preset Medium | PASS (behaviour) |
| Preset High | PASS (behaviour) |
| Preset Epic | PASS (behaviour) |
| Preset Ultra | FAIL (shares every menu defect; behaviour correct) |
| Custom | FAIL (shares every menu defect; behaviour correct) |
| Keep prompt | FAIL |
| Scene Low | FAIL (evidence) |
| Scene High | FAIL (evidence) |
| Scene Ultra | FAIL (evidence) |

Behaviour (highlight, label, strip underline, Apply enable, keep timer) is correct on every state. The menu fails on art treatment and geometry, not on logic. The scene frames fail as evidence, not as rendering.

## Graphics menu (High) - FAIL

Pixel report: 2 of 33 elements PASS. Layout (window 1249x810 at 336,115; tabs; preset buttons; footer) is within tolerance. The failures are the control art and the header ornaments.

1. **Stepper arrows sit low and are text glyphs.** Concept: chevron glyphs vertically centred in their own 36 px end cells with a hairline divider between cell and value; live: `<` `>` EB Garamond text at 22 px, baseline 4-5 px below the value's centre line (measured in `captures/menu-high.png` at the Resolution row: value centre y 496, arrow centre y 500-501), no divider cells. Same on all 13 steppers (3 display, 10 quality).
   Fix type: CODE. `SFGraphicsMenu.cpp` `Stepper` lambda, lines 185-193: replace the `UTextBlock` arrows with `KitImage(WidgetTree, TEXT("chevron_left"|"chevron_right"), 14.f, 14.f)` if the kit has them, otherwise keep the text but drop `SetRenderTranslation(FVector2D(0.f, -4.f))` and set `Arrow->SetFont(RoleFont(TextFamily(), FontSize, false))` with `ArrowSlot->SetVerticalAlignment(VAlign_Center)` and `SetPadding(FMargin(10.f, 0.f))`, and add a 1 px `Hairline()` `Panel` at x 36 and x W-36 inside the overlay (concept divider). Verify: arrow glyph centre within 1 px of the value centre.
2. **Stepper plate art differs from the concept.** Concept: plain rounded well with a single brass hairline (272x37, radius ~4); live: `stepper_field` piece with ornamental brass corner wedges and a visible dark gradient, and the diff found the live plate 41 px wider and 19 px taller than the concept rect (dx -20, dy -10) because the piece's wedges extend past the nine-slice body. Quality rows: live 243 wide but shifted +7 px and 14 px narrower in the body.
   Fix type: ART (shared). `GameClient/ArtSource/UI/Kit-v5/stepper_field.png`, regenerate at 2x (544x74), nine-slice [36,19,36,19] kept, prompt: "UI nine-slice field plate, dark slate well #0E171C with a single 2 px brass #A98A55 hairline border, 8 px corner radius, no corner ornaments, no inner gradient, flat; reference crop: 14b-settings-graphics.png x 582-820, y 416-448 (Resolution stepper)". Then `py -3 tools/Process-KitV5.py` and `tools/Run-InEditor.py tools/Import-HudKit.py --save`. Station panel uses the same piece; recapture it too.
3. **Toggle Off plate is the shield-shaped destructive piece and the cross is invisible.** Concept Off (chosen) is a rounded crimson plate 129x39 with an ivory ✕; live `button_destructive` has pointed brass end caps that overhang the row box by ~8 px each side (x 818-942 against the 820-940 slot) and the `status_cross` glyph renders dark red on crimson (sampled 30,23,18 on the plate vs ivory in the concept), effectively missing at 1:1. Affects V-Sync and Motion Blur rows in every state.
   Fix type: CODE + ART. CODE: `SFGraphicsMenu.cpp` `Toggle` lambda line 210, after `KitImage(...)`, tint the glyph: `if (UImage* Img = Cast<UImage>(Glyph->GetChildAt(0))) { Img->SetColorAndOpacity(Ivory()); }`. ART (shared): `Kit-v5/button_destructive.png`, regenerate at 2x (same canvas), nine-slice [40,28,...] kept, prompt: "UI nine-slice button plate, crimson #A34842 fill, 2 px brass #A98A55 hairline, 8 px radius, straight ends, no pointed caps; reference crop: 14b-settings-graphics.png x 709-820, y 690-725 (Motion Blur Off)".
4. **Close button is a bare glyph.** Concept: 46x46 boxed X (slate plate, brass hairline); live: 40 px `close_x` glyph on nothing, 8 px left and 7 px up of the concept box. Same piece is used by Inventory, Exchange and Gathering Skills, so this is a kit-wide decision; here it reads as unfinished against the concept.
   Fix type: ART (shared). `Kit-v5/close_x.png` at 92x92 (2x of 46), prompt: "UI close button, 46x46 at 1x, slate #17232A plate with 2 px brass #A98A55 hairline and 6 px radius, centred ivory #E8E1CE X stroke 3 px; reference crop: 14b-settings-graphics.png x 1318-1358, y 121-161". CODE: `SFGraphicsMenu.cpp` line 133 `Put(Stage, Close, 1318.f, 121.f, 40.f, 40.f)` -> `Put(Stage, Close, 1313.f, 116.f, 46.f, 46.f)` and `KitImage(..., 46.f)` on line 131.
5. **Controller keycaps are unreadable.** Concept `LB`/`RB`: 48x30 pill, label ~16 px cap height; live: 44x28 rectangle with ~9 px text.
   Fix type: ART (shared). `Kit-v5/keycap_lb.png` and `keycap_rb.png` at 96x60 (2x), prompt: "Controller keycap pill 48x30 at 1x, slate #17232A fill, 2 px brass #A98A55 hairline, full-radius ends, centred label 'LB' (resp. 'RB') in EB Garamond bold ~16 px ivory #E8E1CE; reference crop: 14b-settings-graphics.png x 330-372, y 189-215". CODE: `SFGraphicsMenu.cpp` lines 134-135 size args `44.f, 30.f` and `44.f / S, 28.f / S` -> `48.f, 30.f` and `48.f / S, 30.f / S`.
6. **Header ornaments are missing.** Concept: a crest tab (~120x40 trapezoid rising above the frame top at x 900-1020) holding the stag emblem, and flanking title rules "—◇—" (~130 px each side of "Settings" at y 165); live: stag emblem floats over the plain top border at 920-1000 x 95-135, no crest tab, no rules. Title cap height: concept ~44 px box, live 48 pt bold measures ~55 px tall and 215 px wide against the concept's 161x44 (match 0.49).
   Fix type: ART + CODE. ART: new `Kit-v5/window_crest.png` 240x80 (2x of 120x40), nine-slice none, prompt: "Ornamental brass #A98A55 crest tab for a window frame top edge, trapezoid with chamfered shoulders, dark slate #17232A fill, open at the bottom so it merges with the frame; reference crop: 14b-settings-graphics.png x 780-900, y 68-110". New `Kit-v5/title_rule.png` 260x16 (2x of 130x8), prompt: "Thin brass #A98A55 horizontal rule with a small diamond at the inner end, fades out at the outer end, transparent background; reference crop: 14b-settings-graphics.png x 635-740, y 138-146". CODE: `SFGraphicsMenu.cpp` lines 128-130: `Put(Stage, KitImage(WidgetTree, TEXT("window_crest"), 120.f, 40.f), 779.f, 66.f, 120.f / S, 40.f / S)` before the emblem; `Heading(..., TEXT("Settings"), 42, ...)` (concept box 161x44; re-measure after and settle on the size whose cap height is within 1 px); `Put(Stage, KitImage(WidgetTree, TEXT("title_rule"), 130.f, 8.f), 638.f, 137.f, 130.f / S, 8.f / S)` and a mirrored copy (`SetRenderScale(FVector2D(-1.f, 1.f))`) at x 928.
7. **Render Scale slider has no filled segment and a square thumb.** Concept: track 342x21 with a moss fill from 67% to the thumb, round brass thumb ~22 px; live: unfilled dark track 12 px thick, rounded-square brass thumb ~30x30, value plate uses the ornamental stepper piece.
   Fix type: CODE. `SFGraphicsMenu.cpp` lines 257-266: put a `UProgressBar` styled with `StyleBar(Bar, Moss())` under the slider in an overlay (same `Put` rect 383,638,298,24 with `VAlign_Center`, height 12), set its percent in `Refresh()` to `(Pending.RenderScale - 67) / 33.f`; set `SliderStyle.NormalThumbImage.ImageSize = FVector2D(22.f, 22.f)` (and Hovered/Disabled). ART (shared): `Kit-v5/slider_thumb.png` at 44x44 (2x), prompt: "Round brass #A98A55 slider knob 22 px at 1x with a darker brass rim and a small ivory highlight, transparent background; reference crop: 14b-settings-graphics.png x 568-598, y 636-664".
8. **Selected plate colour is out of tolerance.** `tab_selected` fill samples at (62,85,67) on the Graphics tab and the High preset; dE2000 3.2 and 6.6 against the concept moss (#2B3B2C canonical in menus.md section 1). This is the shared selected piece, so it affects every menu that has already passed; the kit, not this menu, is the place to fix it, and only if the owner wants the darker moss. Reported, not in the fix list.
9. **Revert footer plate colour.** Live Revert well sampled (16,28,30); concept plate is a lighter grey (dE 8.9). Live follows the canonical `button_normal` well; concept disagrees. Section 1 wins per menus.md section 2; reported only.
10. **State data vs concept (reported, owner call, not in the fix list).** Concept High shows V-Sync On, Render Scale 85%, Fullscreen; live High (from `SFGraphicsPresets.cpp` `DefaultRow`) is V-Sync off, 75%, and the capture is Windowed. 75% is tied to the GFX-01 Standard tier so the number stays unless the owner changes the tier; V-Sync default is a tuning value. Window mode is capture environment.

## Preset Low / Medium / High / Epic - PASS (behaviour)

Each frame shows exactly one preset plate in the moss selected state, `Preset: <name>` matches it, the Quality strip underlines the same level, all ten quality steppers read that level (Low: Nanite Tessellation "Off", correct), Render Scale/frame cap/Motion Blur follow the preset table (67/70/75/85/100 %, 60/60/60/120/Unlimited, blur on from Epic), Apply enabled when dirty. Visual defects 1-7 above apply to every frame and are counted once in the menu verdict.

## Preset Ultra - FAIL

Behaviour correct (Ultra selected, Unlimited, 100%, Motion Blur On with tick, strip underline on Ultra). Fails on the same art items 1-7 and nothing else; listed FAIL because the frame is also the first time the slider thumb sits at the track's right end, where the live thumb (30 px square) overhangs the track end by 6 px (thumb right edge x 783 vs track end 777 in `captures/menu-ultra.png`). Fix 7 covers it.

## Custom - FAIL

Behaviour correct: Shadows stepped to Ultra, no preset plate highlighted, `Preset: Custom`, no strip underline, Apply enabled. The status text "Preset: Custom" right edge lands at x 1593, 8 px past the window frame's inner edge at 1585 and overlapping the frame's brass hairline (concept keeps a 24 px inset). Fails on that plus items 1-7.
   Fix type: CODE. `SFGraphicsMenu.cpp` line 158: `PutText(Stage, Status, 1250.f, 269.f)` -> `PutText(Stage, Status, 1350.f, 269.f, 1.f)` (right-align at the Ultra button's right edge, 1337 + 13 in concept units; concept has the label right-aligned to x 1338 at 1080p).

## Keep prompt - FAIL

Modal 823x336 at 548,372 is within 2 px horizontally and 5-7 px short vertically. Behaviour is right: 15 s countdown ticking, bar draining, Keep/Revert wired, Escape reverts.

1. **Keep Changes is brass, concept is moss.** Live primary plate sampled (169,136,82) with dark ink; concept Keep is the moss selected plate with ivory text and brass edge (same as this menu's enabled Apply). dE far beyond 3; match 0.34.
   Fix type: CODE. `SFGraphicsMenu.cpp` line 374: `Button(WidgetTree, TEXT("Keep Changes"), EButtonKind::Primary, 305.f * S)` -> `EButtonKind::Secondary`, then after it `ApplyButtonStyle(Keep.Button, EButtonKind::Secondary, true)` (the same call the enabled Apply uses on line 340).
2. **Content sits 9-22 px too high.** Title dy -22, detail -13, countdown -9: the modal's top padding is 28 px against the concept's ~50 px between frame and title cap.
   Fix type: CODE. `SFGraphicsMenu.cpp` line 356: `FMargin(40.f, 28.f)` -> `FMargin(40.f, 48.f, 40.f, 28.f)`; line 364 detail padding `FMargin(0.f, 14.f, 0.f, 0.f)` -> `FMargin(0.f, 10.f, 0.f, 0.f)`; line 367 countdown `FMargin(0.f, 8.f, 0.f, 12.f)` -> `FMargin(0.f, 4.f, 0.f, 12.f)`. Re-run the diff; target dy within 2 for all four text rows.
3. **Countdown colour.** Live `Hex("BDB6A2")` samples (117,116,108) on the dark plate; concept countdown is ivory. Fix type: CODE. Line 365 `Hex(TEXT("BDB6A2"))` -> `Ivory()`.
4. **Bar fill is the HUD stamina-style bar with an arrow tip.** Concept: moss fill inside a brass-framed 659x18 track, square ends; live: 659x14 `bar_fill_moss` whose brass arrow tip overhangs the track by ~12 px at the right (x 1300 vs track end 1288 in `captures/menu-keep.png`) and no visible track frame. Fix type: CODE. Line 371 `SetHeightOverride(14.f)` -> `18.f`, and wrap `Bar` in `Panel(WidgetTree, WellFill(), Hairline(), FMargin(2.f), 3.f)` so the frame shows; if the overhang persists, it is the shared `bar_fill_moss` piece and goes to ART: `Kit-v5/bar_fill_moss.png`, regenerate with square ends (prompt: "nine-slice bar fill, moss #7FA07A with a 1 px darker rim, square ends, no arrow cap; reference crop: 14c-settings-keep-revert.png x 548-1122, y 508-526").
5. **Title width.** Live 36 pt bold measures 549 wide vs concept 526 (dw +23). Fix type: CODE. Line 359 `36` -> `35`, re-measure; cap height must land within 1 px of the concept's 48 px box.
6. The stepper and toggle defects of the menu (items 1-3 above) are visible through the dim and count once there.

## Scene Low - FAIL (evidence) / Scene High - FAIL (evidence) / Scene Ultra - FAIL (evidence)

Rendering is sensible: Low has no dynamic shadows from the pillars or the mannequin, sky-only reflections on the spheres, no GI bounce (the wall reads flat blue-grey), darker exposure; High has contact shadows, scene reflections on the spheres, GI bounce on the wall; Ultra sharpens the sphere reflections and shadow edges slightly. Low vs High is unmistakable. High vs Ultra is not: the two frames differ only in reflection detail on three spheres and the only preset values that could show on this test map (view distance, foliage, textures, Nanite tessellation, 75% vs 100% render scale) have nothing to act on. A player comparing High and Ultra here would say nothing changed, so the three frames do not demonstrate the preset ladder DEC-05 asks for.
   Fix type: LEVEL (capture). Recapture `scene-low/high/ultra.png` in `/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook` from the same bookmark as the HUD captures (foliage, distant buildings, textured props), 1920x1080 `-game`, same time of day; keep the test-map set as a secondary strip. Acceptance: at 1:1 crop, Ultra vs High shows at least one of: farther foliage/draw distance, higher texture mip, finer shadow cascade on a near prop; Low vs High unchanged from now.

## Fix list

Shared fixes first. Items 1-5 are the top five.

1. **Engineer** - Stepper arrows: `SFGraphicsMenu.cpp` `Stepper` lambda lines 185-193, remove the -4 px render translation, centre the glyph in a 36 px cell with `VAlign_Center`, add the two hairline cell dividers (menu item 1). Covers all 13 steppers.
2. **Codex** - Regenerate `Kit-v5/stepper_field.png` as a plain rounded brass-hairline well (menu item 2, prompt there), reprocess and reimport the kit; recapture station panel too.
3. **Engineer** - Keep prompt: `Keep Changes` to `Secondary` + `ApplyButtonStyle(..., true)` (moss), countdown `Ivory()`, modal top padding 48, text paddings 10/4, bar 18 px in a `Panel` frame, title 35 (keep items 1-5).
4. **Engineer + Codex** - Toggle Off: tint the `status_cross` ivory in the `Toggle` lambda (line 210); Codex regenerates `Kit-v5/button_destructive.png` with straight ends (menu item 3).
5. **Engineer** - Render Scale: moss fill `UProgressBar` under the slider driven from `Refresh()`, 22 px thumb image size; Codex regenerates `Kit-v5/slider_thumb.png` round (menu item 7).
6. **Codex + Engineer** - Close button: boxed 46x46 `close_x.png`; `Put` at 1313,116,46,46 (menu item 4).
7. **Codex + Engineer** - Keycaps: 48x30 `keycap_lb/rb.png` with ~16 px labels; size args updated (menu item 5).
8. **Codex + Engineer** - Header ornaments: new `window_crest.png` and `title_rule.png`, placed per menu item 6; title size 42 then re-measure.
9. **Engineer** - `Preset: Custom` status right-aligned to x 1338 (Custom item).
10. **Engineer** - If the bar tip still overhangs after fix 3, Codex regenerates `bar_fill_moss.png` with square ends (keep item 4).
11. **Engineer** - Recapture `scene-low/high/ultra.png` in Greenlands_Silverbrook from the HUD bookmark; keep the test-map strip as secondary evidence (scene section).
12. After 1-11: `py -3 tools/ui_pixel_diff.py --all`, `pwsh tools/Check-HudStyle.ps1`, new compare images, new Fable reviewer.

Reported, not fixed here (owner or kit-wide calls): `tab_selected` fill dE 3.2-6.6 against canonical moss (menu item 8); Revert well colour vs concept (item 9); High preset V-Sync/85% and Windowed capture vs concept state (item 10).
