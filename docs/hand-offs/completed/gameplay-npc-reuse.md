# Reusable NPC runtime

- **Area/owner:** gameplay / agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-e

## Goal/state
Profiles, content-only seventh live SpawnSlot/shot proof, arbitrary approved groups, stable unique defeat IDs, production parity, corpse delivery, typed behavior/presentation, fixture extraction and dedicated corpse/ground geometry are reviewed/published through `1715e683`.
Separate compatibility rename is implemented: reflected `ASFCombatNpc` in `Combat/SFCombatNpc.h`; old `SFCombatPilot.h` alias and full CoreRedirect retained. Properties, component names, wire/death identity and logs remain compatible. Production/test callers use the generic type; historical threat public names remain source compatible in `SFNpcThreat.h`. No per-species actor classes or appearance changes.

## Current evidence
Editor builds PASS (31.02s and final25.47s). First focused run:21 successes plus one test-harness failure from calling raw editor-constructed soft path without redirect fixup; both actual unchanged old maps loaded successfully. Corrected compatibility regression explicitly applies native soft-path redirect fixup before load; final compatibility-only rerun PASS (`npc-generic-rename-compat-final.log`), superseding that fixture failure. Existing22-test selection includes actual seventh shot, profile/casting, production services/death, fixture flags, corpse/ground dedicated metadata and loot authority/contact paths. Python sandbox authoring and PowerShell caller runners parse; full unpublished-range whitespace check PASS; no owned Unreal process remains.

## Outcome
Complete and independently reviewed/published. No NPC reuse code acceptance remains. The preceding state/evidence describes the implementation checkpoint; final publication is below.

| Published slice | Main commits |
| --- | --- |
| Six preserved profiles; capability-based Mend | `909a0e1d`, `2ce233e1` |
| Generic approved zone groups, unique stable defeat IDs and production services/defense/death parity | `122f8a10`, `4de7c525` |
| Once-only NPC defeat→durable corpse delivery; XP/quest/local fallback preserved | `2dd4605d` |
| Typed shared behavior/presentation presets and fail-closed asset admission | `8b5b6797`, `646eafd7` |
| Explicit development fixture module/policies | `bc447b02` |
| Canonical renderless corpse path/transform/bounds | `5b1ee29a` |
| Authoritative ground geometry before debit; optional model fallback/restart compatibility | `1715e683` |
| Generic actor, old source alias/reflected redirects and unchanged map compatibility | `4a852610` |

Focused evidence in ignored `Artifacts/Orchestration/20261025-resume`: profile validation2PASS; Mend4PASS; integrated zone parity9PASS; corpse hook5PASS; fixture extraction8PASS; dedicated corpse final1PASS; ground geometry9PASS. Preset clock test failure is superseded by `npc-presets-regression-final.log`1PASS; original preset run was12PASS+1FAIL. Fixture package-name collision is superseded by `npc-fixture-extraction-final.log`8PASS. Unsupported PIE/GameRPC metadata fixture setup failures are superseded by `npc-dedicated-corpse-headless-final.log`1PASS. Rename first run was21PASS+1raw-soft-path fixtureFAIL; `npc-generic-rename-compat-final.log`1PASS supersedes it. Earlier22PASS+1FAIL rename count was incorrect and is corrected here.

Actual acceptance includes the isolated content-only seventh profile through real SpawnSlot/ranged damage, a non21 services census, production defense/death/release, stable cross-world defeat identity, no legacy-grant race, dedicated→client outer geometry traces, all8 imported item meshes and unchanged old-class maps/components/tags/source callers. Editor/backend builds and focused PostgreSQL restart/replay/concurrent pickup/no-debit rollback passed; oversized/offcenter/nonuniform geometry recovery evidence is `Artifacts/Persistence/GroundLootGeometry-20261025-1640/recovery.txt` in lane e. Python/PowerShell parser checks and full unpublished-range whitespace passed. No runtime tests rerun for this documentation closure.

Presentation remains the current static placeholder with animation `None`; animated families, rigs and new appearance are not claimed. Loot cursor/body-registration/network closure remains in `queue/gameplay-corpse-ground-loot.md`; runtime release/deployment belongs to its separate release task. This closure does not deploy services or alter those unresolved paths.
