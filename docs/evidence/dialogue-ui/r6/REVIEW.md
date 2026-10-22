# Dialogue UI r6 review (Fable, independent)

Concept: `docs/Game-Design/concept-art/UI/v5/10-npc-dialogue.png` (1672x941, x1.148 to 1080p). Live: `r6/dialogue-open.png`, `dialogue-more.png`, `dialogue-accepted.png`, pair `dialogue-open-compare.png`. Spec: `UI/v5/REVIEW.md` 10-npc-dialogue note, `ui-hud.md` §3 family rules, `visual-review.md`. Backdrop is out of scope by instruction; the board, tracker and HUD treatment are judged. r3 was read for its ask list only; nothing was assumed fixed.

Measured (live, 1080p): board frame x 499-1420 (26.0-74.0 %), y 151-840 (14.0-77.8 %): rule 12 met. Rows: brass edge y 388-445 / 476-533 / 564-621 / 652-709 (57 tall, pitch 88), x 544-970 (426 wide). Vertical rule x 987-988. Card x 1007-1374 (367 wide); art well 290x172 at x 1046-1336 y 196-368. Accept x 1034-1170 (136 wide), Decline x 1190-1348 (158). Selected fill (47,61,45) = #2F3D2D, selected label (232,225,206) ivory. Board fill over empty sand (35,44,48); over the mannequin (26,29,27). HUD dim: tracker ivory 139/232 = 60 %, hotbar numerals 139, badge 147: 40 % dim applied. Tracker shows "The Mill's Supply / Gather Oak Logs 0 / 6 / Deliver to Oswin 0 / 1" in all three captures.

Resolved since r3 (not re-listed): selected row fill and label (r3 §1), card frame and art well (§3), tracker switch (§4), type sizes (§8), HUD dim.

## 10-npc-dialogue — FAIL

A player sees the right design now: two halves, portrait + name plate, numbered moss-selected rows, quest card with art well, Objectives/Rewards, Accept/Decline, tracker synced. It is not acceptable yet because a figure is visible through the left half of the board in every capture, the Accept button is two thirds of its concept width while 46 px of frame sits empty to the right of the card, the bottom 130 px of the left column is bare, and the row, key-cap and Decline art still read as a different ornament family. The board is not acceptable with the shared art as-is: three shared pieces (key-cap numerals, Decline art, row swashes) would each block a PASS on their own; the CODE items below block regardless of the art.

### 1. The speaker's body shows through the board
- **What's wrong:** a grey mannequin silhouette is readable through the left column in all three captures (x 600-950, y 350-850): board fill measures (35,44,48) over sand and (26,29,27) over the body, a 16-level swing. Concept board fill is (14,21,23) and opaque; nothing shows through. The frame tint is `Slate(1.f)` in code, but the `window_frame` texture's own fill is 84 % alpha (blueprint §2), so the tint cannot make it opaque.
- **Fix type:** CODE.
- **Instruction:** `SFDialogue.cpp`, `USFDialoguePanel::NativeOnInitialized`, before `UBorder* Frame = KitPanel(...)` (line 207): add an opaque slate backing under the frame at the same anchors, inset past the chamfer:
  ```cpp
  UBorder* Backing = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
  Backing->SetBrush(FSlateColorBrush(FLinearColor::White)); Backing->SetBrushColor(Hex(TEXT("111C23")));
  Backing->SetVisibility(ESlateVisibility::HitTestInvisible);
  UCanvasPanelSlot* BackingSlot = Root->AddChildToCanvas(Backing);
  BackingSlot->SetAnchors(FAnchors(0.26f, 0.14f, 0.74f, 0.78f)); BackingSlot->SetOffsets(FMargin(0.f)); BackingSlot->SetMargin(FMargin(8.f));
  ```
  (the canvas slot inset of 8 px keeps the backing inside the 10 px chamfer notches). Target: board fill measures within 6 levels of #111C23 over any backdrop; no silhouette through the rows.

### 2. Speaker is behind the board, not beside it
- **What's wrong:** the figure projects at x ~42 %, y 32-79 %, centred behind the left column, and the camera looks down on it. Concept: speaker face at x 21 %, y 29 %, to the left of the board, eye level. The yaw offset added in `SFE1Character.cpp:374` (`DialogueYaw = 38.f`) has no visible effect in these captures.
- **Fix type:** CODE.
- **Instruction:** `SFE1Character.cpp` `TickCameraControls`: the offset only applies while the steer-hold condition is true and the assist is bounded by `CombatFramingDeadzoneDegrees` / `CombatFramingYawSpeed`, and `DesiredPitch` is clamped to the combat bias. On dialogue open, snap instead of assisting: in the branch that sets `DialogueFramingTarget`, set `CameraRotation.Yaw = ToTarget.Yaw + 38.f; CameraRotation.Pitch = -6.f;` once, then let the assist hold it. Recapture after the blend settles (first timed shot >= 2 s after open) and measure: speaker head centre within x 16-24 %, y 24-34 %.

### 3. Accept button is 136 px wide; card leaves 46 px of frame empty on the right
- **What's wrong:** Accept measures 136 wide (concept 200), Decline 158 (concept 163). The card is 367 wide (concept 413) and ends 46 px short of the frame (concept 11 px); the left column starts 45 px in (concept 29). The summary wraps to two lines (concept one) for the same reason.
- **Fix type:** CODE.
- **Instruction:** `SFDialogue.cpp` `NativeOnInitialized`: line 207 `KitPanel(WidgetTree, TEXT("window_frame"), 1.f, Slate(1.f), Brass(), FMargin(4.f, 6.f), 6.f)` (was 10,6); line 270 `LeftSlot->SetPadding(FMargin(4.f, 36.f, 8.f, 0.f))` (was 14,36,14,0); line 334 `RightBox->SetWidthOverride(412.f)`; line 335 `->SetPadding(FMargin(10.f, 6.f, 4.f, 0.f))` (was 16,6,14,0). Result: card ~412 wide ending ~12 px from the frame, Accept = 412-28-18-160 = 206 px, summary "Bring oak logs to the mill before the rain." at 18 px (382 px) fits the 384 px card interior; if it still wraps, `Card` padding `FMargin(12.f, 12.f)`.

### 4. Left column void under the rows
- **What's wrong:** row 4 ends at y 709; the frame ends at 838: 129 px of empty slate (concept 54 px). The spec note sends the extra window height to the art well and row spacing; the rows are already at 64 tall / 24 gap and the void remains.
- **Fix type:** CODE.
- **Instruction:** `SFDialogue.cpp` line 233 divider slot padding `FMargin(0.f, 0.f, 0.f, 36.f)` (was 28); line 266 row slot padding `FMargin(0.f, 0.f, 0.f, 34.f)` (was 24). Rows then end at ~747, void ~91 px; with three rows (`dialogue-more.png`) the void grows, which the concept accepts.

### 5. Response-row art is still the ornate family
- **What's wrong:** `quest_row` draws corner swashes, corner gems and an inner hairline on every row; the 40 px key cap overlaps the left swash. Concept rows: plain 2 px brass rectangle, 10 px chamfer notches, single rule (rule 4; blueprint §5 "Kit piece with corner swashes the concept does not have"). Unchanged from r3.
- **Fix type:** SHARED (kit art, Codex). Blocks a PASS on its own.
- **Instruction:** regenerate `GameClient/ArtSource/UI/Kit-v5/slim/source/quest_row.png` at 2x (720x112), magenta key, from the concept crop x 450-860 y 477-537 (1672 px): "plain 2 px brass #A98A55 rectangle with 10 px chamfered corners, slate #17232A fill at 85 %, no corner ornaments, no gems, no inner rule"; `kit.json` `nine_slice_px` [24,24,24,24]. `process.py`, `Process-KitV5.py`, `Import-HudKit.py --save`.

### 6. Key-cap numerals ~14 px, below the 16 px minimum
- **What's wrong:** `keycap_1..4` at 40 px show a ~14 px numeral (ivory run at x 578 is a single pixel wide stroke); concept numeral ~24 px filling 60 % of a 46 px cap; spec note says 36 px caps. Unchanged from r3.
- **Fix type:** SHARED (kit art, Codex). Blocks a PASS on its own (type minimum rule).
- **Instruction:** regenerate `keycap_1.png`..`keycap_4.png` at 2x (80x72) with the numeral 44 px tall (EB Garamond Bold, ivory #E8E1CE) on slate, 2 px brass border, no inner bevel; same `kit.json` rows. CODE after the art lands: `SFDialogue.cpp` line 262 `KitImage(WidgetTree, Keycaps[Index], 36.f)` and `Pad` left padding 70 (line 243).

### 7. Decline reads as a pressed navy button; Accept is a flat tan slab
- **What's wrong:** Decline fill (17,44,62) navy with a double brass frame and four corner gems: it reads as the hovered/pressed state and as the heavier of the two buttons. Concept Decline: thin brass outline on slate, no gems. Accept: live is a flat tan rectangle with a soft gradient and no chamfered darker border; concept Accept (153,109,47) brass with a 2 px #7A5F33 border and 10 px chamfers. Unchanged from r3.
- **Fix type:** SHARED (`button_normal`, `button_primary_brass`, kit session). Decline blocks a PASS on its own (state reads wrong); Accept art alone would not.
- **Instruction:** `button_normal.png` 2x: slate #17232A 85 % fill, 2 px brass outline, 10 px chamfers, no gems, no inner frame; `button_primary_brass.png` 2x: brass #A98A55 fill, 2 px #7A5F33 border, 10 px chamfers; margins 40,40,40,40 / 56,48,56,48 as in `kit.json`. Concept crops: Accept x 898-1072 y 668-712, Decline x 1088-1230 y 668-712.

### 8. Header divider and section rules are plain lines
- **What's wrong:** the rule under the speech is a flat 1 px brass line at y 354 (concept: centre diamond with two small diamonds and tapered ends); Objectives/Rewards rules have no diamond terminals (concept: small diamond each end). `section_rule` does not exist in the kit, so `SectionRule` falls back to `divider`.
- **Fix type:** SHARED (kit art, Codex). Does not block on its own.
- **Instruction:** regenerate `divider.png` 2x (920x16) with a centre diamond flanked by two 6 px diamonds and tapered ends, `nine_slice_px` [120,0,120,0]; add `section_rule.png` 2x (480x16) with a 6 px diamond at each end, margins [24,0,24,0]. No code change: `SectionRule` already picks `section_rule` when it exists.

### 9. XP roundel glyph
- **What's wrong:** `icon_xp_roundel` is a compass rose at 44 px; concept roundel reads "XP" in brass on a brass ring. Unchanged from r3.
- **Fix type:** SHARED (kit art, Codex). Does not block on its own.
- **Instruction:** regenerate `icon_xp_roundel.png` 128x128: 4 px brass ring, "XP" EB Garamond Bold brass, slate centre. Concept crop x 905-950 y 608-650.

### 10. Double edge between the halves
- **What's wrong:** a 2 px vertical rule at x 987-988 sits 19 px left of the card's own edge at x 1007; concept has no vertical rule, the card edge is the divide.
- **Fix type:** CODE.
- **Instruction:** `SFDialogue.cpp` lines 272-275: delete the `Split` block (the `USizeBox`, its `KitPanel` and `Columns->AddChildToHorizontalBox(Split)`).

### 11. Speech wraps "fresh / timber" instead of "fresh timber / before the rains."
- **What's wrong:** speech column is 289 px (x 686-975); "The mill needs fresh timber" at 22 px needs ~300. Line pitch 35 (concept 32).
- **Fix type:** CODE.
- **Instruction:** `SFDialogue.cpp` line 231 `SpeechSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f))` (was 22); line 226 `Text(WidgetTree, TEXT(""), 21, Ivory())`. Fix 3 also widens the left column slightly.

### 12. Master-HUD faults visible in every capture (outside the board)
- Minimap footer text "Silverbrook 00B7 461 m" spans x 1665-1880 over a 155 px plate (x 1705-1860): text overflows the plate on both sides; concept "Silverbrook · 227 m" centred in a 180x32 plate. SHARED (HUD master). Broken element (overflow): blocks the screen, not the board.
- Level badge numeral "1" sits on the stag glyph; concept "18" sits in the lower lobe below it. SHARED (HUD master). Overlap: blocks the screen, not the board.
- Tracker frame is a grey hairline with a title underline; concept tracker is a 1 px brass chamfered frame with no title rule, title ~24 px (live ~28). SHARED (HUD master). Does not block on its own.
- Minimap ring has one top diamond; concept has four cardinal diamonds. SHARED. Does not block.
- Capture: mirrored world text "E: craft, C: recipe | 4 Hide Strips | 1 Too..." at x 250 y 200 and the flat sand plot show this is the construction sandbox again, and the camera is top-down. `ui-hud.md` §4 forbids the sandbox. Engineer: recapture on the river trail facing the mill, E1 controller, `Shot showui`, after the framing blend settles. Not scored against the board by instruction, but §2 cannot be verified until this is done.

## Shared-art verdict

With every CODE item fixed and the shared art left exactly as-is, the board would still FAIL on three shared items: §6 key-cap numerals (14 px, below the 16 px floor), §7 Decline art (navy fill + gems reads as a pressed state), §5 row swashes (different ornament family on the four largest elements of the left half). §8, §9, §12 tracker/minimap-ring would not block the board alone. §12 footer overflow and badge overlap block the screen-level PASS but are HUD-master items, not the board.

## Fix list

1. SHARED (Codex): regenerate `quest_row.png` as a plain 2 px brass rectangle with 10 px chamfers, no swashes, gems or inner rule; margins [24,24,24,24]. (§5)
2. SHARED (Codex): regenerate `keycap_1..4.png` 2x 80x72 with 44 px EB Garamond Bold ivory numerals, 2 px brass border. (§6)
3. SHARED (kit session): `button_normal` = slate 85 % + 2 px brass outline, 10 px chamfers, no gems; `button_primary_brass` = brass fill + 2 px #7A5F33 border + chamfers. (§7)
4. SHARED (Codex): `divider.png` centre diamond, margins [120,0,120,0]; new `section_rule.png` end diamonds, margins [24,0,24,0]; `icon_xp_roundel.png` "XP" ring. (§8, §9)
5. SHARED (HUD master): minimap footer plate 180x32 with "Place · 000 m" text fitted; level badge numeral in the lower lobe; tracker frame brass hairline, no title rule, title 24; minimap ring four diamonds. (§12)
6. Engineer: opaque #111C23 `Backing` border under the frame at the rule-12 anchors, 8 px inset. (§1)
7. Engineer: snap camera yaw to `ToTarget.Yaw + 38` and pitch -6 on dialogue open; verify speaker head at x 16-24 %, y 24-34 % in the capture. (§2)
8. Engineer: frame padding (4,6); left slot padding (4,36,8,0); `RightBox` 412 wide, padding (10,6,4,0). Accept ends ~206 wide, summary one line. (§3)
9. Engineer: divider bottom pad 36, row bottom pad 34. (§4)
10. Engineer: delete the vertical `Split` rule. (§10)
11. Engineer: speech slot left pad 16, speech text 21. (§11)
12. Engineer (after fix 2 lands): key cap 36 px, row label left pad 70. (§6)
13. Engineer: recapture on the river-trail scene per `ui-hud.md` §4 (E1 controller, `Shot showui`, no sandbox text, after the framing blend); re-pair all three captures. (§12)
