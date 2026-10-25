# Reusable NPC profiles

- **Area:** gameplay
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-e

## Goal
Preserve six NPCs while moving species inputs into authored profiles; prove a content-only seventh ranged type and remove per-type spawn branches.

## State
Profile slice implemented: exact six stat records, canonical loot-rule bindings, transactional bounded loader, unknown-ID actor rejection, generic ranged/Mend dispatch. Algorithms, service authority and death IDs preserved. Zone/group loading, fixture extraction, live seventh-type proof and compatibility rename remain.

## Evidence
Casting review fix: editor build and four affected checks passed (`npc-mend-regression-final.log` in shared scratch); actual `npc/mend_reuse_fixture` pays 6 mana and completes an 8-HP heal, while the identical profile without Mend is rejected without spending/healing.
Editor Development build passed. 24 focused Pilot/Resolution/OutOfPlaceNpc/NpcCasting/Death.ServicesClient tests passed; new profile tests passed again with actor unknown-ID rejection. Two-client services archer and ability netchecks passed: lane `Artifacts/CombatPilot/network-20261025-130419-193` and `network-20261025-130524-272`. Logs/report: shared `Artifacts/Orchestration/20261025-resume/npc-refactor-report.md`. Baseline comparison verifies all 54 stat values and six loot-rule identities. Seventh isolated test content selects ranged capability and resolves shared hit rules; live seventh actor remains unverified.

## Blocked on
Independent 6.1-Sol recheck before push: review found a remaining species check in casting admission; fixed to CanCastMend with actual non-captain actor casting regression. Corpse hook awaits loot_workflow implementation commit.

## Next step
Review profile slice, then generalize zone/group loading to prove the seventh type spawns and fires without C++ species branches.

## Rules for this task
Preserve authority, durable death IDs, six profiles and balance. Coordinate Unreal slot. This lane owns SFCombatPilot integration. Generic rename is a separate compatibility-safe commit.

## Outcome
Profile slice tested; full reuse goal remains open.
