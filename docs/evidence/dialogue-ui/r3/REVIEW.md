# Dialogue UI r3 review (Fable, independent)

Concept: `docs/Game-Design/concept-art/UI/v5/10-npc-dialogue.png` (1672x941, x1.148 to 1080p). Live: `r3/dialogue-open.png`, `dialogue-more.png`, `dialogue-accepted.png`, pair `dialogue-open-compare.png`. Spec: `UI/v5/REVIEW.md` (10-npc-dialogue note, family rules 1-12), `docs/blueprints/ui-hud.md`. Backdrop is out of scope by instruction; board, type and HUD treatment are judged.

Measured (live, 1080p): board frame x 500-1420 (26.0-74.0 %), y 152-840 (14.1-77.8 %): rule 12 met. Rows 56 tall at y 370/454/538/622, pitch 84. Right column 340 wide at x 1038-1378. Buttons 56 tall, Accept x 1038-1200, Decline x 1216-1375. HUD dimmed 40 %.

## 10-npc-dialogue — FAIL

A player sees the same two-column board, numbered rows and Accept/Decline, but the selected row is unreadable, the quest half has lost its card and art frames, the speaker's body shows through the board, the tracker is the wrong quest and the row/button/divider art is a different ornament family. Not recognisable as the same design yet.

### 1. Selected response row: label washed out to grey
- **What's wrong:** the moss wash sits above the label. "Tell me more" measures (162,173,149) instead of ivory #E8E1CE (255,255,254 in the concept); the keycap "1" is dimmed the same way. Fill is (68,85,71) vs the concept's (47,61,45) #2F3D2D.
- **Fix type:** CODE.
- **Instruction:** `SFDialogue.cpp`, `USFDialoguePanel::NativeOnInitialized`, row loop. Put the wash under the label, not over the whole row: build `UOverlay* Face = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());` add `Wash` first (`HAlign_Fill`/`VAlign_Fill`, padding `FMargin(4.f)`, `HitTestInvisible`), then `Pad`; `Control->SetContent(Face);` and drop the `Stack->AddChildToOverlay(Wash)` line. In `StyleRows` set the wash to the concept fill: `FLinearColor(0.184f, 0.239f, 0.176f, 1.f)` (#2F3D2D) when selected, transparent otherwise. Keycap stays on `Stack` above everything.

### 2. Speaker's mannequin shows through the board; speaker is behind the board, not beside it
- **What's wrong:** the NPC projects at x 45 % y 40-70 %, directly behind the left column; its grey body is visible through the rows (board fill (23,32,36) at ~0.79 effective opacity). Concept: speaker face at x 21 % y 29 %, left of the board, and the board is opaque (18,24,25) with no backdrop through it.
- **Fix type:** CODE.
- **Instruction:** `SFE1Character.cpp` framing block (line ~335): when `FrameTarget == DialogueFramingTarget`, add a dialogue yaw offset so the speaker lands at x 20 %: `const float DialogueYawOffset = FrameTarget == DialogueFramingTarget.Get() ? 30.f : 0.f;` (hFOV 90 at 16:9: atan((0.5-0.2)*2*tan 45) = 31 deg) added to the framing yaw, shoulder side right (`Side = 1.f`) while dialogue is open. And `SFDialogue.cpp` line 201: `KitPanel(WidgetTree, TEXT("window_frame"), 1.f, Slate(1.f), ...)` (alpha 0.94 -> 1.0).

### 3. Quest half has no card frame and the art has no well
- **What's wrong:** the mill image is a bare 340x206 rectangle on slate; the right half is plain stacked text. Concept: the whole right half is an inset card (2 px brass frame, 24 px corner overlays) and the art sits in a brass-edged image well (280x170 with corner ornaments), the spec note's "inset quest card frame" and "image 280x170 as the respawn cards".
- **Fix type:** CODE.
- **Instruction:** `SFDialogue.cpp`, right half. Replace `RightBox->SetContent(Right)` with `UBorder* Card = KitPanel(WidgetTree, TEXT("detail_card_frame"), 1.f, Slate(0.85f), Brass(), FMargin(14.f, 12.f), 0.f); Card->SetContent(Right); RightBox->SetContent(Card);` (the piece `SFExchangeWidgets.cpp:492` already uses). Replace the bare `ArtImage(..., 340.f, 206.f)` with `UBorder* Well = KitPanel(WidgetTree, TEXT("respawn_image_well"), 1.f, FLinearColor::Transparent, Brass(), FMargin(0.f), 0.f, false);` sized 300x184 (`USizeBox` 300x184) with `OfferArt` inside at 280x170 (`SetArt(..., FVector2D(280.f, 170.f))` in `Open`). RightBox width 340 stays.

### 4. Quest tracker does not switch to the offered quest
- **What's wrong:** tracker reads "Frontier Founder / Gather Oak Log 0 / 1" (1 row) in all three captures. Concept and spec note: "The Mill's Supply / Gather Oak Logs 0 / 6 / Deliver to Oswin 0 / 1" while the board is open.
- **Fix type:** CODE.
- **Instruction:** `SFQuestPilot.h/.cpp`: add `void USFQuestPilotSubsystem::PresentOffer(const FSFQuestView& View)` (calls `Hud->Present(View)`) and `void RestoreTracker()` (re-presents the pilot's own view). `SFPlayerControllerDialogue.cpp` `TryOpenDialogue` after `DialoguePanel->Open(Tree)`: when `Tree->bHasOffer`, build `FSFQuestView View; View.Title = Tree->Offer.Title; for each objective { FSFQuestLine L; L.Label = Obj.Label; L.Count = 0; L.Target = Obj.Need; View.Lines.Add(L); }` and call `GetWorld()->GetSubsystem<USFQuestPilotSubsystem>()->PresentOffer(View)`; `CloseDialogue` calls `RestoreTracker()`.

### 5. Response-row art is the wrong ornament family
- **What's wrong:** `quest_row` draws corner swashes, a double inner rule and gem corners; the keycap sits on top of the left swash. Concept rows: plain 2 px brass rectangle, 10 px chamfer notches, single rule (rule 4; blueprint §5 "Kit piece with corner swashes the concept does not have").
- **Fix type:** SHARED (kit art, Codex).
- **Instruction:** regenerate `GameClient/ArtSource/UI/Kit-v5/slim/source/quest_row.png` at 2x (720x112), magenta key, from the concept crop x 450-860 y 477-537 (1672 px): "plain 2 px brass #A98A55 rectangle with 10 px chamfered corners, slate #17232A fill at 85 %, no corner ornaments, no inner rule"; `kit.json` `nine_slice_px` [24,24,24,24]. Re-run `process.py`, `Process-KitV5.py`, `Import-HudKit.py --save`.

### 6. Key-cap numerals below the 16 px minimum
- **What's wrong:** `keycap_1..4` at 40x36 show a ~13 px numeral; the concept cap shows a 24 px ivory serif numeral filling 60 % of the cap (cap 46 px in the image; 36 px per the spec note).
- **Fix type:** SHARED (kit art, Codex).
- **Instruction:** regenerate `keycap_1.png`..`keycap_4.png` at 2x (80x72) with the numeral at 44 px (60 % of cap height), EB Garamond Bold, ivory on slate, 2 px brass border; everything else unchanged. Same `kit.json` rows.

### 7. Accept / Decline art
- **What's wrong:** Decline draws a navy fill (#1A3550-ish) with a heavy double brass frame and corner gems (looks pressed/hovered); Accept is a flat tan rectangle without the chamfered brass border. Concept: Accept = brass fill with a 2 px darker brass border and chamfer notches; Decline = thin brass outline on slate. Widths: concept Accept 200 / Decline 165 (18 gap); live 162 / 159.
- **Fix type:** SHARED for the art (`button_normal`, `button_primary_brass`, owned by the kit session); CODE for widths.
- **Instruction:** CODE `SFDialogue.cpp` lines 305-312: `Button(WidgetTree, TEXT("Accept"), EButtonKind::Primary, 200.f)`, `Button(WidgetTree, TEXT("Decline"), EButtonKind::Secondary, 160.f)`, Accept slot `Fill`, Decline slot `Auto`, gap 18 (`FMargin(0.f, 0.f, 18.f, 0.f)`). SHARED: `button_normal` = slate fill 85 % + 2 px brass outline, no gems; `button_primary_brass` = brass fill + 2 px #7A5F33 border + 10 px chamfers.

### 8. Type sizes and leading
- **What's wrong:** speaker name 32 bold (concept ~28); speech line 24 with 43 px line pitch (concept ~21 px, 32 px pitch, wraps "The mill needs fresh timber / before the rains."); quest title 30 bold (concept ~26); summary 20 wraps to two lines (concept 18, one line). "120 XP" / "20 Coin" render in Source Sans; concept uses the text serif.
- **Fix type:** CODE.
- **Instruction:** `SFDialogue.cpp`: `NameText = Heading(WidgetTree, TEXT(""), 28, Ivory())`; `LineText = Text(WidgetTree, TEXT(""), 22, Ivory()); LineText->SetLineHeightPercentage(0.9f);`; `OfferTitle = Heading(WidgetTree, TEXT(""), 26, Ivory())`; `OfferSummary = Text(WidgetTree, TEXT(""), 18, Ivory())`; `XpText = Text(WidgetTree, TEXT(""), 20, Ivory())`, `CoinText = Text(WidgetTree, TEXT(""), 20, Ivory())` (serif, not `Num`).

### 9. Left column void under the rows
- **What's wrong:** 162 px of empty slate under row 4 (y 678-840); concept leaves 56 px. The rule-12 frame is 71 px taller than the image's board and the spec says that height goes to the art well and row spacing; it currently goes to the bottom gap.
- **Fix type:** CODE.
- **Instruction:** `SFDialogue.cpp` row loop: `RowBox->SetHeightOverride(64.f)`, row slot padding `FMargin(0.f, 0.f, 0.f, 24.f)`; `LeftSlot->SetPadding(FMargin(14.f, 36.f, 14.f, 0.f))`; divider slot padding `FMargin(0.f, 0.f, 0.f, 28.f)`. Bottom gap becomes ~105 px.

### 10. Dividers are plain lines
- **What's wrong:** the header divider is a flat 2 px line; the concept's has a centre diamond and tapered ends, and the Objectives/Rewards rules start with a small diamond. `divider` (420x8, margins 0) is stretched to 460 px as a box, which smears any ornament.
- **Fix type:** SHARED (kit art) + CODE.
- **Instruction:** SHARED: regenerate `divider.png` with a centre diamond and set `nine_slice_px` [60,0,60,0] so the ends and centre keep their shape; `section_rule.png` with a left diamond, margins [24,0,8,0]. CODE `SFDialogue.cpp` `SectionHeading`: use `KitPanel(Tree, TEXT("section_rule"), ...)` instead of `divider`.

### 11. XP roundel glyph
- **What's wrong:** `icon_xp_roundel` is a compass rose; the concept roundel reads "XP" in brass on a ring.
- **Fix type:** SHARED (kit art, Codex). Regenerate `icon_xp_roundel.png` 128x128: brass ring 4 px, "XP" EB Garamond Bold brass, slate centre.

### 12. Capture and master-HUD faults (outside the board, visible in every capture)
- Mirrored station debug strings ("E: craft, C: recipe | 4 Hide Strips ...", "Forge") float at y 130-150 above the board: this is the construction sandbox plot, not the E1 river-trail scene the protocol (`ui-hud.md` §4) requires. Engineer: recapture on Silverbrook's river trail facing the mill, E1 controller, `Shot showui`.
- Minimap footer text "Silverbrook 00B7 461 m" overflows its 180 px plate (text ~230 px) and shows a grid cell; concept "Silverbrook · 227 m". SHARED (HUD master).
- Level badge "1" overlaps the stag glyph; concept "18" sits in the lower lobe. SHARED (HUD master).

## Fix list

1. SHARED (Codex): regenerate `quest_row.png` as a plain 2 px brass rectangle with 10 px chamfers, no swashes; margins [24,24,24,24]. (§5)
2. SHARED (Codex): regenerate `keycap_1..4.png` with 44 px numerals at 2x. (§6)
3. SHARED (kit session): `button_normal` thin brass outline on slate, no gems; `button_primary_brass` brass fill with chamfered darker border. (§7)
4. SHARED (Codex): `divider.png` centre diamond, margins [60,0,60,0]; `section_rule.png` left diamond, margins [24,0,8,0]; `icon_xp_roundel.png` "XP" ring. (§10, §11)
5. SHARED (HUD master): minimap footer plate width / text; level badge numeral position. (§12)
6. Engineer: wash under the label; selected fill #2F3D2D opaque; label stays ivory. (§1)
7. Engineer: dialogue framing yaw +30 deg so the speaker sits at x 20 %; board alpha 1.0. (§2)
8. Engineer: `detail_card_frame` around the right half; `respawn_image_well` 300x184 around the art at 280x170. (§3)
9. Engineer: tracker shows the offered quest while open (`PresentOffer` / `RestoreTracker`). (§4)
10. Engineer: Accept 200 Fill / Decline 160 Auto, 18 px gap. (§7)
11. Engineer: name 28, line 22 at 0.9 line height, title 26, summary 18, reward text serif 20. (§8)
12. Engineer: rows 64 tall, 24 gap, left top pad 36, divider bottom pad 28. (§9)
13. Engineer: `SectionHeading` uses `section_rule`. (§10)
14. Engineer: recapture on the river-trail scene per `ui-hud.md` §4; no sandbox debug labels in frame. (§12)
