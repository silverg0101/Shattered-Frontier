# Reusable NPC runtime

- **Area/owner:** gameplay / agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-e

## Goal/state
Six preserved profiles, content-only seventh runtime proof, arbitrary approved groups, unique stable defeat IDs, production parity, corpse route and typed presets are reviewed/published. Fixture extraction is `bc447b02`; canonical renderless corpse metadata is `5b1ee29a`.
The newly exposed dedicated ground-source geometry gap is fixed and awaits review BEFORE generic rename. Actual item/fallback mesh bounds are frozen before DropLoot debit, persisted with the existing optional descriptor, and used by the existing headless source/client view. Geometry validation uses the engine/world numeric envelope, not a new balance-size cap. Corpse50cm envelope and old absent-metadata fingerprints/fallback remain unchanged.

## Current evidence
Editor/backend builds, ledger domain and all9 selected Loot/dedicated checks PASS (`ground-loot-geometry.log`). New regression checks actual8 approved meshes, null optional WorldMesh fallback, missing/out-of-world asset rejection without debit RPC, actual oversized/offcenter bounds capture, and real dedicated/client net-mode outer traces under rotation/nonuniform scale. Backend rejects nonfinite/overflow/empty/out-of-world geometry before debit without changing items/revisions. Owned PostgreSQL restart/concurrent pickup PASS with exact oversized/offcenter/nonuniform descriptor recovery: lane `Artifacts/Persistence/GroundLootGeometry-20261025-1640/recovery.txt`.
Prior metadata log had11 successes plus fixture setup failures, superseded by PASS `npc-dedicated-corpse-headless-final.log`; fixture extraction build+8 checks passed. Loot worker independently observed core55s and new actual cursor90.4s loops before this ground fix; do not label center pickup as outer-bound proof.

## Next
Review/publish ground fix. Generic actor rename preparation is safely parked UNTESTED/unreviewed/unpushed as `7fe407fd` on local `codex/npc-rename-compat`; it is NOT part of lane/e ground fix. Resume that separate slice after this gate: preserve reflected redirect/legacy source alias, add actual old map/path load + generic spawn/runtime proof, and coordinate loot callers. Root handles deployment. No appearance work.
