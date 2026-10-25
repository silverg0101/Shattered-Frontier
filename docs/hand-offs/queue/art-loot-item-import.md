# Import the 8 loot item models into Unreal

- **Area:** art
- **Owner of the next step:** owner (sign-off), then agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-b (`pwsh tools/Enter-Lane.ps1 -Name b`)

## Goal

The 8 Forge-built loot items show their 3D models in game at real size, linked to their item records.

## State

All 8 passed independent review: archers_gloves, fangtooth_dagger, wolf_fang (round 1); brigand_blade, cudgel (round 2); captains_longsword s222, wolf_pelt, wolfhide_bracers (round 3). Not committed, not imported. The 5 blender-garment entries in the handoff are not started.

## Evidence

- Models and reviews: `C:\Projects\Shattered-Frontier\Artifacts\LootItems\` (git-ignored, moved out of lane b) (REVIEW-round1..3.md, FINAL-signoff-sheet.jpg). Final files: round-1 items in `<name>\`, round-2 items in `<name>\round2\` (brigand_blade_v3), round-3 items in `<name>\round3\` (captains_longsword_s222.glb).
- Item list and sizes: `content/items/forge-handoff.json` (9dafb6ed, 449ee18a).
- Item records: SFItemDefinition assets from ee22f632.

## Blocked on

Owner sign-off on the sheet.

## Next step

A Codex task: commit the final GLBs as art source, import into `/Game/Frontier/`, scale each to `size_cm` (the Forge normalises everything to about 1 m), mirror the single bracer for the other arm, set each item record's mesh, and capture an in-game check.

## Rules for this task

- Never patch meshes in Blender; scale and mirror at import only.
- Only write under `/Game/Frontier/`.

## Outcome

