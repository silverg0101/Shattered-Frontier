# Reusable NPC profiles

- **Area:** gameplay
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-e

## Goal
Preserve six NPCs while moving species inputs into authored profiles; prove a content-only seventh ranged type and remove per-type spawn branches.

## State
Profile slice implemented: exact six stat records, canonical loot-rule bindings, transactional bounded loader, unknown-ID actor rejection, generic ranged/Mend dispatch. Algorithms, service authority and death IDs preserved. Generic map/group loading, authored respawn tiers and defensive points, plus actual seventh SpawnSlot/shot and unique cross-world defeat identity are implemented and pass 27 focused editor checks. Typed referenced behavior/presentation presets are implemented: all six retain one exact current default; real AI/reach/practice and body/collision/label consumers resolve per profile. Static-body animation is explicitly none. Fixture extraction and compatibility rename remain. The reviewed corpse reward hook is published as `2dd4605d`.

## Evidence
Preset slice: Editor build passed. Twelve existing affected checks pass in `npc-presets.log`; new actual perception/chase/leash/reach/assist/body-reference regression passes in `npc-presets-regression-final.log` after correcting test-clock stepping. Ten focused .NET reuse checks and WorldContent.Tool build pass; parsed comparison confirms all 18 old behavior defaults and all old six profile fields unchanged. Schema 2 rejects old semantics.
Corpse hook: Editor build and all 5 selected checks pass (`npc-corpse-hook-final.log`): actual RewardKill personal/shared delivery, stable opaque retry after actor destruction, frozen eligible raid snapshot, independent once-only XP/quest credit, no legacy grant, protected exclusion and service-disabled local bag delivery. Existing four Loot checks also pass. Combined live two-client death/UI loop remains with the loot worker after hook review.
Zone parity review fixes: editor build and 9 affected tests pass in shared `npc-zone-parity-integration-final.log`; real one-slot services callback, rejected malformed replies, front dodge/parry/rear hit, real published interrupt/resource cost, production death deadline, durable release/acceptance and true sandbox controls all exercised.
Zone slice two-client services rechecks passed: `Artifacts/CombatPilot/network-20261025-134633-135` (archer) and `network-20261025-134905-904` (ability).
Generic zone slice: 9 focused .NET reuse checks, existing --combat checks, zone generator/WorldContent.Tool validation and Gameplay.Host build pass. Editor build and 27 focused checks pass in shared `npc-zone-runtime.log`, including actual seventh ranged shot at 1200 cm and two-world defeat-ID retry stability.
Casting review fix: editor build and four affected checks passed (`npc-mend-regression-final.log` in shared scratch); actual `npc/mend_reuse_fixture` pays 6 mana and completes an 8-HP heal, while the identical profile without Mend is rejected without spending/healing.
Editor Development build passed. 24 focused Pilot/Resolution/OutOfPlaceNpc/NpcCasting/Death.ServicesClient tests passed; new profile tests passed again with actor unknown-ID rejection. Two-client services archer and ability netchecks passed: lane `Artifacts/CombatPilot/network-20261025-130419-193` and `network-20261025-130524-272`. Logs/report: shared `Artifacts/Orchestration/20261025-resume/npc-refactor-report.md`. Baseline comparison verifies all 54 stat values and six loot-rule identities. Seventh isolated test content now also drives a live SpawnSlot/shot regression.

## Blocked on
Profile/casting review passed and commits 909a0e1d + 2ce233e1 are on main. Zone/parity independent recheck passed and both slices are published as `122f8a10` + `4de7c525`. Corpse reward integration passed review and is published as `2dd4605d`; loot worker owns the actual native two-client workflow gate.

## Next step
Review/publish the typed preset slice, then extract fixture setup and perform a separate compatibility-safe generic rename after the loot fixture is stable. Coordinate canonical dedicated-server corpse mesh metadata with the loot worker.

## Rules for this task
Preserve authority, durable death IDs, six profiles and balance. Coordinate Unreal slot. This lane owns SFCombatPilot integration. Generic rename is a separate compatibility-safe commit.

## Outcome
Profile slice tested; full reuse goal remains open.
