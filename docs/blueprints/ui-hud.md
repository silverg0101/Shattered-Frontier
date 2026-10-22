# UI and HUD blueprint

Scope: the Kit-v5 art kit, nine-slice import, typography, layout, widget code (`SFHudStyle`), the loading screen, and the UI capture protocol. Depth: `docs/Game-Design/concept-art/UI/v5/REVIEW.md` (family rules 1-12), `IMPLEMENTATION-FIX.md` (per-screen tables, G1-G5), `docs/evidence/ui-v5/final/REVIEW-fable.md` (fix list 1-21). Shared rules: [README.md](README.md). Status: `final2/` recapture done after fixes 1-19; its Fable review is pending.

## 1. Pipeline (in order)
| # | Step | Command / file | Done when |
|---|---|---|---|
| 1 | Concept | `docs/Game-Design/concept-art/UI/v5/<screen>.png` (1672x941; design px = x1.148 to 1080p). `gameplay-hud.png` is the master for every persistent element | approved in `REVIEW.md` |
| 2 | Art pieces (Codex) | magenta `#FF00FF` key, **2x** size, one piece per PNG, prompt names the concept crop (box in 1672x941 px); sources under `GameClient/ArtSource/UI/Kit-v5/` (`structural-source.png` atlas, `slim/source/<piece>.png`) | `kit.json` row: `file`, `nine_slice_px` [l,t,r,b] (2x px), `intended_1080p_px` [w,h] |
| 3 | Process | `py -3 GameClient/ArtSource/UI/Kit-v5/build_structures.py`; `py -3 .../slim/process.py`; `py -3 .../split_bars.py`; `py -3 .../preview.py --contact-only`; `py -3 .../scan_borders.py` | contact sheet matches; scan_borders prints no failures (no key speckles in the outer 6 px) |
| 4 | Resample to 1x | `py tools/Process-KitV5.py` (188 pieces -> `Kit-v5/processed/`, LANCZOS to `intended_1080p_px`) | processed folder complete |
| 5 | Import (open editor) | `py -3 tools/Run-InEditor.py tools/Import-HudKit.py --save` -> `/Game/Frontier/UI/Kit/T_Kit_<name>`, `DA_HudKit`, fonts `/Game/Frontier/UI/Fonts/FontFace_{Cinzel,EBGaramondRegular,EBGaramondBold,SourceSansRegular,SourceSansBold}` (static OFL TTFs: variable fonts render as tofu) | import log success; `margin = nine_slice_px/2/texture_1x_size`, `draw_scale` 1.0 |
| 6 | Loading screen | `GameClient/ArtSource/UI/LoadingScreen-v5/generate.sh` (Codex), `tools/Process-HudKit.ps1 -Kit GameClient/ArtSource/UI/LoadingScreen-v5`, `py -3 tools/Run-InEditor.py GameClient/ArtSource/UI/LoadingScreen-v5/Import-LoadingScreen-v5.py --save` | `T_LS_*` imported |
| 7 | Widgets | C++ under `GameClient/Source/ShatteredFrontier` via `SFHudStyle` helpers only (§3); `/build-editor` with the editor closed | builds clean (C4458 shadowing is an error) |
| 8 | Capture | §4 protocol | `<screen>-live.png` 1920x1080 + `<screen>-compare.png` |
| 9 | Review | fresh Fable, per-screen PASS/FAIL, fix list marked Engineer/Codex | all PASS |

## 2. Kit facts and nine-slice (G1/G2)
- Slate draws box corners at the texture's real pixels (`ElementBatcher.cpp` corner = `ActualSize * Margin`); `ImageSize`/`DrawScale` do not scale corners. **Only a 1x texture draws at design size.** Keep the 2x PNGs as source for a 4K kit.
- Margins must cover the baked ornament (2x px): `panel_frame` 36,140,36,140; `window_frame` 36,176,36,176; `modal_frame` 36,206,36,206; `tooltip_frame` 36,86,36,86; `detail_card_frame` 36,142,36,142; `quest_tracker_frame` 36,76 (slim: 24,24,24,24); `target_frame` 36,44; `toast_frame` 68,28,36,28; `button_normal/hover/pressed/disabled/destructive` 40,40,40,40; `button_primary_brass` 56,48,56,48; `tab_*` 36,30; `search_field` 36,26; `dropdown_field` 36,28; `action_bar` 52,44; `slot_hotbar_*` 44; `slot_catalogue_*` 50; `slot_bag_*` 32; `slot_equipment_*` 36; `minimap_footer` 36,22. Rule: a box brush is never drawn smaller than the sum of its margins; widgets size with `USizeBox`, never `Fill` on a kit brush.
- Fixed-size pieces (`hex_level_badge`, `minimap_ring`, `keycap_*`, `checkbox`, `close_x`, `status_tick/cross`, `hold_confirm_ring`, `icon_*`, `portrait_well`, `compass_*`) draw as `Image` via `SFScreenArt::KitImage`, never `Box`. Scene art (`paperdoll_*`, `ghost_*`) and icons keep 2x and are drawn at a given size.
- Bars: stack `bar_<x>_back`, `_fill`, `_frame` (3-slice, `fill_inset_rect_px` [42,13,718,51] of the 760x64 source); `MakeFramedBar(name, 380)`.
- Frame fill #111C23 at 84 % alpha is the art as specified (rule 3: 85 %), not a bug.

## 3. Family rules (measured at 1080p; anchors in %, sizes in px, DPI curve by height)
| Rule | Value |
|---|---|
| Master HUD | `gameplay-hud.png` owns vitals, hotbar, prompt bar, target frame, minimap, tracker, toasts; other screens never redraw them, they dim them 40 % black (`USFMarketBoard` had 55 %) |
| Safe inset | 2 % (38 px); nothing touches an edge |
| Colours | slate #17232A 85 %; brass #A98A55; ivory #E8E1CE; moss #688267 (state only); crimson #A34842 (health, danger); amber #B69A55 (stamina, warnings); never colour alone, every state has a glyph or word |
| Frames | 2 px brass 9-slice, 10 px chamfer notches, 24 px corner overlays, 2 px engraved divider, radius 0 |
| Type | EB Garamond Bold titles 48 window/modal, 36 panel, 26 section (Cinzel only for the loading zone title and "Defeated"); EB Garamond 24 buttons, 20 body, 18 rows/footers, 16 captions/toasts; Source Sans Bold 18-20 numerals; **minimum 16 px** |
| HUD geometry | badge 116 at x2 %, bottom y96 %; bars 380x32, 10 px apart, numerals centred; prompt bar 52 tall, 36 px key cap, bottom y85 %; target frame 410x70 top-centre; minimap 250 px round, right x99 % top y2 %, footer 180x32 "Place . 000 m"; hotbar 6x88, 12 px gap, bottom y96 %, number top-left / count bottom-right 18; tracker and toasts 280 wide at x84-99 %, toast 66 tall, 12 px gap, max 3 |
| Buttons | `SFHudStyle::Button(Tree, Label, Kind, MinWidth)`: 56 tall, min 160, pad 24, label 24; Primary = filled brass, Secondary = outline, Destructive = crimson outline (never crimson fill); `HoldButton` = Primary + 36 px `hold_confirm_ring` at x 20 inside the left end, label "Hold to ...", moss arc via `PaintHoldArc`; nothing else uses a ring; nothing under 44 px is a text button |
| Tabs / slots | tabs 44 tall, selected = moss tint + brass bottom rule, LB/RB caps 40x26 on pad; slots 64 bag / 72 equipment / 88 hotbar / 100 catalogue, 12 px gaps (bag grid 2 px); selected tile = brass edge, selected row/card = moss edge |
| Panels | left column x2-27 %; inventory x4-34 % y3-85 %; station x30-82 %; centre window x26-74 % **y14-78 %**; modal x12-88 % y16-87 % (market as approved x13-87 % y10-85 %); every window has `close_x` 40 top-right and answers Esc/B; modals collapse the side panels |
| Have/need | sans 18 "have / need", tick/cross at row end, 2 px crimson underline under the short number |

## 4. Capture protocol (`IMPLEMENTATION-FIX.md` §Capture protocol)
1. E1 session only: `UnrealEditor.exe GameClient/ShatteredFrontier.uproject /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -windowed -ResX=1920 -ResY=1080 -ForceRes -nosound -nop4 <pilot flags> -UserDir=<isolated dir> "-ExecCmds=py tools/Capture-TimedShots.py <out folder> [seconds] [interval]"`. Never the construction sandbox or network client, never a mannequin on the test plot; if build mode cannot be entered from the E1 controller, report **blocked**.
2. Shots use the console line **`Shot showui`** (HighResShot omits UMG); viewport 1920x1080, DPI 1; never an OS screenshot of the window.
3. Scene: WP2 daylight on the river trail facing the bridge and mill, player lit front-left; reject deep shadow or the flat test plot.
4. Per screen: gameplay = boar or node aimed (target frame) + one harvest toast + tracker; inventory = press I, hover the main-hand axe; market = F7, Iron Axe row selected; trade = F6, my offer locked, partner "Reviewing offer"; build = B from the E1 controller.
5. `<screen>-live.png` (JPEG q2 if > 3 MB) + `<screen>-compare.png` (concept left, live right); README records the exact command and console lines, and which captures are stale.

## 5. Known failure -> rule
| Failure | Rule |
|---|---|
| 90 px smeared corners, 44 px buttons that are corner-only, hotbar studs at 2x | import at 1x (`Process-KitV5.py`) and margins of §2; never `DrawScale` as a fix |
| Labels spill past 100 px tiles and 80 px rail tiles | `KitPanel(..., bInset=false)` on `slot_catalogue_*`/`tab_*`; `Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis)` (UE 5.8 name) as the guard |
| Bag-slot piece as a table row / text field; tooltip frame as a prompt bar; window frames as buttons | one piece per role (§3); frames are never button backgrounds |
| Three ad-hoc hold controls (5 px strip, own ring + disc) | `SFHudStyle::HoldButton` everywhere |
| Inventory visible under a modal | `ToggleTradeDemo` / `ToggleMarketDemo` collapse `InventoryPanel` and build mode first |
| Cinzel caps at 24-28 px titles, 11-15 px body | §3 type roles, 16 px minimum |
| Procedural slate boxes as HUD | every surface is a kit brush + OFL font; procedural boxes are not acceptable |
| Capture from the sandbox / OS window / HighResShot without UI | §4 |
| Dev strings in player UI ("Target -1, 9, 0 | Click or B...") | pass only "Valid placement"/"Invalid placement"; debug goes to `UE_LOG` |
| Kit piece with corner swashes the concept does not have (quest tracker) | regenerate from the concept crop with the prompt in the review (plain 2 px brass rectangle, 10 px chamfers, inner hairline) |
| Chroma-key speckles on piece edges | `scan_borders.py` gate |

## 6. Concept refs
`docs/Game-Design/concept-art/UI/v5/*.png` (16 approved screens; `gameplay-hud.png` master; `kit-v5-contact.png`, `kit-v5-hud-mock.png` previews), `concept-art/UI/HUD-v1/` (kit look origin).

## Controls and acceptance

Button, tab, slot, row and selection specs, the pixel-perfect acceptance rule and the new-menu recipe live in [menus.md](menus.md) (shared by the HUD and every menu).
