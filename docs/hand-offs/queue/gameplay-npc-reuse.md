# Reusable NPC runtime

- **Area/owner:** gameplay / agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-e

## Goal
Preserve the six current NPCs, support a content-only seventh type and arbitrary approved zone/groups, and separate reusable NPC runtime from development fixtures and historical actor naming.

## State
Reviewed profiles/capabilities, generic zone admission, authored respawn tiers/defensive points, cross-world stable defeat identity, production-map parity and corpse hook are published. Typed behavior/presentation presets are published as `8b5b6797` + `646eafd7`; all 18 prior behavior defaults and old six profile fields are unchanged. Static bodies explicitly consume no animation asset.
Fixture extraction is implemented: `SFCombatFixtures` owns all 18 existing sandbox specs and setup/defeat/respawn lifecycle. Main AI uses explicit default-none idle/aggro-trace flags; group names cannot enable fixture behavior. Await independent review before push.

## Evidence
Editor build and all 8 selected checks pass in shared `npc-fixture-extraction-final.log`: real setup/tag filtering, explicit idle/trace, actual lethal defeat/alternate-point respawn, production services/defense/death, live seventh shot, evade/aggro/assist/dodge and retaliation. Non-UE baseline comparison preserves all 18 map/tag/group/bounds/defensive/flag mappings. The first combined run exposed a package-name collision between tests; both test worlds now restore names automatically and the same selection passes.
Preset evidence: original `npc-presets.log` has 12 successes + 1 fixture-clock failure, superseded by `npc-presets-regression-final.log`; all 4 asset-admission checks pass in `npc-presets-asset-admission.log`. Ten focused .NET reuse checks and bridge build passed. Missing/wrong-type renderer assets reject transactionally; dedicated cooks skip render loads.

## Next step
Review/publish fixture extraction. After the loot worker's actual native two-client loop is stable, separately rename the actor with old reflected class redirects/source compatibility and old-map load proof. Coordinate canonical per-profile dedicated corpse mesh metadata. Keep these remaining acceptance items open; root handles deployment.

## Rules
No appearance work. Preserve service authority, XP/quest and slot semantics, stable opaque defeat/retry IDs, once-only corpse/direct delivery fence and historical wire fields. Targeted checks; independent 6.1-Sol review before push; explicit file commits; no shared-main edits or force/stash/reset.
