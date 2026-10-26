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

`BackendServices/Progression/Progression.cs` still counts only Logging + Mining for gathering and has no durable lock/donor protocol. Full six-skill track/global accounting, individual specialization caps and durable preference/donor integration remain production work. This change makes the current service UI honest; it does not implement those systems or alter the canonical design. Parent tracking: `ui-live-client-regressions.md` (root-owned).

## Validation and next step

`git diff --check` passed. Added `ShatteredFrontier.Gathering.Pilot.SkillLockCapability` using the actual WBP: unadmitted fixture hidden; admitted local adapter shown; configured service and ledger modes hidden/disabled; no capability fallback after settings/availability change; server refusal; unbound hidden. Existing `ShatteredFrontier.Gathering.Pilot.Progression` preference checks and progression donor tests remain. Build/targeted automation await the shared UE lease; no runtime pass or visual acceptance claimed.
