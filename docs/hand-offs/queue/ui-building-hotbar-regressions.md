# Building catalogue and hotbar regression

- **Area:** ui
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-c (`pwsh tools/Enter-Lane.ps1 -Name c`)

## Goal

Restore construction item previews/icons in the build catalogue and construction hotbar slots, and restore drag/drop between catalogue, hotbar, inventory, and world according to existing design semantics.

## State

Root cause found and code fix implemented. Catalogue cells had no shortcut drag payload; hotbar drop accepted only item definitions. Build icons and the HUD kit were loaded via string paths but their content roots were not always cooked. Hotbar cancellation also cleared a shortcut on Escape/focus/rejected UI drop, conflating cancellation with deliberate world removal. Unreal automation and packaged visual review remain pending the shared Unreal job lease.

## Evidence

User-provided packaged-client screenshot: `C:\Users\grego\AppData\Local\Temp\codex-clipboard-d0b3a4b0-fcf9-430d-a2a8-697ca812dadf.png`. Source assets exist at `GameClient/Content/Frontier/UI/Kit/T_Kit_icon_{timber_floor,timber_wall,wooden_door,timber_roof}.uasset` and `UI/Icons/T_Icon_build_{foundation,wall,door,roof}.uasset`; those roots were missing from `DefaultGame.ini`. The root separately checked player soft-load roots; they are not added globally because Server denies the required MetaHuman/Mutable plugins. `pwsh tools/Test-BuildHotbarAssets.ps1` passes for UI cook roots, source art and all 9 allowed face-class packages matching the verified generated-face manifest; this is source closure evidence, not a cooked-package check. Added `ShatteredFrontier.Hud.BuildHotbar.DragAndAssets` for icon resolution and catalog/inventory/hotbar/world-drop semantics; it has not been run. Lane C was clean and 46 commits behind; `tools/Enter-Lane.ps1 -Name c` moved it to `613c4e73`.

## Blocked on

Root's shared Unreal job lease for editor automation, package verification and visual capture.

## Next step

Acquire the shared Unreal job lease, run `ShatteredFrontier.Hud.BuildHotbar.DragAndAssets`, fix any failures, then capture the packaged client and obtain the required independent visual review and owner signoff.

## Rules for this task

Preserve ordinary inventory drop behavior. Hotbar drag changes/removes shortcuts and must not silently destroy equipped items. Coordinate before touching other regions of `SFHudWidgets.cpp`; avoid camera/input and combat HUD target regions owned by parallel tasks. Do not run Unreal-heavy work until the root grants the shared lease. Visual verification requires a fresh Sol reviewer and owner signoff.

## Outcome

Pending.
