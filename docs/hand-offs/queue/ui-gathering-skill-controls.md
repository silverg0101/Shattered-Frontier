# Match gathering skill controls to the actual progression rules

- **Area:** ui
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-a
- **Branch:** codex/gathering-capability-visibility

## Goal and rule

Expose Raise/Hold/Lower only when the active progression adapter implements them. Skills Mastery v1 sections 1 and 6 specify 7,200 global points, 4,000 gathering-track points and 900 default / 1,000 specialized individual points (display 720.0 / 400.0 / 90.0 / 100.0). Six gathering skills cannot all reach maximum. Lower donates a point only when another skill gains at the shared cap; it never decays automatically. No caps or skill values changed.

## Implemented

`SFGatheringPilot` captures session preference support on server admission from the configured progression mode: both InventoryLedger and Gameplay Services must be disabled. The owner-only replicated capability defaults false and never follows connection health or restore readiness. The panel collapses and disables unsupported controls, including while unbound; request and server endpoints reject unsupported changes. An explicitly admitted local adapter retains its controls with a plain-language tooltip. Owner WBP layout is unchanged.

## Production gap

The frozen 46daae5f preview still hides unsupported service controls. Separate branch `codex/gathering-durable-locks` adds a production candidate: six gathering values/locks and deadlines persist in Progression, an idempotent revision-fenced `ChangeSkillLock` RPC commits choices, and the existing Logging/Mining practice path applies the six-skill track cap and same-track donor rule. The client admits controls only after a complete authoritative service snapshot, then applies acknowledged choices and restores deadlines on reconnect. Other four gathering practice producers and specialization acquisition remain separate production work. Parent tracking: `ui-live-client-regressions.md` (root-owned).

## Validation and next step

Frozen preview: combined Editor build and focused capability, progression and gathering-skills checks passed at `cfb9c752`; logs under lane B `Artifacts/ClientIntegration/20261026/`. Separate durable-lock backend candidate `9711c2c7` passed Progression.Tests, Gameplay.Host Release build, CharacterState durable restart and GameplayHosts ProgressionOnly; the latter passed again after Up/Down/Locked wire coverage. Lane E client Editor build and exact `ShatteredFrontier.Gathering.Pilot.SkillLockCapability` WBP check passed (logs under ignored `Artifacts/GatheringDurableLocks/`). Independent source review found no P1/P2. This candidate is not in the frozen preview or deployed host. Matching packaged service/client network observation and owner visual acceptance remain open.
