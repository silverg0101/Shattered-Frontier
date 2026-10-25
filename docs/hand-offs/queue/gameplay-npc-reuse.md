# Reusable NPC runtime

- **Area/owner:** gameplay / agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-e

## Goal
Preserve six NPCs and support a content-only seventh type/arbitrary approved groups, with shared runtime independent of development fixtures and historical actor naming.

## State
Reviewed profiles/capabilities, zone admission/respawn metadata, unique stable defeat identity, production parity, corpse hook and typed presets are published. Fixture extraction is published as `bc447b02`: 18 exact sandbox specs/lifecycle live in SFCombatFixtures; AI uses explicit default-none fixture flags.
Canonical dedicated corpse metadata is implemented, awaiting independent review. Schema-3 presentation supplies asset-local bounds matching renderer geometry; the hook freezes canonical path and actual component world pose before teardown. Existing ledger metadata and additive Rewards fields retain optional bounds, legacy fingerprints and immutable replay. The source actor builds hit bounds without server render assets and replicates path/scale for client rendering. No database schema/topology or appearance change.

## Evidence
Editor/backend builds passed. Eleven affected checks pass in `npc-dedicated-corpse.log`; its unsupported PIE setup failure and the subsequent GameRPC-without-world-subsystem failure are superseded by PASS `npc-dedicated-corpse-headless-final.log`. The final test uses real dedicated/client net-mode predicates and actual RewardKill/subsystem/actor/OnRep paths: no server mesh, component offset/rotation/nonuniform scale, retry after teardown, one-time scaling and outer client traces before/after mesh resolution.
Ledger domain and ten NPC metadata checks pass. PostgreSQL immediate restart/concurrent pickup and immutable bounds/transform recovery pass at lane `Artifacts/Persistence/WorldLootMetadata-20261025-1538/recovery.txt`. All old stats/behavior/presentation values compare unchanged. Earlier fixture extraction: build + 8 checks passed in `npc-fixture-extraction-final.log`.
Preset evidence truth: original `npc-presets.log` had 12 successes + 1 clock failure, superseded by regression-final; asset-admission 4 checks passed. The loot worker's core native two-client loop passed before this metadata slice; combined live descriptor/click/coupling follow-up stays coordinated with that worker.

## Next step
Review/publish dedicated metadata, then separately rename the actor with old reflected class redirects/source compatibility and old-map load proof. Core loot loop is stable; coordinate temporary fixture/controller refs. Root handles deployment.

## Rules
No appearance work. Preserve authority, XP/quest/slot semantics, stable opaque retry IDs, once-only corpse/direct route and historical wire fields. Targeted checks; independent 6.1-Sol review before push; explicit file commits; no shared-main edits or force/stash/reset.
