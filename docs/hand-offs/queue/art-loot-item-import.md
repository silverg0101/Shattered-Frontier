# Import the 8 loot item models into Unreal

- **Area:** art
- **Owner of the next step:** independent reviewer, then owner
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-b (`pwsh tools/Enter-Lane.ps1 -Name b`)

## Goal

The 8 Forge-built loot items show their 3D models in game at real size, linked to their item records.

## State

Owner approved the eight reviewed Forge sources for import on 2026-10-25. All eight source GLBs are preserved under `GameClient/ArtSource/LootItems/Approved/`; imported meshes and materials are under `/Game/Frontier/Gameplay/LootItems/` and linked from their native `DA_*` definitions through the optional `WorldMesh` field. `SB_LootModels` is a development display with a 10 cm cube per item; the one bracer is mirrored only as a second actor. In-engine visual review and final owner acceptance remain open. Existing inventory/equipment consumers use icons; gameplay 3D drop/equip visibility is not implemented by this import. The 5 blender-garment entries are not started.

## Evidence

- Models and reviews: `C:\Projects\Shattered-Frontier\Artifacts\LootItems\` (git-ignored, moved out of lane b) (REVIEW-round1..3.md, FINAL-signoff-sheet.jpg). Final files: round-1 items in `<name>\`, round-2 items in `<name>\round2\` (brigand_blade_v3), round-3 items in `<name>\round3\` (captains_longsword_s222.glb).
- Item list and sizes: `content/items/forge-handoff.json` (9dafb6ed, 449ee18a).
- Item records: SFItemDefinition assets from ee22f632.
- `Artifacts/LootItems/import-evidence.json` gives source-hash-backed principal length and imported bounds. `Artifacts/LootItems/in-engine/round1/` has eight 1920x1080 client captures and eight concept/live pairs. Editor build and persisted `LOOT_CHECK_PASS 8` passed. Width/thickness differ from several handoff dimensions; see `Artifacts/Orchestration/20261025-resume/loot-import-report.md` in the shared checkout.

## Blocked on

Independent in-engine Opus review and final owner sign-off. Product drop/equip 3D consumers are a separate gameplay task.

## Next step

Wait for the in-engine review fix list; apply only listed fixes (at most three review rounds), capture a new named round, then send to the owner. Preserve source GLBs and the round-1 images. Ask separately for an authorized gameplay implementation task if actual drop/equip 3D presentation is required.

## Rules for this task

- Never patch meshes in Blender; scale and mirror at import only.
- Only write under `/Game/Frontier/`.

## Outcome

