# UI v5 final2 — independent acceptance review

Reviewer: Fable (art director / acceptance judge), 2026-10-21. Inputs: `docs/Game-Design/concept-art/UI/v5/*.png`, `IMPLEMENTATION-FIX.md`, `REVIEW.md`, `docs/blueprints/ui-hud.md`; the nine `*-live.png` frames here (read at full 1920x1080) and their `*-compare.png`; `README.md` staging notes; `GameClient/ArtSource/UI/Kit-v5/kit.json` + PNGs; widget code under `GameClient/Source/ShatteredFrontier`. All px are 1080p design px measured on the `*-live.png` frames. Verdict rule: PASS only when a player would take the live frame for the concept; anything broken fails the screen.

Result: **0 PASS / 9 FAIL.** Five screens (gathering, crafting-station, defeated, loading-screen, and the gameplay target/prompt/toast state) were not staged; their frames are plain gameplay and are judged on what is visible plus the code that would draw them.

Cross-screen facts (established from the frames and the code; the per-screen lists reference them):

- **A. The kit is absent from any widget built during the startup map load.** `Private/Framework/SFHudKit.cpp:9` gates the load on `GIsRunning`, which the engine sets only after `GEngine->Start()` has loaded the map and run `BeginPlay`. `ASFPlayerController::BeginPlay → InitializeLocalControls` (`SFConstructionNetwork.cpp:1262`) creates the Build Catalogue there, so every `Kit()` call in `USFConstructionSandboxPalette::NativeOnInitialized` returns null: all frames fall back to procedural `Frame()` boxes and every `KitImage` draws `UImage`'s default brush, a white quad. Widgets built later (vitals, market, trade, inventory) get the kit. Fix list 1.
- **B. `SFHudStyle::KitImage` paints a white square for a missing piece** (`Public/Framework/SFHudStyle.h:286-300`: the `UImage` keeps its default white brush when `Kit(Name)` is null). Fix list 1.
- **C. The minimap footer prints the literal text `00B7`.** `Private/Framework/SFHudLayout.cpp:327` formats `TEXT("%s 00B7 %d m")`; every live frame reads "Silverbrook 00B7 72 m" and the 210 px string overruns the 168 px `minimap_footer` plate (text x 1670-1880, plate x 1692-1860). Fix list 2.
- **D. `USizeBox::SetWidthOverride` does not cap width in a Fill slot.** Trade's 300 px gold plate, lock plate and status plate render 394 px wide (x 548-942 and 978-1372); the market search field, in an overlay slot that defaults to Left, collapses to the text box's 190 px desired width (x 515-705) instead of the 720 px box. Fix list 4, 6.
- **E. Hold ring and coin art read as dots.** `hold_confirm_ring.png` (72x72 2x) is a #111C23 disc with a 2 px rim; at 36 px it is a dark dot with a 1 px edge (market x 1310-1346 y 730-766, trade x 970-1006 y 822-858). `icon_coin.png` (40x40) fills 65 % of its canvas with a stag-face coin; at 20 px it is a 13 px brass blob (market rows x 866-880). Fix list 11, 12.
- **F. Staging.** All frames: mannequin pawn, Silverbrook square/field, no WP2 river-trail lighting; gathering and crafting frames show the pilot stations as grey checker `/Engine/BasicShapes/Cube` boxes with mirrored yellow `UTextRenderComponent` labels ("Forge E: craft, C: recipe | 1 Iron Ingot / 8 Iron Nails", `SFGatheringPilot.cpp:119-124, 250-256`). No target frame, prompt bar, toast, skills panel, station panel, defeated modal or loading screen was captured. Fix list 14, 15.
- **G. In-game `button_normal` and `tab_normal` carry corner brackets that the committed PNGs do not have.** The kit sources (`button_normal.png`, `tab_normal.png`, slimmed in `ba633d6` 14:12) are plain chamfered plates; the live Cancel Trade, Offer Locked and inventory Materials/Tools/Food tabs show the bracketed `button_hover`-style corners. `T_Kit_tab_normal.uasset` did not change in the 18:35 import (`6a1ba24`: 22568 -> 22568 bytes). Fix list 13.

---

## gameplay-hud — FAIL

1. **What's wrong:** Minimap footer reads "Silverbrook 00B7 72 m" (cross-screen C); the string is 210 px on a 168 px plate and runs 20 px past the plate on both sides. Concept: "Silverbrook · 227 m" inside the 180x32 plate.
   **Fix type:** CODE. `Private/Framework/SFHudLayout.cpp:327` replace `TEXT("%s 00B7 %d m")` with `TEXT("%s · %d m")`. Also `ZoneText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis)` at the footer's construction (`:167`) so a long zone name can never leave the plate.

2. **What's wrong:** No target frame (concept 410x70 at y 43-113 "Wild Boar 320 / 320"), no "F Interact" prompt bar (bottom y 918), no toasts (three 280x66 under the tracker). The boar the README names is the dark lump at x 1230-1290 y 555-590, not aimed.
   **Fix type:** CAPTURE. Stage with the combat pilot: aim the boar inside the pilot's aim range so `USFHudLayout::NativeTick` sets `CombatTarget` (`SFHudLayout.cpp:294-310`), then fell one oak so a harvest toast is live (`USFGatheringHud` toasts). Console: `SF.Combat.Spawn Boar 300` (or the pilot's spawn command recorded in the README), face it, `Shot showui`.

3. **What's wrong:** Scene: mannequin pawn, open field, overcast flat light; concept is the E1 adventurer on the river trail in WP2 daylight. Hotbar slots 1-4 are build thumbnails (product gap already recorded in the previous review; not counted).
   **Fix type:** CAPTURE/LEVEL (IMPLEMENTATION-FIX capture protocol 1-2). Use `review_silverbrook_in_game.py` reference camera on the river trail; the mannequin stays until the E1 character mesh exists (record as a product gap in the README).

Measured OK: badge 116 at x 38 bottom 1037 with "1" in the plate; HP 380x32 "80 / 80" and stamina "150 / 150" 10 px apart; hotbar 6 x 88 at x 672-1256 with numerals and the "2" count; minimap 250 at x 1663 y 22 with ring and arrow; tracker 280 wide at x 1620 y 340, plain chamfered brass frame (previous item 15 done), title 24 bold, 18 px row with checkbox and "0 / 1".

## build-mode — FAIL

1. **What's wrong:** Twelve white squares where kit art should be (cross-screen A/B): title glyph 28 px (x 44-72 y 58-86), search magnifier 20 px, four rail glyphs 32 px (Structures/Decor/Terrain/Favourites), four tile thumbnails 56 px (Foundation/Wall/Door/Roof), material swatch 28 px, dropdown arrow 20 px, the tick after "Unlimited dev stock". Every frame in the panel is the procedural fallback (flat `Frame()` boxes; no `panel_frame` corner art, no `tab_*` art, no `slot_catalogue_*` art). The action bar at the bottom (built by `USFHudLayout`, later) is correct kit art, which proves the asset itself is fine.
   **Fix type:** CODE (shared). Fix list 1: `Private/Framework/SFHudKit.cpp:9` `if (!bTried && !Kit.IsValid() && GIsRunning)` → `if (!bTried && !Kit.IsValid() && !IsRunningCommandlet())`. Guard: `SFHudStyle.h:297` add `else { Image->SetVisibility(ESlateVisibility::Collapsed); }` so a missing piece is empty, never white.

2. **What's wrong:** Panel x 38-518, y 32-872 (840 tall); concept y 32-886. Status line reads "Invalid placement" with a cross and no ghost wall in the world; the concept shows a valid placement with the green ghost. The README says the production catalogue state is blocked.
   **Fix type:** CAPTURE. Enter build mode with a tile selected and the cursor on buildable ground so `SetPlacementValid(true)` runs (`SFConstructionSandboxPalette.cpp:266`), ghost visible. If the E1 controller cannot place on `Greenlands_Silverbrook`, keep the README's "blocked" line; the panel is still judged on item 1.

Measured OK (geometry only): title 36 bold with 2 px rule; search 432x36; tabs 210x44 (Voxel Objects moss); rail 4 x 80 at x 44 with 16 px captions inside the tiles; grid 3 x 4 of 100 px at 12 px gaps, Foundation selected with a brass edge; material row 40; Required Materials box 432 wide, heading 20, row 18; action bar 52 tall centred at bottom 918 with the LMB/R/X/Esc caps at 0.75 scale.

## inventory — FAIL

1. **What's wrong:** The footer is drawn outside the panel: "Weight 4.0 / 24 kg" baseline at y 945, 27 px below the frame's bottom edge (y 918), across the HP bar and badge (x 115-285 overlaps the badge at x 38-154 and the bar at x 185+). Cause: content height 880 px (title 44 + tabs 50 + equipment 5 x 84 + 4 + grid 5 x 66 + 6 + footer 26) against 834 px inside the frame (886 minus 2 x (8 + 18 inset)); the `UVerticalBox` overflows unclipped.
   **Fix type:** CODE. `Private/Inventory/SFInventoryPanel.cpp`: line 93 and 102-104 equipment slot padding `FMargin(0.f, 0.f, 0.f, 12.f)` → `FMargin(0.f, 0.f, 0.f, 6.f)` and no padding on the last slot of each column (saves 36 px); line 53 title padding `4.f` → `0.f`; line 140 grid padding `6.f` → `2.f`; line 154 footer `FMargin(4.f, 4.f, 4.f, 0.f)` → `FMargin(4.f, 2.f, 4.f, 0.f)` (total 46 px, content 834). Guard: after line 43 `Frame->SetClipping(EWidgetClipping::ClipToBounds);` so nothing can ever paint over the HUD again. Paperdoll box line 98 `396.f` → `380.f` to match the shorter column (5 x 72 + 4 x 6 = 384).

2. **What's wrong:** No tooltip (concept: 360x320 `tooltip_frame` at x 585 y 124 with 96 px art, "Iron Axe", type line, four stat rows, rule, description). The README hovered the bag axe and the main-hand slot; `UpdateTooltip` (`SFInventoryPanel.cpp:393-401`) tests only `SlotFrames` (bag cells) against `FSlateApplication::Get().GetCursorPos()`, the OS cursor.
   **Fix type:** CODE + CAPTURE. Line 398-401: also test `MainHandFrame` (map it to the equipped tool's entry). CAPTURE: move the real cursor with `FSlateApplication::Get().SetCursorPos(FVector2D(140, 624))` (centre of bag cell 1) from the capture script before `Shot showui`; a synthetic pointer event does not move the OS cursor.

3. **What's wrong:** No "Gold 248" + coin at the footer's right (concept). `GoldRow` stays collapsed until `USFExchangeComponent` exists (`:254`); the market demo already shows "You have 1,240 g" from the same wallet.
   **Fix type:** CODE. `SFInventoryPanel.cpp:254` read the wallet through the same accessor the market card uses (`ToggleMarketDemo`'s fake exchange when no component exists) so the demo wallet shows here too; print `Gold 1,240` (label 18 grey + numeral 18 ivory + `icon_coin` 20).

4. **What's wrong:** Tabs Materials/Tools/Food show bracketed corners (cross-screen G); the concept and `tab_normal.png` are plain chamfered plates.
   **Fix type:** ART/pipeline (fix list 13).

Measured OK: panel x 80-655, y 32-918; `icon_stag_emblem` 28 + "Inventory" 36 bold + close X 40; tabs 100x44 with All selected (moss); equipment 5 + 4 slots of 72 with ghost icons; paperdoll centred; 8 x 5 bag grid of 64 px at 2 px gaps with the axe and pick, counts bottom-right.

## market-board — FAIL

1. **What's wrong:** Search field is 190x36 at x 515-705 (concept 720x44 spanning the table, x 455-1080 of the 1672 image = 520-1240). The `UEditableTextBox` sits in an overlay slot with the default Left alignment, so it takes its desired width (cross-screen D).
   **Fix type:** CODE. `Private/Exchange/SFExchangeWidgets.cpp:456` `SearchStack->AddChildToOverlay(Search);` → `UOverlaySlot* SearchSlot = SearchStack->AddChildToOverlay(Search); SearchSlot->SetHorizontalAlignment(HAlign_Fill); SearchSlot->SetVerticalAlignment(VAlign_Fill);`.

2. **What's wrong:** "Hold to Buy" ring is a 36 px dark dot with a 1 px rim (x 1310-1346, y 730-766); concept ring is a brass ring ~5 px thick with a dark centre and the moss arc visible inside (cross-screen E).
   **Fix type:** ART (fix list 11).

3. **What's wrong:** Price coin is a 13 px blob before every price and in the card's "200 gold" / "10 gold" rows (cross-screen E). Concept: a clean 18-20 px gold coin.
   **Fix type:** ART (fix list 12).

4. **What's wrong:** A 6 px scrollbar shows at x 1230-1236 beside six rows (`ListSize` 340 tall, rows 6 x 57 = 342). Concept: no scrollbar with six rows.
   **Fix type:** CODE. `SFExchangeWidgets.cpp:484` `SetHeightOverride(340.f)` → `348.f`, and after line 487 `Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed)` until `Rows->GetChildrenCount() > 6` (set in `Rebuild`).

5. **What's wrong:** "Stew" row has no icon (x 530-570 blank); every other row has one.
   **Fix type:** ART/content (minor). Add `GameClient/ArtSource/UI/Icons/T_Icon_stew` (or map the demo id to `icon_bread`) so `SetItemImage` finds a texture.

Measured OK: dim 40 % with no inventory panel underneath (previous item 3 done); modal x 250-1670, y 108-918; stag + "Market Board" 48 bold + close X; tabs 190x44 on a brass rule; rail 5 x 190x48 with glyphs, All Items moss; header Item ▲ / Qty / Unit Price / Seller / Time Left with 1 px cell rules; rows 56 with rules, Iron Axe moss-selected; card 340 wide with the 280x150 well, name 26, Quantity / Unit price / Seller, rule, Market fee, "You have 1,240 g", hold button 300x56.

## trade — FAIL

1. **What's wrong:** The footer breaks out of the window: window y 150-845, "Hold to Accept Trade" y 800-856 (its bottom 11 px below the frame edge), the hold dot at y 822-858 straddles the frame line, "Expires 01:22" at y 872 is outside the window. Cause: the 4 x 3 offer grid is a `UUniformGridPanel`, so every row takes the tallest cell (64 slot + 20 name + 12 padding = 96); rows 2-3 are 96 instead of 76 and the column is 40 px too tall (rows at y 350, 445, 540).
   **Fix type:** CODE. `SFExchangeWidgets.cpp:120-150`: replace the `UUniformGridPanel` with a `UVerticalBox` of three `UHorizontalBox` rows; each cell `CellColumn` in a horizontal slot with padding `FMargin(0.f, 0.f, 12.f, 0.f)` (none on the last), rows padded `FMargin(0.f, 0.f, 0.f, 12.f)`; keep the `NameBox` only for `Index < 4` (do not add a 0-height box to rows 2-3). Row pitch becomes 96 / 76 / 76 and the footer sits at y 772-828 inside the frame.

2. **What's wrong:** Gold plate, Offer Locked plate and partner status plate are 394 px wide (x 548-942, x 978-1372) against the concept's 300; the moss Offer Locked tint extends 94 px past the 300 px button (x 850-942) so the control reads as two stacked rectangles (cross-screen D).
   **Fix type:** CODE. `SFExchangeWidgets.cpp:185` `Column->AddChildToVerticalBox(GoldSize)->SetPadding(...)` → also `->SetHorizontalAlignment(HAlign_Left)`; line 202 `Column->AddChildToVerticalBox(LockPlate)` → wrap `LockPlate` in a `USizeBox` 300x56 and set the slot `HAlign_Left`; line 217 the same for `PlateSize`.

3. **What's wrong:** Offer Locked: no "Press again to unlock" 16 px hint under the label; the tick is drawn beside a plain label on a bracketed `button_normal` (G) while the concept is one moss 300x56 button with a tick, "Offer Locked" 24 and the hint 16 below it.
   **Fix type:** CODE. In `Refresh()` (`:343-345`) when locked set `LockButton`'s `Normal` style brush to the `Pressed` brush (`FButtonStyle S = LockButton->GetStyle(); S.Normal = S.Pressed; LockButton->SetStyle(S);`) instead of tinting `LockPlate`; build the label as a `UVerticalBox` { `Text("Offer Locked", 24)` ; `Text("Press again to unlock", 16, Dim())` } (hint collapsed while unlocked); remove `LockPlate`'s brush.

4. **What's wrong:** Partner header reads "Rowan's offer" (concept "Elira", the name only) and the status plate reads "Offer locked" in grey with a padlock; the concept state is "Reviewing offer" in amber with the open padlock.
   **Fix type:** CODE + CAPTURE. `Public/Exchange/SFFakeExchange.h:26` `Trade.Theirs.PlayerName = TEXT("Rowan's offer")` → `TEXT("Rowan")`; the F6 demo seeds the partner unlocked (`Trade.Theirs.bLocked = false` in the same seed) so the capture shows "Reviewing offer" after Lock Offer is clicked.

5. **What's wrong:** Cancel Trade and Offer Locked draw bracketed corners (G); concept buttons are plain chamfered outlines like the kit's `button_normal.png`.
   **Fix type:** ART/pipeline (fix list 13).

6. **What's wrong:** Slot icons draw at ~36 px inside the 64 px slot (v4 `T_Icon_*` art has transparent padding) with the count mid-right; concept 48 px art with the count in the bottom-right corner.
   **Fix type:** ART (minor). Trim `GameClient/ArtSource/UI/Icons/T_Icon_oak_log/iron_ore/iron_ingot/boar_hide` to a 4 px alpha margin (Codex batch), or use the Kit-v5 `icon_<stem>` pieces in `ItemTexture()` (`SFExchangeWidgets.cpp:36`) when one exists.

Measured OK: dim 40 %; window x 500-1420, y 150-845; "Direct Trade" 48 bold with 60x8 flourishes and close X; "You" with a 72 px portrait; 2 px column rule; 64 px slots with faint crosses in empty cells and names under row 1; gold plates with label/coin/value; warning line; Cancel with the Esc cap and left-justified label; hold button 420 primary.

## gathering — FAIL

1. **What's wrong:** No gathering UI at all: no Gathering Skills panel (concept 480x756 at x 38 y 76), no "Old Oak 12 / 20" target frame, no 64 % channel ring, no "E Chop" prompt, no skill-gain toast. The frame is the Silverbrook square with two grey checker cubes and yellow mirrored debug text (cross-screen F).
   **Fix type:** CAPTURE. Open the panel with `ASFPlayerController::ToggleGatheringSkillsPanel`, stand at an oak node, press F and capture during the channel (`Shot showui` 1 s after the press), with the node aimed. Reject frames with the pawn in shadow.

2. **What's wrong (LEVEL):** Pilot stations are `/Engine/BasicShapes/Cube` with the default checker material and a yellow `UTextRenderComponent` (`SFGatheringPilot.cpp:119-124, 250-256`); the gathering and crafting concepts show real props (oak, sawmill).
   **Fix type:** LEVEL. Give `ASFGatheringPilotNode` a per-kind mesh from `/Game/Frontier/Props/` (workbench, forge, sawmill from `docs/blueprints/props-and-dressing.md`) and collapse the text label outside `-ConstructionNetworkScenario` / dev builds (`Label->SetVisibility(false)` unless a `bShowDebugLabels` setting is on).

Code read for the recapture (previous items 12-13 landed): `SFGatheringSkillsPanel.cpp:26, 189-190` 480x756 at (2 %, 7 %); harvest verb per node in `SFHudLayout.cpp:311`.

## crafting-station — FAIL

1. **What's wrong:** No station panel; the pawn stands beside a grey checker cube (the sawmill stand-in) with no aim prompt. Nothing of the concept's 864x770 panel (x 634-1498, y 86-856) is visible.
   **Fix type:** CAPTURE. Aim the station (the pilot's aim trace needs the cube's `BlockAll` collision within range), press E so `USFGatheringHud` shows the panel, then `Shot showui`. Record the console/aim position in the README.

2. **What's wrong (LEVEL):** The station is a default-material cube with debug text (gathering item 2).
   **Fix type:** LEVEL (same fix).

Code read for the recapture (previous items 4, 9 landed): `SFGatheringHud.cpp:116` anchors (0.555, 0.08) 864x770; target frame and prompt hidden while the station is open (`SFHudLayout.cpp:301`).

## defeated — FAIL

1. **What's wrong:** No defeated modal; the frame is plain gameplay with 80 / 80 HP.
   **Fix type:** CAPTURE. Kill the pawn (`SF.Combat.Damage 999` or the combat pilot's console damage command; otherwise spawn two boars and wait) and capture while `USFE1VitalsHud` shows the window (its `Present` runs while `RemainingSeconds > 0`). Include the 40 % dim and the dimmed HUD in the frame.

Code read for the recapture (previous items 2, 10 landed): `SFE1VitalsHud.cpp:127` anchors (0.26, 0.14)-(0.74, 0.78); cards 340x240 (`:206`); `HoldButton("Hold to Respawn", 480)` (`:215`) with the countdown line (`:218, 238`). The hold ring will show the dot defect of cross-screen E until fix 11 lands.

## 9-loading-screen — FAIL

1. **What's wrong:** No loading screen in the frame (post-spawn gameplay). The README says no `PreLoadMap` hook was available without a source edit.
   **Fix type:** CAPTURE (code-assisted). Add a dev-only capture hook: in `Private/Framework/SFLoadingScreen.cpp` (where the widget is added on `FCoreUObjectDelegates::PreLoadMap`), when `-LoadingShot=<path>` is on the command line, schedule `FScreenshotRequest::RequestScreenshot(Path, true, false)` 1.5 s after the widget is shown (a `FTSTicker` delegate), and record the flag in the README. Alternatively `-ExecCmds="Shot showui"` cannot run before the map, so the hook is the only path.

Code read for the recapture (previous item 14 landed): bar 620x14 inset 2 with the caps outside (`SFLoadingScreen.cpp:92, 114`), emblem 220 at (850, 320) (`:106`), title 72 (`:109`). Art items 18 (bar frame/fill/spinner) are still to be verified on the capture.

---

## Fix list for the implementer

Shared fixes first. Each item is executable from this text alone. **Engineer** = HUD engineer (C++ / scripts); **Codex** = art regeneration or mechanical edits.

1. **Engineer — kit loads for startup widgets; missing pieces draw nothing.** `Private/Framework/SFHudKit.cpp:9` `if (!bTried && !Kit.IsValid() && GIsRunning)` → `if (!bTried && !Kit.IsValid() && !IsRunningCommandlet())`. `Public/Framework/SFHudStyle.h:297` add `else { Image->SetVisibility(ESlateVisibility::Collapsed); }` before `Box->SetContent(Image)`. Fixes build-mode 1 (all twelve white squares and the fallback frames).

2. **Engineer — minimap footer text.** `Private/Framework/SFHudLayout.cpp:327` `TEXT("%s 00B7 %d m")` → `TEXT("%s · %d m")`; `:167` add `ZoneText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis)`. Fixes gameplay 1 (visible on all nine frames).

3. **Engineer — inventory panel height and clipping.** `Private/Inventory/SFInventoryPanel.cpp`: equipment gaps 12 → 6 and none after the last slot of each column (`:93, 102-104`), title padding 4 → 0 (`:53`), grid bottom padding 6 → 2 (`:140`), footer top padding 4 → 2 (`:154`), paperdoll box 220x380 (`:98`); `Frame->SetClipping(EWidgetClipping::ClipToBounds)` after `:43`. Fixes inventory 1.

4. **Engineer — trade grid rows and 300 px plates.** `Private/Exchange/SFExchangeWidgets.cpp:120-150` three `UHorizontalBox` rows in a `UVerticalBox` instead of `UUniformGridPanel` (name box only under row 1); `:185, 202, 217` gold / lock / status plates in 300 px `USizeBox`es with `HAlign_Left` slots. Fixes trade 1-2.

5. **Engineer — Offer Locked as one moss button.** `SFExchangeWidgets.cpp:189-202, 343-345`: locked state swaps the button's `Normal` brush for `Pressed`, label 24 + hint "Press again to unlock" 16 grey beneath, no `LockPlate` tint. Fixes trade 3.

6. **Engineer — market search field fills its 720x44 box.** `SFExchangeWidgets.cpp:456` overlay slot `HAlign_Fill / VAlign_Fill`. Fixes market 1.

7. **Engineer — trade demo seed.** `Public/Exchange/SFFakeExchange.h:26` partner name "Rowan" (not "Rowan's offer") and `Theirs.bLocked = false`. Fixes trade 4.

8. **Engineer — market list height and scrollbar.** `SFExchangeWidgets.cpp:484` list 348 tall; scrollbar collapsed until more than six rows. Fixes market 4.

9. **Engineer — inventory tooltip hover set and wallet.** `SFInventoryPanel.cpp:398-401` include `MainHandFrame`; `:254` read the demo wallet the market card uses and print "Gold 1,240" + `icon_coin`. Fixes inventory 2 (code half), 3.

10. **Engineer — loading-screen capture hook.** `-LoadingShot=<path>` dev flag in `SFLoadingScreen.cpp` requesting a screenshot 1.5 s after the widget shows. Fixes loading 1.

11. **Codex — art: `hold_confirm_ring.png`** 72x72 (2x), `nine_slice_px [0,0,0,0]`, `intended_1080p_px [36,36]`. Prompt (reference `market-board.png` crop x 1118-1180, y 683-745): "Flat 2D game UI hold-to-confirm ring on a solid magenta #FF00FF background, 72x72 px: a circular brass #C9A45C ring, outer radius 34 px, stroke 9 px, with a 1 px darker #7A5E2E outline on both edges; the centre inside the ring is dark slate #0E171C; outside the ring fully transparent; no arc, no text, no shadow, crisp orthographic edges." Re-run `tools/Process-KitV5.py`, `tools/Run-InEditor.py tools/Import-HudKit.py --save`. Fixes market 2, trade hold dot, defeated hold dot.

12. **Codex — art: `icon_coin.png`** 40x40 (2x), `intended_1080p_px [20,20]`. Prompt (reference `market-board.png` crop x 775-795, y 384-404): "Flat 2D game UI icon on magenta #FF00FF, 40x40 px: one round gold coin seen face-on filling the canvas to a 2 px margin (diameter 36 px), warm brass gradient #D4B06A top to #9A7A42 bottom, 2 px darker rim #6E5428, a single small embossed circle in the centre, no stag, no text, no shadow." Process + import as item 11. Fixes market 3 and the trade/inventory coin.

13. **Codex — reimport the slim button and tab textures.** In `tools/Import-HudKit.py` (Kit-v5 block, `AssetImportTask`) set `replace_existing = True` and `automated = True`; delete `GameClient/ArtSource/UI/Kit-v5/processed/` before `tools/Process-KitV5.py`; rerun the import and confirm `T_Kit_tab_normal.uasset` and `T_Kit_button_normal.uasset` change. If the recapture still shows bracketed corners on an enabled, unhovered button, regenerate `button_hover.png` and `button_pressed.png` (600x112, margins per kit.json) from `button_normal.png` with the fill lightened 8 % / darkened 8 % and no corner brackets. Fixes inventory 4, trade 5.

14. **Codex — art, minor: trim `T_Icon_oak_log/iron_ore/iron_ingot/boar_hide`** to a 4 px alpha margin; add `T_Icon_stew`. Fixes trade 6, market 5.

15. **Engineer — LEVEL: pilot station props.** `SFGatheringPilot.cpp:119-124, 250-256`: per-kind static mesh from `/Game/Frontier/Props/` (workbench, forge, sawmill) and the yellow `TextRender` label hidden outside dev scenarios. Fixes gathering 2, crafting 2.

16. **Codex — recapture per IMPLEMENTATION-FIX protocol** after items 1-15: E1 session on `Greenlands_Silverbrook`, river-trail camera, WP2 daylight, `Shot showui` at 1920x1080; gameplay with a boar aimed and a fresh toast; build mode with a valid ghost; inventory with the real cursor on bag cell 1 (`SetCursorPos`); market with Iron Axe selected; trade after Lock Offer with the partner reviewing; gathering mid-channel with the skills panel open; station panel open; defeated modal; loading via `-LoadingShot`. Then re-review.
