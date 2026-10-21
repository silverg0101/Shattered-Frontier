# UI v5 final — independent acceptance review

Reviewer: Fable (art director / acceptance judge), 2026-10-21. Inputs: `docs/Game-Design/concept-art/UI/v5/*.png`, `REVIEW.md`, `IMPLEMENTATION-FIX.md`; live pairs in this folder; `GameClient/ArtSource/UI/Kit-v5/kit.json` (+ PNGs); widget code under `GameClient/Source/ShatteredFrontier`. All px are 1080p design px (compare images are 0.5x: multiply by 2). Verdict rule: PASS only when a player would take the live frame for the concept; anything broken is a FAIL regardless of how much else matches.

Result: **0 PASS / 9 FAIL.** Two screens are judged on captures the README itself labels as earlier frames (`inventory-compare.png`, `loading-screen-compare.png`); they are marked **stale capture** and must be recaptured after the fixes.

Cross-screen facts established from the code (so the per-screen lists do not repeat them):

- The kit is now imported at 1x and `SFHudStyle::KitBrush` draws boxes at design size (G1 done). Frame fills are #111C23 at alpha 214/255 = 84% (rule 3 says 85%): the "see-through" look in the captures is the art as specified plus a bright, daylit backdrop. Not a bug; no fix.
- `SFHudStyle::KitPanel` adds `Element->InsetPixels` (= `nine_slice_px.left / 2`) to every caller's padding. On small pieces (`slot_catalogue_*` margin 24 → 12 px inset; `tab_*` margin 22 → 11 px) this eats the content area: a 100 px tile has 76 px for a 70 px icon plus a 16 px caption, an 80 px rail tile has 58 px for a 36 px glyph plus a label. This is the root of every "label spills past its tile" defect in build-mode. One helper fix (item 1 below).
- No shared hold-to-confirm button exists. `SFExchangeWidgets.cpp:91 WithHoldStrip` draws a 5 px ivory strip; `SFE1VitalsHud.cpp:202-219` builds its own ring + disc; neither matches rule 8 / G3 (36 px ring at x 20 inside the left end, moss arc). Three screens fail on it (market, trade, defeated). One helper fix (item 2).
- `ASFPlayerController::ToggleMarketDemo` / `ToggleTradeDemo` (SFPlayerController.cpp:390-429) leave `InventoryPanel` visible under the modal. Two screens fail on it (item 3).

---

## gameplay-hud — FAIL

1. **What's wrong:** No target frame, no prompt bar, no toasts in the frame; the pawn is a mannequin on the Silverbrook blockout, not the E1 character on the river trail. The concept's three right-column toasts (280x66) and the top-centre target frame (410x70) are absent, so the master HUD cannot be judged complete.
   **Fix type:** CAPTURE (protocol in IMPLEMENTATION-FIX §Capture protocol, steps 1-3). Not a code defect; the widgets exist (`USFHudLayout::NativeOnInitialized` lines 97-130, `USFGatheringHud` toasts lines 73-100). Recapture with a boar aimed and a harvest just completed.

2. **What's wrong:** Level badge has no numeral. Concept: "18" in the lower plate of the 116 px hex. Live: the plate is empty.
   **Fix type:** CODE. `Private/Characters/SFE1VitalsHud.cpp:79-81`:
   ```cpp
   UTextBlock* LevelText = Num(WidgetTree, TEXT("1"), 22, Ivory());
   UOverlaySlot* LevelSlot = BadgeStack->AddChildToOverlay(LevelText);
   LevelSlot->SetHorizontalAlignment(HAlign_Center); LevelSlot->SetVerticalAlignment(VAlign_Bottom); LevelSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
   ```
   The numeral sits 10 px above the hex's bottom vertex, where the art is a 1 px brass line on transparent: it is drawn over the ground. The plate in `hex_level_badge.png` is the lower third of the hex (centre of the plate at y ≈ 0.80 of the 116 px). Replace the padding with `FMargin(0.f, 0.f, 0.f, 16.f)`, make the numeral `Num(WidgetTree, TEXT("1"), 22, Ivory(), true)` with `LevelText->SetShadowOffset(FVector2D(1.f, 1.f)); LevelText->SetShadowColorAndOpacity(Hex(TEXT("0E171C")));`, and add a 1 px ivory outline via `FSlateFontInfo Font = LevelText->GetFont(); Font.OutlineSettings.OutlineSize = 1; Font.OutlineSettings.OutlineColor = Hex(TEXT("0E171C")); LevelText->SetFont(Font);`. Verify on recapture that the numeral sits inside the dark plate; if the art has no plate, see ART note under item 10.

3. **What's wrong:** Stamina bar has no numeral. Concept: "150 / 150" centred in the amber bar. Live: blank.
   **Fix type:** CODE. `SFE1VitalsHud.cpp:108` `AddBar(TEXT("bar_stamina"), Amber(), StaminaBar, StaminaPlain);` discards the returned `UTextBlock*`. Keep it: `StaminaText = AddBar(...)` (add `TObjectPtr<UTextBlock> StaminaText;` to `USFE1VitalsHud`) and in `Present` set `StaminaText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Max, Max)))` with `Max = GetDefault<USFCombatPilotSettings>()->PlayerStamina` (add the typed setting, default 150, if it does not exist) until a stamina stat is bound.

4. **What's wrong:** Minimap footer reads "Silverbrook" only. Concept: "Silverbrook · 227 m" (zone · distance to the tracked objective), one line, 18 px.
   **Fix type:** CODE. `Private/Framework/SFHudLayout.cpp:165` `ZoneText = Text(WidgetTree, ZoneFromMapName(...), 18, Ivory());` In `NativeTick`, when `USFQuestPilotSubsystem` exposes a tracked objective location (add `TOptional<FVector> TrackedObjectiveLocation() const` to the subsystem, returning the first incomplete `FSFQuestLine` target actor's location), set `ZoneText` to `FString::Printf(TEXT("%s · %d m"), *Zone, FMath::RoundToInt(FVector::Dist2D(PawnLocation, Target) / 100.f))`; otherwise the zone alone.

5. **What's wrong:** Quest tracker frame carries rounded brass corner swashes (≈ 22 px radius at each top corner) and a double hairline; the concept tracker is a plain 2 px brass rectangle with 10 px chamfer notches and a 2 px engraved rule under the title. Visible at 1613-1893 x 340-456.
   **Fix type:** ART. `GameClient/ArtSource/UI/Kit-v5/quest_tracker_frame.png`, 560x308 (2x), `nine_slice_px [24,24,24,24]`, `intended_1080p_px [280,154]`. Keep the size and margins. Prompt (gpt-image, reference crop `docs/Game-Design/concept-art/UI/v5/gameplay-hud.png` box x 1405-1655, y 365-500 of the 1672x941 image): "Flat 2D game UI frame on a solid magenta #FF00FF background for keying, 560x308 px. A rectangular slate-navy panel fill #17232A at 85% opacity, a single 2 px brass #A98A55 outline with 10 px chamfered (cut) corners, a 2 px darker engraved inner hairline 4 px inside the outline. No scrollwork, no swashes, no rounded corners, no texture, no text. Edges crisp, orthographic, no shadow." Rebuild with `py -3 GameClient/ArtSource/UI/Kit-v5/build_structures.py` conventions (premultiplied resize, 1 px alpha erosion), rerun `scan_borders.py`, `tools/Process-KitV5.py`, `tools/Import-HudKit.py --save`.

6. **What's wrong:** Hotbar icons are the v4 `T_Icon_build_*` thumbnails (wall, foundation, roof, door) in slots 1-4. The master HUD concept shows axe, pickaxe, sword, bandage, potion, campfire. Rule 1 names `gameplay-hud.png` as the master. Acceptable only while the E1 inventory has no such items; record it as a product gap, not a UI defect. No fix in this round.

Everything else measured on this frame matches: badge 116 at x 38 / bottom 1037; bars 380x32, 10 px apart, numeral 20 centred; hotbar 6 x 88, 12 px gap, 666-1254, bottom 1037, numerals top-left 18, count bottom-right; minimap 250 at x 1901 / y 22 with the ring and the 18 px arrow; tracker 280 wide at x 1613 / y 340, title 24 bold, 18 px checkbox rows.

## build-mode — FAIL

1. **What's wrong:** Captured in the construction sandbox (grey checker plot, mannequin, "CONSTRUCTION DEVELOPMENT SANDBOX | UNLIMITED TEST STOCK" world text, no ghost wall, minimap footer "ConstructionSandbox"). The protocol (IMPLEMENTATION-FIX step 4) says: build mode from the E1 session on Silverbrook, or report blocked; never the sandbox.
   **Fix type:** CAPTURE. If `ASFPlayerController::ToggleBuildMode` cannot enter build mode on `Greenlands_Silverbrook`, the README must say so and the screen stays open.

2. **What's wrong:** Tile captions spill out of the 100 px tiles and over the thumbnail: "Foundation" runs from x 150 to x 250 across the tile edge and its baseline overlaps the icon's bottom 8 px; "Wall"/"Door"/"Roof" sit on the tile's bottom edge. Concept: 64 px thumbnail centred in the top 70 px, 16 px caption centred in the bottom 24 px, both inside the frame.
   **Fix type:** CODE (shared helper, see Fix list 1). Cause: `KitPanel` adds 12 px inset on each side of `slot_catalogue_normal` (margin 24/2), leaving 76 px for a 70 px icon + 20 px label. In `Public/Framework/SFHudStyle.h:172` add a parameter `bool bInset = true` to `KitPanel(...)` and compute `const float InsetX = bInset ? Element->InsetPixels : 0.f;` / `const float InsetY = bInset ? FMath::Min(...) : 0.f;`. In `Private/Construction/SFConstructionSandboxPalette.cpp:154` call `KitPanel(WidgetTree, TEXT("slot_catalogue_normal"), 1.f, Well, Hex(TEXT("3A4348")), FMargin(4.f, 6.f, 4.f, 4.f), 3.f, /*bInset*/ false)`, and line 156 `KitImage(WidgetTree, TileIcons[Tile], 70.f)` → `56.f`. Line 173 (empty cells) likewise `bInset=false`.

3. **What's wrong:** Category-rail labels overflow their 80 px tiles: "Structures" and "Favourites" extend ≈ 8 px past both edges. Concept: 36 px glyph + 16 px caption inside an 80x80 tile.
   **Fix type:** CODE. Same cause (`tab_*` inset 11 px). `SFConstructionSandboxPalette.cpp:129` `KitPanel(WidgetTree, Category == 0 ? TEXT("tab_selected") : TEXT("tab_normal"), 1.f, Well, Hex(TEXT("3A4348")), FMargin(0.f), 3.f)` → add `, /*bInset*/ false`; line 131 glyph `36.f` → `32.f`; line 132 label stays 16 but add `Label->SetOverflowPolicy(ETextOverflowPolicy::Ellipsis);` as the guard.

4. **What's wrong:** Panel is 946 px tall (y 43-989) against the concept's 32-886: the "Required Materials" box carries a two-line dev string "Target -1, 9, 0 | Click or B to request placement." Concept: one 18 px line "Valid placement" with a tick.
   **Fix type:** CODE. `SFConstructionSandboxPalette.cpp:263-270 SetStatusText` receives the controller's debug message. In the controller's call site pass only the validity word: `Palette->SetStatusText(FText::FromString(bValid ? TEXT("Valid placement") : TEXT("Invalid placement")))`, keep the target coordinates in `UE_LOG`. Also `line 70` `PanelSlot->SetAnchors(FAnchors(0.02f, 0.04f))` → `FAnchors(0.02f, 0.03f)`, and line 207 `StatusTextBlock->SetAutoWrapText(true)` → remove (one line). Swap the cross glyph when invalid: line 204 keep the `status_tick` image in a member and `SetBrush(KitBrush(*Kit(bValid ? TEXT("status_tick") : TEXT("status_cross"))))`.

5. **What's wrong:** Material row chevron is a 18 px ">" glyph; concept shows a dropdown triangle at the right edge and a 28 px material swatch at the left. Live swatch is a 28 px timber icon (acceptable).
   **Fix type:** ART. `GameClient/ArtSource/UI/Kit-v5/row_chevron.png` is drawn rotated/used as a dropdown arrow. Add `dropdown_arrow.png` 40x40 (2x), `nine_slice_px [0,0,0,0]`, `intended_1080p_px [20,20]`: prompt (reference crop `build-mode.png` x 418-440, y 646-662): "Flat 2D UI glyph on magenta #FF00FF, 40x40 px: a solid downward-pointing isosceles triangle, 24 px wide, 12 px tall, centred, colour ivory #E8E1CE, crisp edges, no outline, no shadow." Then `SFConstructionSandboxPalette.cpp:187` `KitImage(WidgetTree, TEXT("row_chevron"), 18.f)` → `KitImage(WidgetTree, TEXT("dropdown_arrow"), 20.f)`.

6. **What's wrong:** Build action bar is 44 px tall with 0.7-scaled caps; concept bar is ≈ 40 px tall with caps ≈ 26 px and 18 px labels, centred, bottom at y 918. Live is within 4 px; the LMB cap reads as a dark blob at 18 px.
   **Fix type:** CODE (minor). `SFHudLayout.cpp:244` `PieceScale = ... 0.7f` → `0.75f` and keep height 44. No art change.

Measured OK: panel 480 wide at x 38; title 36 bold with 28 px glyph and 2 px rule; search 36 tall with glyph; tabs 210x44 (selected moss); rail 80 px at x 54; grid 3 x 4 of 100 px with 12 px gaps, Foundation selected with brass edge; material row 40 px; hotbar/vitals as gameplay.

## inventory — FAIL (stale capture)

Judged on `inventory-compare.png`, labelled "Earlier live capture"; the current attempt (`inventory-current-attempt-compare.png`) shows no panel at all. Recapture per protocol step 5 (press I, hover the main-hand axe).

1. **What's wrong:** Tooltip is the engine cursor tooltip rendered inside the panel: it covers bag rows 2-5 (x 100-480, y 690-880) and has no item art and no stat rows. Concept: a floating 360x320 `tooltip_frame` to the right of the panel (x 585, y 124) with 96 px item art at the left, title 26, type line 18 grey, four stat rows (label 18 grey / value 18 ivory), engraved divider, 18 px description.
   **Fix type:** CODE. `Private/Inventory/SFInventoryPanel.cpp:126-141` builds `Tip` as a `UVerticalBox` of four text lines and `line 224` attaches it with `SetToolTip`. Replace with a single panel-owned tooltip: one `UBorder* Tooltip = KitPanel(WidgetTree, TEXT("tooltip_frame"), 1.f, Slate(0.96f), Brass(), FMargin(16.f, 12.f), 3.f)` added to the canvas root with `UCanvasPanelSlot* TipSlot = Root->AddChildToCanvas(Tooltip); TipSlot->SetAnchors(FAnchors(0.34f, 0.03f)); TipSlot->SetAlignment(FVector2D(0.f, 0.f)); TipSlot->SetAutoSize(true); TipSlot->SetPosition(FVector2D(16.f, 86.f));` (= x 669, y 118: right of the panel, level with the equipment column). Content: `UHorizontalBox` { `UImage` 96x96 via `SFScreenArt::Icon(stem, 96.f)` | `UVerticalBox` { `Heading(…, 26)`, `Text(type, 18, Hex("A9A593"))`, rows for Damage / Speed / Durability / Value (`Text(label, 18, Hex("A9A593"))` fill + `Num(value, 18, Ivory())`), `KitImage("divider", 300, 4)`, `Text(description, 18, Ivory())` with `SetAutoWrapText(true)` and `SetWrapTextAt(300.f)` } }. Show it from `NativeTick` for the slot under `GetCachedGeometry().IsUnderLocation(cursor)`, hide otherwise; stat rows come from `FSFInventoryEntry` once the item definition exposes them (until then show only the rows that exist). Drop `SetToolTip` entirely.

2. **What's wrong:** Paperdoll is 190x342; concept 220x396 (the figure's head reaches the top equipment slot, feet the bottom one). Equipment slots are 68 px; concept/rule 11 says 72.
   **Fix type:** CODE. `SFInventoryPanel.cpp:77` `Box->SetWidthOverride(68.f); Box->SetHeightOverride(68.f);` → `72.f, 72.f`; line 97/99 `FVector2D(190.f, 342.f)` and `DollBox 190 x 342` → `220.f, 396.f`; line 94/103/105 padding `FMargin(0.f, 0.f, 0.f, 6.f)` → `12.f` (rule 11 gap). Height check: 5 x 72 + 4 x 12 = 408 ≥ 396, fits.

3. **What's wrong:** Footer lacks "Gold 248" + coin at the right; live right side is the empty-tab hint. Concept footer: weight left, gold right.
   **Fix type:** CODE. `SFInventoryPanel.cpp:156-161`: add `GoldText = Num(WidgetTree, TEXT(""), 18, Ivory())` + `KitImage(WidgetTree, TEXT("icon_coin_stack"), 20.f)` right-aligned; bind to the exchange wallet (`USFExchangeComponent` wallet when present) and collapse the row's right side until a wallet exists. Move `EmptyText` into the grid overlay (centred over the grid) instead of the footer.

4. **What's wrong:** Title glyph is a rotated 22 px procedural diamond; concept shows a small engraved ornament glyph left of "Inventory".
   **Fix type:** CODE. `SFInventoryPanel.cpp:45-50` replace the `CrestBox`/`Crest` border with `KitImage(WidgetTree, TEXT("icon_stag_emblem"), 28.f)` (same piece the market header uses).

5. **What's wrong (stale, verify):** In this frame the bag slots and the tooltip frame show heavy brass corner brackets (≈ 10 px thick "ears") instead of the concept's 1 px hairline. Commit `eb7cc5f` ("Slim Kit-v5 tooltip and slot art") post-dates the capture; `slot_bag_normal.png` now reads as a thin frame with a #0C1418 fill.
   **Fix type:** CAPTURE first. If the recapture still shows brackets: ART, `GameClient/ArtSource/UI/Kit-v5/tooltip_frame.png` 640x360 (2x), `nine_slice_px [20,20,20,20]`, `intended_1080p_px [320,180]`. Prompt (reference `inventory.png` crop x 290-500, y 95-265): "Flat 2D UI frame on magenta #FF00FF, 640x360 px: slate-navy fill #17232A at 92% opacity, one 2 px brass #A98A55 outline with 8 px chamfered corners, a second 1 px brass hairline 6 px inside. No brackets, no scrollwork, no shadow, crisp orthographic edges."

Measured OK: panel x 77-653, y 32-918; title 36 bold + 40 px close X; tabs 100x44 with All selected; 8 x 5 bag grid of 64 px; weight line 18.

## market-board — FAIL

1. **What's wrong:** The inventory panel stays open under the modal (x 77-653, its tabs and bag grid read through the 40% dim and through the board at x 250-653). Concept: only the dimmed master HUD behind the board.
   **Fix type:** CODE (shared, Fix list 3). `Private/Framework/SFPlayerController.cpp:412-429 ToggleMarketDemo`: before showing the board, `if (InventoryPanel) { InventoryPanel->SetVisibility(ESlateVisibility::Collapsed); }` and `if (Palette) { Palette->SetBuildModeActive(false); }`. Same in `ToggleTradeDemo` (390-409).

2. **What's wrong:** "Hold to Buy" has no hold ring; the concept button carries a 36 px ring inside its left end and a moss arc while held. Live draws a 5 px ivory strip under the label on hold (`WithHoldStrip`).
   **Fix type:** CODE (shared, Fix list 2). `SFExchangeWidgets.cpp:473-477` → `const FSFHudButton Buy = SFHudStyle::HoldButton(WidgetTree, TEXT("Hold to Buy"), 300.f); BuyRing = Buy.Ring;` and in `NativeTick` `SFHudStyle::SetHoldProgress(Buy, Fraction)` instead of `BuyProgress->SetWidthOverride(...)` (lines 613, 616). Delete `WithHoldStrip` (lines 91-104).

3. **What's wrong:** Listing table has no "Time Left" column, no column separators, no 1 px row rules, no sort arrow on "Item". Concept: five columns (Item ▲ 250 / Qty 90 / Unit Price 130 / Seller 110 / Time Left 140 = 720), 1 px #3A4348 vertical rules between header cells and horizontal rules between rows.
   **Fix type:** CODE. `SFExchangeWidgets.cpp:435` `HeadCell(TEXT("Item"), 280.f, &ItemHead); HeadCell(TEXT("Qty"), 90.f, nullptr); HeadCell(TEXT("Unit Price"), 170.f, &PriceHead); HeadCell(TEXT("Seller"), 180.f, nullptr);` → widths `250 / 90 / 130 / 110` + `HeadCell(TEXT("Time Left"), 140.f, nullptr)`; inside `HeadCell` give the `UBorder` `Frame(FLinearColor::Transparent, Hex(TEXT("3A4348")), 0.f)` with `SetPadding(FMargin(10.f, 0.f))`, and after the "Item" label add `KitImage(WidgetTree, TEXT("icon_sort_arrow"), 14.f)`. Rows (line 575): the unselected brush `FSlateColorBrush(Hex("17232A", 0.6f))` → `Frame(Hex(TEXT("17232A"), 0.6f), Hex(TEXT("3A4348")), 0.f)` and line 601 row padding `FMargin(0.f, 0.f, 0.f, 1.f)` stays; add a fifth `Cell(Text(WidgetTree, FSFMarketViewModel::TimeLeft(L), 18, Ivory()), 140.f)` (format "11h 28m" from `L.ExpiresAt`).

4. **What's wrong:** Price column coin glyph is an unreadable 18 px blob; concept shows a clear 16 px gold coin before the number. Same in the detail card where live prints "5 g" with no coin.
   **Fix type:** ART + CODE. ART: add `GameClient/ArtSource/UI/Kit-v5/icon_coin.png` 40x40 (2x), `nine_slice_px [0,0,0,0]`, `intended_1080p_px [20,20]`: prompt (reference `market-board.png` crop x 435-455, y 370-390): "Flat 2D game UI icon on magenta #FF00FF, 40x40 px: a single round gold coin seen face-on, warm brass gradient #C9A45C to #8A6A34, 2 px darker rim, tiny engraved stag silhouette in the centre, no text, no shadow." CODE: `SFExchangeWidgets.cpp:595` `KitImage(WidgetTree, TEXT("icon_coin_stack"), 18.f)` → `KitImage(WidgetTree, TEXT("icon_coin"), 20.f)`; detail rows 469/471: prefix the Unit price and Market fee values with the same 20 px coin and print `"%s gold"`.

5. **What's wrong:** Category rail has four rows; concept has five (All Items / Weapons / Materials / Consumables / Tools) with line glyphs. Live glyphs are item icons (axe pair, bread).
   **Fix type:** CODE (minor). `SFExchangeWidgets.cpp:401-402`: add `TEXT("Weapons")` between All Items and Materials, mapped to `ESFInventoryTab::Gear` with a weapons filter once items carry a weapon flag; icons `icon_structures/icon_oak_log/icon_woodcutting_axes/icon_bread` → `icon_map_marker`-style line glyphs are not in the kit, keep the current ones.

6. **What's wrong:** Tabs do not sit on a rule; concept tab strip has a 1 px brass rule along its bottom spanning the board, with the selected tab's bottom edge open. Search field has no magnifier glyph.
   **Fix type:** CODE (minor). After `Contents->AddChildToVerticalBox(Tabs)` (line 396) add `Contents->AddChildToVerticalBox(KitPanel(WidgetTree, TEXT("divider"), 1.f, Brass(), Brass(), FMargin(0.f), 0.f))->SetPadding(FMargin(20.f, 0.f, 20.f, 14.f))`. Search: wrap `Search` in an `UOverlay` with `KitImage(WidgetTree, TEXT("icon_search"), 20.f)` at left padding 14, and `Style.Padding = FMargin(44.f, 4.f, 14.f, 4.f)` as the palette does (`SFConstructionSandboxPalette.cpp:89-100`).

Measured OK: board x 250-1670, y 108-918, dim 40%; header stag 56 + "Market Board" 48 bold + close X 40; tabs 190x44; rail 190x48 rows with All Items selected; search 720x44; rows 56 px with moss selected row; detail card 340 wide with 280x150 well, name 26, rows 18, divider, "You have 1,240 g".

## trade — FAIL

1. **What's wrong:** Inventory panel visible under the modal (as market-board 1).
   **Fix type:** CODE (shared, Fix list 3).

2. **What's wrong:** Window is x 499-1421, y 130-896 (`AddWindow(... 0.26f, 0.12f, 0.74f, 0.83f)`), 766 tall; concept/REVIEW: y 151-842, 691 tall, bottom 76 px above the hotbar. Live bottom edge sits 22 px above the hotbar slots.
   **Fix type:** CODE. `SFExchangeWidgets.cpp:226` `AddWindow(Root, Frame, 0.26f, 0.12f, 0.74f, 0.83f);` → `AddWindow(Root, Frame, 0.26f, 0.14f, 0.74f, 0.78f);`. To fit: line 128 header bottom padding 12 → 8; line 158 grid bottom padding 12 → 8; line 150-151 the per-cell name box 20 px tall is only needed under row 1: give rows 2-3 `NameBox->SetHeightOverride(0.f)` (`Index >= 4`), which removes 40 px; line 285 footer padding `FMargin(0.f, 10.f, 0.f, 4.f)` → `FMargin(0.f, 6.f, 0.f, 0.f)`.

3. **What's wrong:** Item names under the offer slots are clipped: "Oak Lo", "ron Ingo", "Boar Hid" (72 px clip box, 16 px EB Garamond). Icons render ≈ 30 px inside the 64 px slot with the 18 px count centred over them instead of a 48 px icon with the count bottom-right.
   **Fix type:** CODE. `SFExchangeWidgets.cpp:151` `NameBox->SetWidthOverride(72.f); NameBox->SetHeightOverride(20.f); NameBox->SetClipping(EWidgetClipping::ClipToBounds);` → `NameBox->SetWidthOverride(76.f); NameBox->SetHeightOverride(20.f); ItemName->SetOverflowPolicy(ETextOverflowPolicy::Ellipsis);` (no clipping). Icon: line 39-44 `SetItemImage` uses `SetDesiredSizeOverride(48)` but the kit `icon_<stem>` textures are 2x and `SetBrushFromTexture` keeps the texture's own ImageSize, so the overlay slot centres a brush whose desired size is overridden but whose draw is not stretched. Replace the body with `Image->SetBrush(SFScreenArt::Icon(Stem, Size));` (`Public/Framework/SFScreenArt.h:27`, which sets `ImageSize = Size`). Count slot (line 144) padding `FMargin(0.f, 0.f, 4.f, 2.f)` → `FMargin(0.f, 0.f, 6.f, 4.f)` and give the count a 1 px shadow.

4. **What's wrong:** Gold Offered is a bare 18 px label + coin + a 150x44 field; concept is one 300x44 slate plate holding "Gold Offered" at the left and coin + value at the right.
   **Fix type:** CODE. `SFExchangeWidgets.cpp:161-181`: build `UBorder* Plate = KitPanel(WidgetTree, TEXT("search_field"), 1.f, Well(), Hex(TEXT("3A4348")), FMargin(14.f, 0.f), 3.f)` sized 300x44; inside, a `UHorizontalBox` { `Text("Gold Offered", 18, Ivory())` fill | `KitImage("icon_coin", 24)` | value }. For mine the value is the `UEditableTextBox` with `FSlateNoResource()` backgrounds (`Style.BackgroundImageNormal = FSlateNoResource()` etc.), width 90, right-justified; for theirs `Num(…, 20, Ivory())`.

5. **What's wrong:** "Cancel Trade" label is drawn under the Esc cap: the cap (padding 16 at the left) overlaps the "C" because the caption is centred in the 300 px button. Concept: cap at x+16, label starting 12 px right of it.
   **Fix type:** CODE. `SFExchangeWidgets.cpp:264-272`: after `Button(...)`, `Cancel.Label->SetJustification(ETextJustify::Left);` and `Cast<UBorder>(Cancel.Label->GetParent())->SetPadding(FMargin(72.f, 0.f, 24.f, 0.f));` (cap 16 + 40 + 16).

6. **What's wrong:** "Hold to Accept Trade" has no ring and is drawn disabled (text at ≈ 50% alpha) in a frame whose state should show it enabled (my offer locked, partner reviewing per protocol step 5; the capture has my offer unlocked). Concept: enabled primary with the 36 px ring.
   **Fix type:** CODE (shared, Fix list 2) + CAPTURE (lock the offer before the shot).

7. **What's wrong:** Headers read "Your offer" / "Rowan's offer" with a stag emblem for me; concept reads "You" / "Elira" with a 72 px character portrait each.
   **Fix type:** CODE. `SFExchangeWidgets.cpp:122` `bMine ? TEXT("icon_stag_emblem") : TEXT("portrait_villager")` → `bMine ? TEXT("portrait_millwarden_oswin") : TEXT("portrait_villager")` (the player has no portrait asset; use the adventurer portrait when one is generated); `FillSide` line 304: `Side.Name->SetText(FText::FromString(bMine ? TEXT("You") : Source.PlayerName))` (add `bMine` to `FSideWidgets`).

8. **What's wrong:** Empty offer slots are plain; concept shows a faint diagonal cross in each empty cell. Title lacks the two 60x8 flourishes beside "Direct Trade".
   **Fix type:** CODE (minor). Slot: add `KitImage(WidgetTree, TEXT("status_cross"), 28.f)` to each cell overlay with `SetRenderOpacity(0.18f)`, collapsed when the slot fills. Title: wrap the heading in a `UHorizontalBox` with `KitImage(WidgetTree, TEXT("divider"), 60.f, 8.f)` on each side, 16 px padding.

Measured OK: title 48 bold + close X; two columns split by the 2 px rule; 4 x 3 grid of 64 px slots at 12 px gaps; Lock Offer 300x56 secondary; partner `status_plate`; warning line 24 px glyph + 18 amber; Cancel 300x56; "Expires 00:47" 16 grey under Cancel; dim 40%.

## gathering — FAIL

1. **What's wrong:** No gathering UI in the frame: no Gathering Skills panel, no channel ring, no target frame, no "E Chop" prompt, no skill-gain toast. The scenario frame is the field at night under a tree. Nothing of the concept's UI can be judged.
   **Fix type:** CAPTURE. Protocol step 3 lighting (WP2 Silverbrook daylight) and a harvest in progress on an oak with the skills panel open (`ASFPlayerController::ToggleGatheringSkillsPanel`). Reject any frame where the player is in deep shadow (this one is).

2. **What's wrong (from code, will show on recapture):** `USFGatheringSkillsPanel` is 522x700 at (2.7%, 6%) (`SFGatheringSkillsPanel.cpp:26, 189-190`); the concept panel is x 2-27% (480 wide), y 7-77% (≈ 756 tall), four 150 px rows with a 72 px icon well, Source Sans level numeral under the icon, slender 300x12 moss XP bar with "420 / 1,000" at the right, "Next:" + 24 px unlock icon + name.
   **Fix type:** CODE. Line 26 `SkillsPanelSize(522.f, 700.f)` → `(480.f, 756.f)`; line 190 `SetPositionInViewport(FVector2D(0.027 * Viewport.X, 0.06 * Viewport.Y))` → `(0.02 * Viewport.X, 0.07 * Viewport.Y)`; XP bars via `MakeFramedBar(WidgetTree, TEXT("bar_xp"), 300.f)` with the numeral `Num(…, 18)` right of the bar. Re-judge on the recapture.

3. **What's wrong (from code):** Harvest prompt reads "F Harvest"; concept "E Chop" (verb per node). Rule: the key comes from the input mapping (F is correct); the verb should name the action.
   **Fix type:** CODE. `SFHudLayout.cpp:53` `case ESFPromptContext::Harvest: return {{TEXT("F"), TEXT("Harvest")}};` → the verb from the aimed node: add `FString ASFGatheringPilotNode::ActionVerb() const` ("Chop" for logging, "Mine", "Gather", "Fish") and in `SetPrompts` substitute `Segment.Action` for Harvest.

## crafting-station — FAIL

1. **What's wrong:** Two "Workbench" headings: the 410x70 target frame at top centre (y 43-113) prints "Workbench" and overlaps the station panel's top edge (panel top y 98), 24 px above the panel's own 36 px title. Concept has one title inside the panel and no target frame while the station is open.
   **Fix type:** CODE. `Private/Framework/SFHudLayout.cpp:294` `TargetBoxRoot->SetVisibility(CombatTarget ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);`. Add to `ASFPlayerController` `bool IsStationPanelOpen() const { return GatheringHud && GatheringHud->IsStationShown(); }` (expose `bool USFGatheringHud::IsStationShown() const { return StationPanel && StationPanel->GetVisibility() != ESlateVisibility::Collapsed; }`), then in `NativeTick`: `const bool bStation = Cast<ASFPlayerController>(Controller) && Cast<ASFPlayerController>(Controller)->IsStationPanelOpen();` and `TargetBoxRoot->SetVisibility(CombatTarget && !bStation ? … : Collapsed)`.

2. **What's wrong:** The "F Refine" prompt bar (bottom y 918, 52 tall → y 866-918) overlaps the panel's bottom edge (panel y 98-890) and reads through it. Concept: no prompt bar while the station is open; the Craft button carries the key cap.
   **Fix type:** CODE. Same `bStation` flag: `SFHudLayout.cpp:282` `ESFPromptContext Context = bBuildMode ? … : None;` → when `bStation`, force `Context = ESFPromptContext::None`.

3. **What's wrong:** Recipe names overflow the 318 px recipe column into the right column: "Starter Tool Repair Kit", "Foundation Material Bundle", "Wall/Door/Roof Material Bundle" cross the vertical divider by 40-90 px and collide with the ingredient row. Concept rows: 56 px, 40 px icon, one 20 px name that fits ("Oak Planks").
   **Fix type:** CODE. `Private/Gathering/SFStationPanel.cpp:230` `Text(WidgetTree, SFStationRecipes::ItemDisplayName(...), 20, Ivory())` → size 18, then `NameText->SetOverflowPolicy(ETextOverflowPolicy::Ellipsis); NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));`; line 229 icon `50.f` → `40.f`; line 233 row height `62.f` → `56.f`, padding 8 → 6. Also shorten the content names in `SFStationRecipes` ("Foundation Bundle", "Wall Bundle", "Door Bundle", "Roof Bundle", "Tool Repair Kit") — content change, Codex.

4. **What's wrong:** Panel top is y 98 (anchor 0.091) and bottom 890; concept panel y 85-855 (x 33-78%). Live bottom edge sits 2 px above the prompt-bar zone and 60 px closer to the hotbar than the concept.
   **Fix type:** CODE. `Private/Gathering/SFGatheringHud.cpp:116` `StationSlot->SetAnchors(FAnchors(0.554f, 0.091f)); … SetSize(FVector2D(859.f, 792.f));` → `FAnchors(0.555f, 0.08f)` and `FVector2D(864.f, 770.f)` (x 634-1498, y 86-856).

5. **What's wrong:** Selected recipe row has no left arrow notch; concept shows a small brass chevron notch on the panel's left edge pointing at the selected row.
   **Fix type:** CODE (minor). In `Rebuild` (line 226-233) wrap the selected row in an `UOverlay` with `KitImage(WidgetTree, TEXT("row_chevron"), 14.f)` aligned `HAlign_Left/VAlign_Center`, padding `(-18, 0, 0, 0)`, rotated 180° (`SetRenderTransformAngle(180.f)`) so it points right.

6. **What's wrong:** Ingredients heading is 18 px regular; concept "Ingredients" 18 grey above inset rows that are 44 px tall with no brass frame. Live uses the `queue_row` brass frame for ingredient rows (52 px).
   **Fix type:** CODE (minor). `SFStationPanel.cpp:245` `StationFrame(WidgetTree, TEXT("queue_row"), …)` → a flat `UBorder` with `Frame(Hex(TEXT("0E171C"), 0.85f), Hex(TEXT("3A4348")), 0.f)`; line 264 height `52.f` → `44.f`.

Measured OK: title 36 bold + 2 px rule; search 42 with glyph; three tabs 44 tall with All selected; right column name 30 bold, description 19, 136 px output art, "Ingredients", have/need with crimson 0 + underline + cross glyph, stepper 52/90/52 x 46, "Craft time 3 s", Craft primary 370x56 with the F cap; queue title + "Nothing crafting".

## defeated — FAIL

1. **What's wrong:** Button reads "Respawn in 00:04" with a dark 36 px disc and no visible arc; concept: "Hold to Respawn" with the 36 px brass ring and a moss arc (rule 8 hold button). The live arc is painted at `RespawnProgress` but the ring art sits on a dark disc so the arc is invisible at low progress.
   **Fix type:** CODE (shared, Fix list 2). `Private/Characters/SFE1VitalsHud.cpp:204-220` → `const FSFHudButton Respawn = SFHudStyle::HoldButton(WidgetTree, TEXT("Hold to Respawn"), 480.f);` and the countdown becomes a separate line under the button: `CountdownText = Text(WidgetTree, TEXT(""), 18, Hex(TEXT("BDB6A2")))` ("Automatic respawn in 00:04"), placed where "You keep everything you carry." is (line 221), and that sentence moves 6 px lower. `Present` line 234 writes the countdown into `CountdownText` as today. `NativePaint` (240-260) → `SFHudStyle::PaintHoldArc(OutDrawElements, LayerId, Respawn.Ring, RespawnProgress)`. Remove the local `RingDisc`.

2. **What's wrong:** Window is 840 wide (x 540-1380) and 664 tall (y 192-856); concept/rule 12 centre window x 499-1421 (922), y 151-842 (691).
   **Fix type:** CODE. `SFE1VitalsHud.cpp:124` `WindowSlot->SetAnchors(FAnchors(0.5f, 0.178f)); … SetAutoSize(true);` → `WindowSlot->SetAnchors(FAnchors(0.26f, 0.14f, 0.74f, 0.78f)); WindowSlot->SetOffsets(FMargin(0.f));` (drop AutoSize), line 127 `WindowWidth->SetWidthOverride(760.f)` → `842.f`; cards (line 195) `Sized(Card, 326.f, 256.f)` → `(340.f, 240.f)` with a 24 px gap (line 195 padding `22.f` → `24.f`).

3. **What's wrong:** Respawn cards: live image 312x150 with the title and status on a slate strip below (card 256 tall); concept cards are the image filling the card (326x230) with the title/status overlaid on a dark gradient in the lower 70 px, moss edge + moss tick roundel on the selected card. Live tick roundel is a flat moss disc whose ivory tick is not visible at 24 px.
   **Fix type:** CODE. `SFE1VitalsHud.cpp:166-197 AddCard`: make `Card` an `UOverlay` { `Picture(Art, FVector2D(340.f, 240.f), shade)` fill; `UBorder` gradient strip (`Frame(Hex("0E171C", 0.78f), Transparent, 0)`) aligned bottom, 72 px tall, containing title 24 + status 18; `CardFrame` (`respawn_image_well`, moss tint when selected) as the top layer with `FSlateNoResource` fill }. Roundel: `Frame(Moss(), Ivory(), 19.f)` keep, tick `KitPicture(TEXT("status_tick"), FVector2D(24.f, 24.f), Ivory())` → `FVector2D(28.f, 28.f)` and give the disc a 2 px ivory outline (`OutlineSettings.Width = 2.f`).

4. **What's wrong:** Footer sentence differs: live "You keep everything you carry." vs concept "Dropped items remain at the defeat site." This is a product rule, not a layout defect; the text must match whichever rule ships. Record the decision in the README; no fix prescribed here.

5. **What's wrong:** Scene: no boar or fallen body, character upright, no world context. Not UI code; CAPTURE/animation. Not counted against the widget.

Measured OK: dim 40% + desaturation; "Defeated" 72 serif, crimson rule with stag, "Defeated by Wild Boar" 26; two cards side by side with the shrine selected (moss edge) and the clock roundel on the bedroll; hotbar/vitals dimmed with "0 / 80".

## 9-loading-screen — FAIL (stale capture)

Judged on `loading-screen-compare.png` ("Earlier live capture"); `loading-current-attempt-compare.png` proves the collector starts after pawn creation. Recapture with a capture hook on `FCoreUObjectDelegates::PreLoadMap` (or `-ExecCmds="HighResShot …"` 1.5 s after launch) so the loading widget is in the frame.

1. **What's wrong:** Progress bar: live is a 620x28 dark trough with a 1 px brass hairline and the fill barely started (≈ 20 px); concept is a 620x14 brass bar (fill brass #C9A45C, trough #1A2228) with diamond end caps outside both ends and a small diamond tick above the centre, fill at ≈ 60%.
   **Fix type:** ART + CODE. ART: `GameClient/ArtSource/UI/LoadingScreen-v5/source/ls_bar_frame.png` (processed 1024x32) and `ls_bar_fill.png` (512x16). Prompt for the frame (reference `9-loading-screen.png` crop x 290-690, y 505-525): "Flat 2D UI progress-bar frame on magenta #FF00FF, 1280x56 px: a long horizontal slate trough #1A2228, 24 px tall, with a 2 px brass #A98A55 outline; at each end a small brass diamond cap 40 px tall centred on the bar's axis, extending 24 px past the trough; one 16 px brass diamond ornament centred above the bar, touching its top edge. No text, no shadow, crisp orthographic edges." Fill prompt: "Flat 2D UI bar fill on magenta #FF00FF, 1024x32 px: a horizontal brass bar, soft vertical gradient #D4B06A top to #9A7A42 bottom, 1 px lighter highlight line at the top, no end caps, no text." CODE: `Private/Framework/SFLoadingScreen.cpp:92` `constexpr float BarWidth = 620.f, BarHeight = 28.f, BarInset = 4.f;` → `BarHeight = 14.f, BarInset = 2.f`, and line 114 the frame slot size `FVector2D(BarWidth + 48.f, BarHeight + 20.f)` positioned at `(960 - (BarWidth + 48) / 2, 931 - (BarHeight + 20) / 2)` so the caps and the top diamond draw outside the trough; line 136 the ease `0.92 * (1 - exp(-t/6))` → `0.92 * (1 - exp(-t/2.5))` so the bar is visibly filled after 1.5 s.

2. **What's wrong:** Emblem is 276 px; spec (REVIEW implementation notes) and concept measure 220 px with the diamond's bottom vertex 40 px above the title's cap line. Live emblem's diamond bottom overlaps the "S" of the title by ≈ 10 px.
   **Fix type:** CODE. `SFLoadingScreen.cpp:106` `.Position(FVector2D(960.f - 138.f, 447.f - 138.f)).Size(FVector2D(276.f, 276.f))` → `.Position(FVector2D(960.f - 110.f, 430.f - 110.f)).Size(FVector2D(220.f, 220.f))`.

3. **What's wrong:** Zone title is 84 px; spec 72 px display title. Live letterforms are EB Garamond Regular (accepted by the concept, which is mixed-case), but 12 px too large, and the engraved rule under it is 506 px while the concept rule spans ≈ 560 px with a centre diamond.
   **Fix type:** CODE. `SFLoadingScreen.cpp:109` `Font(LoadingScreenFont(TEXT("FontFace_EBGaramondRegular"), 84))` → `72`; line 110 rule slot `Size(FVector2D(506.f, 48.f))` at `960 - 253` → `Size(FVector2D(560.f, 48.f))` at `960 - 280`.

4. **What's wrong:** Spinner ring is 2 px thin; concept ring is a 40 px ring with a 5 px stroke and a 270° brass arc.
   **Fix type:** ART. `LoadingScreen-v5/source/ls_spinner.png` (processed 128x128): prompt "Flat 2D UI spinner on magenta #FF00FF, 128x128 px: a 270-degree circular arc, radius 52 px, stroke 14 px, brass #A98A55 with a lighter #D4B06A leading end, rounded caps, centred, no shadow, no text."

Measured OK: full-bleed splash matches the concept art; vignette; tip line 24 px centred at y 862; spinner at y 989.

---

## Fix list for the implementer

Shared fixes first. Each item is executable from this text alone. **Engineer** = HUD engineer; **Codex** = art regeneration or mechanical edits.

1. **Engineer — `SFHudStyle::KitPanel` inset flag.** `Public/Framework/SFHudStyle.h:172`: add `bool bInset = true` as the last parameter; when false, add no `InsetPixels` to the padding. Call it with `false` for `slot_catalogue_*` (`SFConstructionSandboxPalette.cpp:154, 173`) and the rail `tab_*` tiles (`:129`); shrink the tile icon to 56 (`:156`) and the rail glyph to 32 (`:131`); add `SetOverflowPolicy(Ellipsis)` on both captions. Fixes build-mode 2-3.

2. **Engineer — one hold button.** Add to `SFHudStyle.h`: `struct FSFHudHoldButton : FSFHudButton { UImage* Ring = nullptr; };` `inline FSFHudHoldButton HoldButton(UWidgetTree* Tree, const FString& Label, float MinWidth)` = `Button(Tree, Label, EButtonKind::Primary, MinWidth)` wrapped in an `UOverlay` with `KitImage(Tree, TEXT("hold_confirm_ring"), 36.f)` at `HAlign_Left / VAlign_Center`, padding `(20, 0, 0, 0)`, and the caption padding `FMargin(72.f, 0.f, 24.f, 0.f)` so the label clears the ring; `inline int32 PaintHoldArc(FSlateWindowElementList& Out, int32 LayerId, const UImage* Ring, float Fraction)` = the arc code now at `SFE1VitalsHud.cpp:247-258` (radius 0.36 x side, 5 px, #7FA07A, clockwise from 12). Use it for Buy (`SFExchangeWidgets.cpp:473-477, 613-616`), Accept Trade (`:280-284, 350-351`) and Respawn (`SFE1VitalsHud.cpp:204-220, 240-260`); delete `WithHoldStrip` (`:91-104`). Fixes market 2, trade 6, defeated 1.

3. **Engineer — modals close the side panels.** `SFPlayerController.cpp:390-429`: in `ToggleTradeDemo` and `ToggleMarketDemo`, before showing, collapse `InventoryPanel` and call `Palette->SetBuildModeActive(false)`. Fixes market 1, trade 1.

4. **Engineer — station open state hides the target frame and prompt.** `USFGatheringHud::IsStationShown()`, `ASFPlayerController::IsStationPanelOpen()`, and in `SFHudLayout.cpp:282, 294` force `Context = None` and collapse `TargetBoxRoot` while open. Fixes crafting 1-2.

5. **Engineer — vitals numerals.** `SFE1VitalsHud.cpp:79-81` level numeral bold 22 with shadow + 1 px outline, bottom padding 16; `:108` keep `StaminaText` and write "max / max" from a typed `PlayerStamina` setting (default 150). Fixes gameplay 2-3.

6. **Engineer — trade window geometry and fields.** `SFExchangeWidgets.cpp:226` anchors `(0.26, 0.14)-(0.74, 0.78)`; name boxes 76 wide, ellipsis, rows 2-3 nameless; `SetItemImage` uses `SFScreenArt::Icon(stem, Size)`; Gold Offered as one 300x44 `search_field` plate (label left, coin + value right); Cancel label left-justified with 72 px left padding; headers "You"/<partner>, portraits `portrait_*` 72; faint `status_cross` at 18% in empty cells; title flourishes `divider` 60x8 each side. Fixes trade 2-5, 7-8.

7. **Engineer — market table and detail card.** Five columns 250/90/130/110/140 with 1 px #3A4348 cell rules and a sort arrow on Item; rows get a bottom rule; `icon_coin` 20 px before every price (rows, Unit price, Market fee) printed as "24 gold"; a brass rule under the tab strip; magnifier glyph in the search field; Weapons rail row. Fixes market 3-6.

8. **Engineer — inventory tooltip and sizes.** Replace `SetToolTip` with a panel-owned 360 px `tooltip_frame` at canvas (0.34, 0.03) + (16, 86) holding 96 px art, title 26, type 18 grey, stat rows, divider, description 18 wrapped at 300; equipment slots 72 with 12 px gaps; paperdoll 220x396; footer gold readout; `icon_stag_emblem` 28 as the title glyph. Fixes inventory 1-4.

9. **Engineer — station panel.** `SFGatheringHud.cpp:116` anchors `(0.555, 0.08)` size `864x770`; recipe rows 56 px, 40 px icon, name 18 with ellipsis; ingredient rows flat 44 px; selected-row chevron. Fixes crafting 3-6.

10. **Engineer — defeated window.** Anchors `(0.26, 0.14)-(0.74, 0.78)`, content width 842; cards 340x240 with the image filling the card and the title/status on a 72 px bottom strip; tick 28 px on a 2 px-outlined moss disc; countdown as an 18 px line under the hold button. Fixes defeated 2-3.

11. **Engineer — build palette status and anchor.** Controller passes only "Valid placement"/"Invalid placement" to `SetStatusText` (tick/cross glyph swaps), no wrap; panel anchor `(0.02, 0.03)`; `dropdown_arrow` 20 px in the material row; build caps scale 0.75. Fixes build-mode 4-6.

12. **Engineer — gameplay footer distance and harvest verb.** Minimap footer "<zone> · <n> m" from the tracked objective; `ASFGatheringPilotNode::ActionVerb()` feeds the Harvest prompt ("Chop", "Mine", "Gather", "Fish"). Fixes gameplay 4, gathering 3.

13. **Engineer — gathering skills panel geometry.** `SFGatheringSkillsPanel.cpp:26, 189-190`: 480x756 at (2%, 7%); XP bars `MakeFramedBar("bar_xp", 300)` with 18 px numerals. Fixes gathering 2 (verify on recapture).

14. **Engineer — loading screen numbers.** `SFLoadingScreen.cpp:92, 106, 109-110, 114, 136`: bar 620x14 with inset 2 and caps drawn outside; emblem 220 at (850, 320); title 72; rule 560; ease time constant 2.5 s. Fixes loading 1-3 (code half).

15. **Codex — art: `quest_tracker_frame.png`** 560x308 (2x), margins [24,24,24,24], 280x154 at 1080p; plain 2 px brass rectangle with 10 px chamfers and an inner engraved hairline, no swashes (prompt in gameplay-hud 5). Process + import + rerun `scan_borders.py`.

16. **Codex — art: `icon_coin.png`** 40x40 (2x), 20 px at 1080p, single face-on gold coin (prompt in market-board 4); add to `kit.json` with `nine_slice_px [0,0,0,0]`, `intended_1080p_px [20,20]`.

17. **Codex — art: `dropdown_arrow.png`** 40x40 (2x), 20 px, ivory down-triangle (prompt in build-mode 5); add to `kit.json`.

18. **Codex — art: loading bar frame, fill and spinner.** `LoadingScreen-v5/source/ls_bar_frame.png` 1280x56, `ls_bar_fill.png` 1024x32, `ls_spinner.png` 128x128 (prompts in loading 1 and 4); update `LoadingScreen-v5/kit.json` sizes (frame 668x36 processed, fill 512x16, spinner 128x128), run `tools/Process-HudKit.ps1 -Kit GameClient/ArtSource/UI/LoadingScreen-v5` and `Import-LoadingScreen-v5.py`.

19. **Codex — content: recipe display names** in `SFStationRecipes` shortened to fit 220 px at 18 px ("Foundation Bundle", "Wall Bundle", "Door Bundle", "Roof Bundle", "Tool Repair Kit").

20. **Codex — art, conditional: `tooltip_frame.png`** only if the inventory recapture still shows bracket corners (prompt in inventory 5).

21. **Codex — recapture per protocol** after items 1-19: E1 session on `Greenlands_Silverbrook`, WP2 daylight on the river trail, `HighResShot 1920x1080`; gameplay with a boar aimed + toast; build mode from the E1 controller (or "blocked" in the README); inventory with the axe hovered; market with Iron Axe selected; trade with my offer locked and the partner reviewing; gathering mid-chop with the skills panel open; station aimed; defeated; loading via a PreLoadMap hook. Then re-review.
