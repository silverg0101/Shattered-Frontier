# Dialogue UI r16 review (Fable, independent)

Concept: `docs/Game-Design/concept-art/UI/v5/10-npc-dialogue.png` (1672x941, x1.1483 to 1080p). Live: `r16/dialogue-open.png`, `dialogue-more.png`, `dialogue-accepted.png`, pairs `*-compare.png`. Numbers: `r16/pixel/10-npc-dialogue/report.md`, `r16/ink-measure.txt`, plus my own 1:1 / 2x-5x crops and dE2000 samples (concept resized to 1920x1080, LANCZOS). Spec: `docs/blueprints/menus.md` §1-2, `visual-review.md`, `UI/v5/REVIEW.md` 10-npc-dialogue note. r3/r6 read for their ask lists only; nothing assumed fixed. Backdrop and the mannequin body are out of scope; the board, the speaker's placement, the dimmed HUD and the tracker are judged. Cross-screen: `ui-v5/final3/trade-live.png`, `market-board-live.png`, `inventory-live.png`, `build-mode-live.png`.

Measured (live, 1080p): board frame x 486-1437, y 219-841 (concept rect exact, IoU 1.00). Portrait, divider, rows, key caps, art well, objective rows, roundel, coin, Accept (205x54) and Decline (164x54) all within +-1 px of the concept rects. Text ink within +-2 px left/top and +-4 px height except the two button labels (+5/+6 px tall, +20/+21 px wide). HUD dim: hotbar numeral, HP text, tracker title, footer and badge all measure (139,135,124) against the undimmed master's (232,225,206) = 60 %, i.e. the 40 % dim is applied. Tracker shows "The Mill's Supply / Gather Oak Logs 0 / 6 / Deliver to Oswin 0 / 1" in all three frames. Board fill over the backing (15,26,33) vs concept (16,24,27), dE 2.6-2.8: inside tolerance; nothing shows through the board.

## 10-npc-dialogue — FAIL

The geometry is the concept's and a player would recognise the layout (portrait + name, numbered rows, quest card with art well, Objectives/Rewards, Accept/Decline, synced tracker, dimmed HUD). It fails on broken and wrong-state frames and on four elements that do not match at 1:1: `dialogue-more.png` has the speech running through the divider and under row 1; `dialogue-open.png` shows row 3 selected instead of the greeting's row 1 (and `dialogue-more` row 3 instead of row 1); the key caps are a different piece of art (tan bevelled block, dE 28.6 on fill); the Objectives/Rewards headings are brass bold instead of ivory regular (dE 28); and the Accept/Decline labels are 32 px type instead of 24 px (cap +5 px, shared helper, every screen). Items 1-5 each block on their own.

### 1. Speech overflows the divider and row 1 (`dialogue-more.png`)
- **What's wrong:** the node after "Tell me more" is five wrapped lines at 320 px: ink y 340-489. The divider sits at y 425-433 and row 1 at y 460-532, so lines 3-5 cross the rule and "season." is drawn under the row plate (half hidden). Broken element: overlap and truncation.
- **Fix type:** CODE (Engineer), plus the content rule.
- **Instruction:** `SFDialogue.cpp` `USFDialoguePanel::NativeOnInitialized`: the left column below the name must flow, not sit on fixed rects. Replace `Line(Board, LineText, 20, 662.f, 341.f);`, `Box(Board, Rule(WidgetTree), 516.f, 425.f, 466.f, 8.f);` and the per-row `Box(Board, Stack, 513.f, RowTops[Index], 472.f, 72.f);` with one `UVerticalBox* Flow` placed by `Box(Board, Flow, 513.f, 332.f, 472.f, 500.f)`:
  ```cpp
  UVerticalBox* Flow = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
  Flow->AddChildToVerticalBox(LineText)->SetPadding(FMargin(662.f - 513.f, 0.f, 0.f, 0.f));          // speech block top = 341 - 0.43*20 = 332
  Flow->AddChildToVerticalBox(Rule(WidgetTree))->SetPadding(FMargin(3.f, 32.f, 3.f, 27.f));          // 2-line speech ends y 393; rule 425-433; row 1 at 460
  // in the row loop, instead of Box(...):
  USizeBox* RowBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); RowBox->SetHeightOverride(72.f); RowBox->SetContent(Stack);
  Flow->AddChildToVerticalBox(RowBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 13.f));                     // concept pitch 85-86
  Box(Board, Flow, 513.f, 332.f, 472.f, 500.f);
  ```
  With the two-line greeting every element lands on its concept rect (+-1 px); a longer line pushes the rows down instead of through them. Set `Frame->SetClipping(EWidgetClipping::ClipToBounds)`. Content rule: a speech line may not exceed 3 wrapped lines at 320 px (~95 characters); rewrite the "Tell me more" node ("The wheel is idle until I can saw the oak. Six logs from the river road will keep the stones turning.") wherever the Oswin tree is authored, and add a check in the tree loader that logs any line wider than 3 lines.

### 2. Selected row is the hovered row, not row 1 (`dialogue-open`, `dialogue-more`)
- **What's wrong:** concept and brief: row 1 selected on open. Live open: row 3 "What is the reward?" moss, rows 1/2/4 hairline; live more: row 3 "Farewell" moss. Only `dialogue-accepted` shows row 1. `report.md` row 1 selected fill dE 15.2 is this. Cause: `NativeTick` line 420 sets `Selected` to whichever row the idle cursor rests on (the capture leaves the mouse at ~y 660).
- **Fix type:** CODE (Engineer).
- **Instruction:** `SFDialogue.cpp` `NativeTick`: only take hover when the cursor moved. Add `FVector2D LastMouse = FVector2D(-1.f, -1.f);` to the class, then
  ```cpp
  const FVector2D Mouse = FSlateApplication::Get().GetCursorPos();
  if (Mouse != LastMouse) { LastMouse = Mouse; for (...) { if (RowButtons[Index]->IsHovered()) { Selected = Index; } } }
  ```
  `tools/Capture-DialogueUi.py` PLAN: park the cursor off the board before the first shot (the `cursor:x,y` step from `Capture-UiV5.py`, `cursor:960,1060`), so the frame shows the keyboard/default selection. Target: `dialogue-open` row 1 moss + brass edge, rows 2-4 hairline; `dialogue-more` row 1 selected.

### 3. Key caps are a different piece of art
- **What's wrong:** concept cap (5x crop): slate #17232A fill, 1.5 px brass #A98A55 outline with a faint second outer line, 4 px chamfered corners, white serif numeral ~26 px regular. Live `keycap_1..4` at 53 px: tan bevelled block, fill (109,97,72) vs concept (17,24,25), dE 28.6; 8 px bevelled gold frame; numeral bold with a drop shadow. Reads as a different kit.
- **Fix type:** SHARED (Kit-v5 art, Codex).
- **Instruction:** regenerate `GameClient/ArtSource/UI/Kit-v5/slim/source/keycap_1.png`..`keycap_4.png` at 2x (106x108, magenta key) from the concept crop x 460-508 y 408-456 (1672 px): "flat square key cap, slate #17232A fill, 1.5 px brass #A98A55 outline with 4 px chamfered corners and a 1 px faint outer brass line 2 px outside, no bevel, no gradient, no shadow; numeral N centred, EB Garamond Regular, ivory #E8E1CE, 52 px tall at 2x". `kit.json` unchanged. `process.py`, `Process-KitV5.py`, `Import-HudKit.py --save`. No code change (53 px placement already matches).

### 4. Objectives / Rewards headings are brass bold
- **What's wrong:** concept: ivory (254,249,202) regular, ~20 px. Live: brass (169,138,85) bold; dE 28.6 / 28.2. The section rules also differ: concept has a 6 px diamond at each end; live `section_rule` is a plain hairline with a 4 px dot at the left end only.
- **Fix type:** CODE (Engineer) for the headings; SHARED (Codex) for the rule art.
- **Instruction:** `SFDialogue.cpp` lines 322 and 338: `Text(WidgetTree, TEXT("Objectives"), 17, Ivory())` and `Text(WidgetTree, TEXT("Rewards"), 17, Ivory())` (drop `Brass(), true`). Art: regenerate `section_rule.png` 2x (480x16): 1 px brass rule with a 6 px (1080p) hollow diamond at both ends, `kit.json` `nine_slice_px` [24,0,24,0]; `divider.png` 2x (932x16): centre 10 px hollow diamond flanked by two 5 px diamonds 14 px either side, tapered ends, margins [120,0,120,0].

### 5. Button labels are 32 px type (shared helper)
- **What's wrong:** concept "Decline" ink 16 px tall / 70 wide, "Accept" 23 / 71 (24 px em, regular). Live 21 / 91 and 29 / 91: +5 and +6 px tall, +21 and +20 px wide. Trade "Cancel Trade" ink is also 21 tall and market "Hold to Buy" 31: the shared `Button` passes Slate size 24, which is 24 pt = 32 px at the 96 DPI Slate uses; §1 and family rule 5 say 24 px. Accept is also bold (`bPrimary`), concept regular. Cap height +-1 fails on every screen; consistent across screens, so not a cross-screen fail, but a §2 fail.
- **Fix type:** SHARED (`SFHudStyle.h`, UI v5 session).
- **Instruction:** `SFHudStyle.h` `Button(...)` line 391: `UTextBlock* Caption = Text(Tree, Label, 18, bPrimary ? Hex(TEXT("2A1E10")) : Ivory());` (18 pt = 24 px; no bold). Then `tools/Capture-UiAll.ps1` to recapture every screen that uses `Button`. If the project decides the whole kit keeps the 32 px label, menus.md §1 must say "24 pt (32 px)" and family rule 5 must change; until one of those lands this is a fail, not a flag.

### 6. Name and quest title are bold
- **What's wrong:** concept "Millwarden Oswin" and "The Mill's Supply" are EB Garamond Regular (2x crop: thin hairlines on M, w, l); live uses `Heading` = EB Garamond Bold. Width matches only because the size was tuned down to the bold face (25 pt vs the concept's ~26).
- **Fix type:** CODE (Engineer).
- **Instruction:** `SFDialogue.cpp` line 261 `NameText = Text(WidgetTree, TEXT(""), 25, Ivory()); Resize(NameText, 26.3f);` and line 317 `OfferTitle = Text(WidgetTree, TEXT(""), 23, Ivory()); Resize(OfferTitle, 24.2f);`. Verify ink: name 663-924 x 284-308, title 1044-1271 x 456-484 (+-2).

### 7. Accept art: inner plate and no dark border
- **What's wrong:** live `button_primary_brass` draws a second, lighter rectangle inset 6 px inside the fill (reads as a pressed/inset button) and has no darker border; concept is one flat brass plate (180,134,70) with a 2 px #7A5F33 border and 10 px chamfers. Fill dE 1.1 (fine); edge (231,188,113) vs (175,136,93). Fails the 1:1 art check; Decline (outline, chamfers, dE 2.3) passes.
- **Fix type:** SHARED (Kit-v5 art, Codex).
- **Instruction:** regenerate `button_primary_brass.png` 2x from the concept crop x 898-1072 y 668-712 (1672 px): "flat brass #A98A55 plate, 2 px #7A5F33 border, 10 px chamfered corners, subtle vertical burnish only, no inner rectangle, no inset line"; `kit.json` margins as now.

### 8. Speaker placement
- **What's wrong:** speaker head centre at x 21 %, y 45 % (concept x 21 %, y 29 %; r6 target y 24-34 %): the camera pitches down 14 deg and the speaker stands beside the lower half of the board. A second figure's shins and feet show under the board at x 730-920, y 841-900 (the player's own body behind the board; concept has the player at the left edge, x 0-25 %).
- **Fix type:** CODE (Engineer).
- **Instruction:** `GameClient/Config/DefaultGame.ini` `DialogueFramingPitchDegrees=-4.0` (clamp -30..10 in `SFE1Settings.cpp` allows it) and `DialogueFramingYawDegrees=42.0`; in `ASFE1Character::SetDialogueFramingTarget` while a speaker is set, `GetMesh()->SetOwnerNoSee(true)` (restore on clear) so no part of the player projects behind the board. Target: head centre y 24-34 %, nothing of the player visible below y 841 between x 486-1437.

### 9. Canonical-style flags (section 1 wins over the concept; not failed)
- Unselected rows: concept 1 px dim-brass (100,85,65) edge with 10 px chamfer notches and corner marks; live hairline #3A4348 (51,61,66), no chamfers, fill (20,32,38) vs (21,26,27) dE 4.1. §1 "hairline row (#17232A 60 %, #3A4348)". Market board rows use the same `RowBrush`: consistent.
- Text colour: every concept text sample is near white (254,249-255,202-255); live is canonical ivory #E8E1CE, dE 8-10 everywhere. §1 ivory.
- Selected row: moss fill + brass edge = `tab_selected`, same as the market board's selected row and the trade "Offer Locked" state: consistent.
- Card fill (13,24,32) vs concept (15,21,23), dE 4.2: `Slate(0.85f)` over the #10171A backing picks up blue. Non-canonical value; if the UI v5 session wants it closer, `KitPanel(..., Hex(TEXT("0F1517")), ...)` at line 308. Does not block.

### 10. HUD master items (outside the board; SHARED, do not block the board)
- Tracker title "The Mill's Supply" is bold ~30 px with a 6 px ornament under it; concept ~26 px regular, no ornament, frame y 333-460 vs live 340-500. Counts are bold Source Sans; concept serif.
- Minimap footer "Silverbrook · 389 m" has no plate behind it; concept 180x32 plate.
- Level badge numeral sits on the stag; concept in the lower lobe.

## Cross-screen consistency

Rows (selected = `tab_selected` moss + brass; unselected = hairline), Accept/Decline (`button_primary_brass` / `button_normal`, 56 tall, same 32 px label), frame (`window_frame` with the top-centre finial, same as trade) and the type roles match trade, market board, inventory and build mode. No control differs between the dialogue and those screens. Item 5 is a fault they all share.

## Fix list

1. SHARED (Codex): regenerate `keycap_1..4.png` 2x 106x108: slate fill, 1.5 px brass chamfered outline, faint outer line, ivory EB Garamond Regular numeral 52 px at 2x, no bevel/shadow. (§3)
2. SHARED (UI v5 session): `SFHudStyle.h` `Button` caption `Text(Tree, Label, 18, ...)`, no bold for primary; recapture all `Button` screens. (§5)
3. SHARED (Codex): `button_primary_brass.png` flat brass, 2 px #7A5F33 border, chamfers, no inner rectangle. (§7)
4. SHARED (Codex): `section_rule.png` diamond at both ends [24,0,24,0]; `divider.png` centre diamond + two flanking diamonds [120,0,120,0]. (§4)
5. SHARED (HUD master): tracker title 26 px regular, no ornament, serif counts; minimap footer plate 180x32; badge numeral in the lower lobe. (§10)
6. Engineer: left column flows in a `UVerticalBox` (speech, rule pad 32/27, rows 72 + 13) at (513, 332); clip the frame; cap speech at 3 lines and rewrite the "Tell me more" node. (§1)
7. Engineer: hover selects only when the cursor moved (`LastMouse`); capture parks the cursor at (960, 1060) before the shots. (§2)
8. Engineer: `Objectives` / `Rewards` = `Text(..., 17, Ivory())`, regular. (§4)
9. Engineer: `NameText` / `OfferTitle` = `Text` regular, Resize 26.3 / 24.2. (§6)
10. Engineer: `DialogueFramingPitchDegrees=-4`, `DialogueFramingYawDegrees=42`, player mesh `SetOwnerNoSee(true)` while a dialogue speaker is set. (§8)
11. Engineer (optional, non-blocking): card fill `Hex("0F1517")` at line 308. (§9)
