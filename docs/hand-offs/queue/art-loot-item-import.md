# Import the 8 loot item models into Unreal

- **Area:** art
- **Owner of the next step:** independent reviewer, then owner
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-b (`pwsh tools/Enter-Lane.ps1 -Name b`)

## Goal

The 8 Forge-built loot items show their 3D models in game at real size, linked to their item records.

## State

Owner approved the eight reviewed Forge sources for import on 2026-10-25. All eight source GLBs are preserved under `GameClient/ArtSource/LootItems/Approved/`; imported meshes and materials are under `/Game/Frontier/Gameplay/LootItems/` and linked from their native `DA_*` definitions through the optional `WorldMesh` field. `SB_LootModels` is a development display with resting items and a 10 cm cube per item; the one bracer is mirrored only as a second actor. Round 1 independent review failed all eight, round 2 passed six and failed the captain's longsword and wolf pelt. The final round-3 captures are frozen for independent review; owner acceptance remains open. Existing inventory/equipment consumers use icons; gameplay 3D drop/equip visibility is not implemented by this import. The 5 blender-garment entries are not started.

## Evidence

- Models and reviews: `C:\Projects\Shattered-Frontier\Artifacts\LootItems\` (git-ignored, moved out of lane b) (REVIEW-round1..3.md, FINAL-signoff-sheet.jpg). Final files: round-1 items in `<name>\`, round-2 items in `<name>\round2\` (brigand_blade_v3), round-3 items in `<name>\round3\` (captains_longsword_s222.glb).
- Item list and sizes: `content/items/forge-handoff.json` (9dafb6ed, 449ee18a).
- Item records: SFItemDefinition assets from ee22f632.
- Shared ignored `Artifacts/LootItems/in-engine/round{1,2,3}/` has immutable 1920×1080 captures, concept/live pairs and manifests. Round 3 adds source GLB/unlit diagnostics and close-ups; `diagnostics.md` states their lighting limits and the remaining captain/pelt defects. Exact 2048×2048 source/import pixels match for all 16 textures. Editor build and persisted `LOOT_CHECK_PASS 8` passed; every fallback mesh has 48,460–50,000 triangles. Independent reviews: `Artifacts/Orchestration/20261025-resume/loot-import-review-frozen-round1.md` and `loot-import-review-round2.md`. Width/thickness owner question: [LOOT-01](../../decisions/LOOT-01-reviewed-model-size-targets.md).

## Blocked on

Final in-engine round-3 Opus review and owner sign-off. Product drop/equip 3D consumers are a separate gameplay task. LOOT-01 size question is for the owner.

## Next step

Use the frozen round-3 evidence for the last independent review, then hand any remaining visual defects and LOOT-01 to the owner. The three-round limit precludes a fourth independent fix/review cycle. Preserve all source GLBs and frozen images. Ask separately for an authorized gameplay implementation task if actual drop/equip 3D presentation is required.

## Rules for this task

- Never patch meshes in Blender; scale and mirror at import only.
- Only write under `/Game/Frontier/`.

## Outcome

