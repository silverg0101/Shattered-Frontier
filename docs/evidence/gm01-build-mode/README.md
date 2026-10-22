# GM-01 build-mode review (2026-10-21)

Run: `pwsh tools/Test-BuildModeInput.ps1` -> `-game` on `/Game/Frontier/Maps/Dev/BuildMode/BuildMode_Review` (ASFGameMode + ASFPlayerController), 1920x1080 offscreen. Result: PASS (`result.json`, `build-mode-review.json`, `game.log`).

- Every `IMC_Player` and `IMC_Build` action reached its handler (`INPUT_ACTION` lines in the log, 35 events): Inventory, Skills, EquipAxe/Pickaxe, CycleRecipe, ClaimPlot, Trade, Market, TargetNext (Tab), BuildMode (B), Escape, Shape1-4, Rotate, Line, Plane, LayerUp/Down, Demolish, Door, Place. B and Esc toggled `BUILD_MODE Enabled=1/0`.
- Pawn-polled actions judged by effect: move forward 1309 cm, strafe 619 cm, jump, mouse-look turn 30 deg, wheel zoom in/out, and wheel zoom in build mode (arm 1200 -> 0).
- Key assignments read back from the live subsystem match `tools/Create-PlayerInputAssets.py`; build keys are unmapped before B; LMB leaves `IA_Orbit` in build mode.
- Found and fixed: `IA_TargetNext` (Tab) had no asset, so Tab targeting logged "input action missing". Regenerated `IA_TargetNext` and `IMC_Player`.
- Injection bypasses mapping contexts, so keys are verified by query, not by simulated key presses.
- Screenshots `01`/`02` show the pawn on the flat dev floor; the capture has no palette or HUD, so no UI design is claimed and no concept pair or Fable review applies. A UI review would need an on-screen palette capture.

Other checks: `ShatteredFrontier.Combat.TabTarget`, `.Construction.DoorInteraction`, `.Construction.CursorControls`, `.E1.*` pass; `Test-ConstructionSandboxInput.ps1` and `Test-ConstructionNetwork.ps1` PASS.
