# Camera and mouse controls

Goal: correct near-target combat framing and expose responsive, persistent local mouse controls in Settings > Controls.

Scope: lane A; `SFGameUserSettings`, `SFGraphicsMenu`, `SFE1Character::TickCameraControls`, `SFCombatFramingLogic` and focused tests. Existing WBP row/footer layout is retained. Combat feedback, target HUD and defeat presentation belong to the separate lane-D task.

Cause: combat assist aims from the elevated spring-arm pivot rather than the rendered camera, over-tilting at close ground-level targets. Mouse input is raw per-frame counts (no erroneous dt factor), but the Controls tab is inert and its release-only 0.25 degrees/count cannot be adjusted by the player.

Controls: horizontal/vertical integer percent of release mouse gain; default 200%, range 25–800%, step 25%; invert vertical mouse; existing framing/DoF/shoulder preferences. Local player permission, local GameUserSettings persistence, next-frame effect after Apply. Reset stages shipped defaults; reopening discards unapplied edits. Keyboard/stick rates, zoom and authority remain independent.

Evidence: Editor build72.54s PASS; all5 exact checks PASS (`ShatteredFrontier.Camera.` plus `ShatteredFrontier.Hud.Menus.Settings`). Local evidence `Artifacts/CameraMouse/20261026-controls`: `focused-tests.log`, `capture-evidence.log`, `settings-controls-default00000.png`, `settings-controls-applied-40000000.png`. Actual canonical stepper→Apply updated horizontal400%, then `LoadSettings(true)` retained400. No WBP asset/layout regeneration. Frozen ff59 archives are untouched; the two explicitly owned safe-gate clients were verified and closed for the sole UE lease.

Open: camera visual capture. Initial adapters failed on Python reflected names; corrected Settings capture succeeds. Standalone sandbox target-selection adapter still leaves `CombatTarget=None`, so its zero-shoulder camera images are excluded from acceptance. The component/geometry regression passes, but actual mouse tracking at two settings and selected-target framing still need native/injected-input observation. UE lease released; no owned Editor/game process remains. Next: fresh Sol source/Settings review, then coordinate camera capture using real target/input; owner sign-off remains required (maximum three review rounds). No deployment implied.
