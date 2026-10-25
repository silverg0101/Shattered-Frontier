# Reusable NPC profiles

- **Area:** gameplay
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-e

## Goal
Preserve six NPCs while moving species inputs into authored profiles; prove a content-only seventh ranged type and remove per-type spawn branches.

## State
Profile slice implemented: exact six stat records, canonical loot-rule bindings, transactional bounded loader, unknown-ID actor rejection, generic ranged/Mend dispatch. Algorithms, service authority and death IDs preserved. Generic map/group loading, authored respawn tiers and defensive points, plus actual seventh SpawnSlot/shot and unique cross-world defeat identity are implemented and pass 27 focused editor checks. Fixture extraction and compatibility rename remain.

## Evidence
Zone slice two-client services rechecks passed: `Artifacts/CombatPilot/network-20261025-134633-135` (archer) and `network-20261025-134905-904` (ability).
Generic zone slice: 9 focused .NET reuse checks, existing --combat checks, zone generator/WorldContent.Tool validation and Gameplay.Host build pass. Editor build and 27 focused checks pass in shared `npc-zone-runtime.log`, including actual seventh ranged shot at 1200 cm and two-world defeat-ID retry stability.
Casting review fix: editor build and four affected checks passed (`npc-mend-regression-final.log` in shared scratch); actual `npc/mend_reuse_fixture` pays 6 mana and completes an 8-HP heal, while the identical profile without Mend is rejected without spending/healing.
Editor Development build passed. 24 focused Pilot/Resolution/OutOfPlaceNpc/NpcCasting/Death.ServicesClient tests passed; new profile tests passed again with actor unknown-ID rejection. Two-client services archer and ability netchecks passed: lane `Artifacts/CombatPilot/network-20261025-130419-193` and `network-20261025-130524-272`. Logs/report: shared `Artifacts/Orchestration/20261025-resume/npc-refactor-report.md`. Baseline comparison verifies all 54 stat values and six loot-rule identities. Seventh isolated test content selects ranged capability and resolves shared hit rules; live seventh actor remains unverified.

## Blocked on
Profile/casting review passed and commits 909a0e1d + 2ce233e1 are on main. The zone/group slice passes editor and two-client checks and awaits fresh review. Corpse hook awaits loot_workflow implementation commit.

## Next step
Obtain fresh independent review of generic zone/group plus defeat identity, then push and continue fixture extraction/rename/integration.

## Rules for this task
Preserve authority, durable death IDs, six profiles and balance. Coordinate Unreal slot. This lane owns SFCombatPilot integration. Generic rename is a separate compatibility-safe commit.

## Outcome
Profile slice tested; full reuse goal remains open.
