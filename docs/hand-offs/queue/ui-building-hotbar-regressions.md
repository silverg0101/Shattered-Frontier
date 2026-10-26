# Building catalogue and hotbar regression

- **Area:** ui
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-b (integrated)

## Goal

Restore construction item previews/icons in the build catalogue and construction hotbar slots, and restore drag/drop between catalogue, hotbar, inventory, and world according to existing design semantics.

## State

Root cause fixed and integrated. Catalogue cells now carry shortcut drag payloads; hotbar accepts construction or bag item definitions, moves/swaps shortcuts, and clears only on deliberate world release. Escape/focus/rejected UI drops retain the binding. UI Kit and Icons cook roots are explicit. Static checks and `ShatteredFrontier.Hud.BuildHotbar.DragAndAssets` passed in combined integration. Packaged asset presence, native catalogue/bag/slot/world drag and visual review remain open.

## Evidence

User-provided packaged-client screenshot: `C:\Users\grego\AppData\Local\Temp\codex-clipboard-d0b3a4b0-fcf9-430d-a2a8-697ca812dadf.png`. Source assets and UI cook roots are present. `pwsh tools/Test-BuildHotbarAssets.ps1` and the focused WBP regression passed; logs under lane B `Artifacts/ClientIntegration/20261026/`. Source closure does not establish actual cooked-package presence or native drag behavior.

## Blocked on

Matching package verification, fresh visual review and owner sign-off.

## Next step

Inspect the matching client package for build icons/kit assets, run native catalogue/bag/hotbar/world interaction, then capture and independently review the packaged views before owner sign-off.

## Rules for this task

Preserve ordinary inventory drop behavior. Hotbar drag changes/removes shortcuts and must not silently destroy equipped items. Visual verification requires a fresh Sol reviewer and owner signoff.

## Outcome

Pending.
