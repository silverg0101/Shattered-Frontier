# Dialogue UI r23 review (Fable, independent)

Concept: `docs/Game-Design/concept-art/UI/v5/10-npc-dialogue.png` (1672x941, x1.1483 to 1080p). Live: `r23/dialogue-open.png`, `dialogue-more.png`, `dialogue-accepted.png`, pairs `*-compare.png`. Numbers: `r23/pixel/10-npc-dialogue/report.md`, `r23/ink-measure.txt`, plus my own 1:1 / 2x-4x crops, x-height row profiles and dE2000 samples (concept resized to 1920x1080, LANCZOS). Spec: `docs/blueprints/menus.md` section 1-2, `visual-review.md`, `UI/v5/REVIEW.md` 10-npc-dialogue note. r3/r6/r16 read for their ask lists only; nothing assumed fixed. Backdrop and the mannequin body are out of scope; the board, the speaker's placement, the dimmed HUD and the tracker are judged. Content strings from QST-02/CRF-01 judged by treatment only. Cross-screen: `ui-v5/final3/trade-live.png`, `market-board-live.png`, `inventory-live.png`, `build-mode-live.png`.

Measured (live, 1080p, dialogue-open):
- Board frame, portrait, quest art well, objective rows, XP slot, Accept, Decline: on their concept rects (+-1 px).
- Divider y 433 (concept 429, +4); rows 1-4 and key cap 1 +3/+4 px low; everything else in the left column +-1.
- Type, x-height by row profile (same family and weight as concept): row label 12/12, speech 12/12, summary 10/11, name cap 23/24, title cap 22/23, objective cap 15/16, count 16/17. Accept label ink 74x26 (concept 73x25), Decline 69x17 (71x17). Type passes section 2 everywhere on the board (r16 items 5 and 6 fixed).
- Key cap numeral "1": concept ink 11x22 px, bold, lum 255; live 5x16 px, regular, lum 239.
- Divider: concept brass (131,113,71) lum 117, live (52,53,44) lum 52, dE 25.4. Objectives/Rewards rules: concept lum 138/123, live 67/67.
- Accept fill (184,154,99): dE 8.8 vs concept (173,128,65), dE 5.1 vs canonical brass #A98A55. Decline fill dE 1.4. Board fill dE 2.6, card fill 3.2, row 1 selected 1.5, row 2 unselected 4.4, key cap fill 4.3.
- HUD dim: HP fill, stamina fill, hotbar, minimap ring, tracker title all at 0.59-0.61 of `gameplay-hud-live.png` = the 40 % dim is applied, tracker included. Tracker shows the offered quest in all three frames.
- Speaker (mannequin, player hidden by `SetOwnerNoSee`): head box x 19-175, y 505-608, centre x 5 %, y 52 %. Concept head centre x 21 %, y 27 %; r6/r16 target y 24-34 %.
- `dialogue-more`: four-line speech, divider and rows flow below it (ends y 790 inside the frame), no overlap. `dialogue-accepted`: two rows, Accept disabled "Accepted", Decline unchanged. Row 1 selected on open and after "Tell me more" (r16 items 1, 2 fixed).

## 10-npc-dialogue — FAIL

A player recognises the design: the board, portrait and name, numbered key-cap rows with moss selection, quest card with art well, Objectives/Rewards, Accept/Decline, synced tracker, dimmed HUD. It fails on the speaker standing at the screen edge below the board instead of beside its top, on three kit pieces that do not match at 1:1 (the primary brass plate is pale, the divider and section rules render at half brightness, the key caps carry a small thin numeral), on a truncated objective in the tracker, and on the left column sitting 3-4 px low. Items 1-5 each block on their own; 6 is a section 2 geometry miss.

### 1. Speaker stands at the screen edge, below the board
- **What's wrong:** head centre x 5 %, y 52 % (concept x 21 %, y 27 %). The NPC's head is level with row 1, its left arm reaches x 19, and the figure fills the strip between the screen edge and the board rather than standing beside the board's upper-left corner. `DefaultGame.ini` already holds the r16 values (`DialogueFramingYawDegrees=42.0`, `DialogueFramingPitchDegrees=-4.0`): they do not produce the target.
- **Fix type:** CODE (Engineer).
- **Instruction:** `GameClient/Config/DefaultGame.ini`: `DialogueFramingYawDegrees=27.0`, `DialogueFramingPitchDegrees=10.0` (both inside the `SFE1Settings.cpp` clamps 0..60 / -30..10). If +10 pitch does not lift the head to y 24-34 %, lower the camera instead: in `SFE1Character.cpp` line 346-347 (the `FrameTarget == DialogueFramingTarget` branch) set the framing focus to the target's eye height (`FrameTarget->GetActorLocation() + FVector(0, 0, 60)`) and drop the shoulder offset to 0 for the dialogue case. Acceptance: head centre x 15-25 %, y 24-34 %, nothing of the speaker cut by x 0, and the player's mesh still hidden. Recapture all three frames.

### 2. Accept plate is pale tan, not brass (shared kit piece)
- **What's wrong:** `button_primary_brass` face samples (184,154,99): dE 8.8 against the concept plate (173,128,65) and dE 5.1 against the section 1 brass #A98A55 that `Button(..., Primary)` passes as `Fill`. The 1:1 crop shows a flat tan with soft corners; the concept is a saturated gold-brass plate with a 2 px dark border and 10 px chamfers. The market "Hold to Buy" and trade "Hold to Accept Trade" plates in `ui-v5/final3` are the same tan, so the dialogue is consistent with them and the miss is in the piece.
- **Fix type:** SHARED (Kit-v5 art, Codex; UI v5 session owns the piece).
- **Instruction:** regenerate `GameClient/ArtSource/UI/Kit-v5/button_primary_brass.png` (2x, 600x112, magenta key) from the concept crop x 898-1072 y 668-712 (1672 px) with the `generate.sh` prompt changed to "face #A98A55 at centre, top #B8975C, bottom #8A6C3E, 2 px #7A5F33 border, 10 px chamfered corners, fine lighter inner line, no inner rectangle"; then in `slim/process.py` level the face so a 20x20 sample at the plate centre measures #A98A55 within dE 2. `kit.json` margins unchanged. Reimport (`Process-KitV5.py`, `Import-HudKit.py --save`) and recapture trade, market board and the dialogue.

### 3. Divider and section rules render at half brightness
- **What's wrong:** live divider (52,53,44), lum 52 vs concept 117 (dE 25.4); Objectives/Rewards rules lum 67 vs 138/123. At 1:1 the rules are olive hairlines and the diamond ends nearly vanish; the concept is a 2 px brass engraved rule (family rule 4). Cause: `divider.png` is a 1 px line at alpha 176 in a 24 px tall 2x piece (12 px at 1080p) drawn into an 8 px `SizeBox` (`SFDialogue.cpp` line 159, `Rule()`), so the 0.5 px line is resampled across two rows; `section_rule.png` is also a 1 px line at 2x.
- **Fix type:** SHARED (Kit-v5 art, Codex) + CODE (Engineer).
- **Instruction:** Art: redraw `GameClient/ArtSource/UI/Kit-v5/divider.png` (840x24) and `section_rule.png` (520x16) with a 4 px (2x) rule in #A98A55 at full alpha, diamonds as now; `kit.json` margins unchanged. Code: `SFDialogue.cpp` `Rule()` line 159 `Box->SetHeightOverride(12.f)` (the piece's 1080p height; no resample) and `SectionRule()` line 166 keeps 8. Then move the divider slot to keep y 425-433: see item 6. Acceptance: a row sample on the rule measures within dE 3 of #A98A55 over the board fill.

### 4. Key cap numeral is small and thin; outline is a single rounded hairline
- **What's wrong:** concept "1" ink 11x22 px, bold ivory, inside a 1.5 px brass outline with a faint second outer line and 4 px chamfers. Live `keycap_1..4` (106x108 at 2x, drawn 53 px): numeral 5x16 px regular (6 px short, half the stroke), one thin outline (103,97,64; dE 13.1 vs the concept's outer brass), rounded corners, no second line. Readable at a glance only as a smaller, lighter mark than the concept's.
- **Fix type:** SHARED (Kit-v5 art, Codex).
- **Instruction:** regenerate `GameClient/ArtSource/UI/Kit-v5/keycap_1.png`..`keycap_4.png` (2x, 106x108, magenta key) from the concept crop x 460-508 y 408-456 (1672 px): "flat square key cap, slate #17232A fill, 3 px (2x) brass #A98A55 outline with 8 px (2x) chamfered corners and a 1 px fainter brass line 4 px outside it, no bevel, no gradient; numeral N centred, EB Garamond Bold, ivory #E8E1CE, 44 px tall at 2x (22 px at 1080p)". `kit.json` unchanged; no code change (53 px placement already on the concept rect).

### 5. Tracker truncates the second objective
- **What's wrong:** tracker row 2 reads "Talk to Millwarden... 0 / 1" (ellipsis at x 1826) in all three frames. The same string fits on the board ("Talk to Millwarden Oswin 0 / 1", scaled to fit); the tracker's 20 px label with `ETextOverflowPolicy::Ellipsis` in the 286 px plate does not. Truncated text on screen while the board is open.
- **Fix type:** SHARED (HUD tracker, `SFQuestPilot.cpp`; Engineer).
- **Instruction:** move `DialogueLayout::FitWidth` (`SFDialogue.cpp` lines 214-222) into `SFHudStyle.h` as `SFHudStyle::FitWidth(UTextBlock*, float BasePx, float MaxWidth)` and call it from the dialogue. In `SFQuestPilot.cpp` line 1240-1241 replace the ellipsis policy with `FitWidth(LabelText, 20.f, 286.f - 24.f /*plate pad*/ - 30.f /*checkbox + gap*/ - 52.f /*count*/)` after the count is built, keeping `ClipToBounds`. Title (line 1252) likewise. Acceptance: no ellipsis in the tracker for any `content/quests/*.json` objective; floor 18 px.

### 6. Left column 3-4 px low
- **What's wrong:** divider y 433 (concept 429), rows 1-4 and key caps +3/+4 px (report: divider +4, row 1 +4, rows 2-4 +3, key cap 1 +4). Section 2 tolerance is +-2. The flow's rule padding assumes a 20 px two-line speech block ending at y 393; the live block ends at y 396-397 (line height 23.5 px).
- **Fix type:** CODE (Engineer).
- **Instruction:** `SFDialogue.cpp` line 280: `Flow->AddChildToVerticalBox(Rule(WidgetTree))->SetPadding(FMargin(3.f, 28.f, 3.f, 27.f));`. With item 3's 12 px rule box, use `FMargin(3.f, 26.f, 3.f, 25.f)` so the rule's 2 px line sits at y 429-431 and row 1 at y 460. Acceptance: report divider dy 0 +-2, rows dy 0 +-2.

### 7. Canonical-style flags (section 1 wins over the concept; not failed)
- Unselected rows: concept 1 px dim-brass edge with chamfer notches; live hairline #3A4348, fill (20,32,38) dE 4.4. Section 1 "hairline row (#17232A 60 %, #3A4348)". Same `RowBrush` as the market board rows.
- Selected row and key cap plate: `tab_selected` with 4 px rounded corners, not the concept's chamfered double edge. Same piece as the market board's selected row and the trade "Offer Locked" state.
- Text colour: concept text samples near white; live canonical ivory #E8E1CE. Section 1.
- Window frame: corner swirl ornaments and top finial (`window_frame`), concept has corner knots. Same piece as trade and market board.
- Card fill (15,26,33) vs concept (14,22,23), dE 3.2: `Slate(0.85f)` over the backing picks up blue; same tint on every screen. Does not block.
- Rewards row: item lines "4 Rough Timber" / "2 Iron Nails" at 18 px (scaled to fit, per the content note) under 20 px objective rows; icons 40 px on the 46 px slots; the second pair starts at x 1255 (+17 vs the coin column) because the pair flows. By design; readable, nothing clipped.
- Portrait: the NPC portrait is a wider bust crop (face about 40 % of the well) where the concept is a head crop (about 60 %). Content, not treatment; does not block. If the UI v5 session wants the concept read, re-crop `T_Portrait_Oswin` to the head.
- Disabled "Accepted": tint 0.5 on the tan plate with dark-ink label; reads as disabled and carries the word. Section 1.

### 8. HUD master items (outside the board; SHARED, do not block)
- Tracker title "The Mill's Supply" is bold 24 px with a 6 px ornament; concept regular ~21 px ink, no ornament. Same as r16 item 10; `SFQuestPilot.cpp` line 1251.
- Level badge "1" sits in a plate under the hex; concept has the numeral in the lower lobe. Master HUD piece.

## Cross-screen consistency

Rows (selected = `tab_selected` moss + brass, unselected = hairline), Decline (`button_normal`, 54 tall, 24 px label), frame (`window_frame` with finial) and the type roles match trade, market board, inventory and build mode. Accept uses the same `button_primary_brass` plate as the market and trade primaries, so item 2 is shared and fixing it changes those screens; the `final3` trade and market captures still show 32 px button labels ("Cancel Trade", "Hold to Buy") where the dialogue now shows 24 px: that is the shared `Button` fix landing after those captures were taken (stale captures, not a dialogue difference). No control differs between the dialogue and those screens.

## Fix list

1. SHARED (Codex): regenerate `button_primary_brass.png` to a #A98A55 face with 2 px #7A5F33 border and 10 px chamfers; level the face in `process.py`; reimport; recapture dialogue, trade, market (item 2).
2. SHARED (Codex): redraw `divider.png` and `section_rule.png` with a 4 px (2x) full-alpha #A98A55 rule (item 3).
3. SHARED (Codex): regenerate `keycap_1..4.png` with a 44 px (2x) bold ivory numeral, 3 px brass chamfered outline and faint outer line (item 4).
4. SHARED (Engineer): `FitWidth` into `SFHudStyle.h`; tracker labels and title in `SFQuestPilot.cpp` 1240-1252 scale to fit instead of ellipsis (item 5).
5. Engineer: `DefaultGame.ini` `DialogueFramingYawDegrees=27.0`, `DialogueFramingPitchDegrees=10.0`, or eye-height focus in `SFE1Character.cpp` 346-347; head centre x 15-25 %, y 24-34 % (item 1).
6. Engineer: `SFDialogue.cpp` line 159 `SetHeightOverride(12.f)` for `Rule()`; line 280 rule padding `FMargin(3.f, 26.f, 3.f, 25.f)` (items 3, 6).
7. Engineer: recapture r24 (open, more, accepted), rerun `tools/ui_pixel_diff.py 10-npc-dialogue` and ink measure; new reviewer.
