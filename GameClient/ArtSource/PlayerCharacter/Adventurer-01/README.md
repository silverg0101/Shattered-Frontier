# Adventurer-01 (player body, GFX-01 WP7 step 2) — REJECTED

Rejected by the owner on 2026-10-21: the image-to-3D mesh breaks down up close. Nothing loads it any more; see [../README.md](../README.md) for the MetaHuman plan. The notes below record what was built.

A clothed frontier adventurer: linen shirt, laced leather jerkin, two belts, a satchel on the right hip with its strap over the left shoulder, a hip pouch, bracers and buckled knee boots. This is a **stopgap on the UE4 `SK_Mannequin` skeleton**; CHR-01 decision 4 moves player characters to the UE5/MetaHuman-compatible skeleton.

## Steps and outputs
1. `concept-v001.png` plus `Views1024-v001/raw/{front,left,back,right}.png`: Codex image generation, prompt in `Views1024-v001/codex-prompt.txt`.
2. `Views-v001/`: `tools/trellis2/prepare_mv_views.py`.
3. `PixalMV-v001/Adventurer.glb`: `tools/trellis2/run_pixal_mv.py --asset-name Adventurer` with the defaults (seed 42, 1024 cascade). The 87 MB `Adventurer.raw.ply` intermediate is not committed.
4. `AutoRig-v001/`: `tools/Build-NpcAutoRig.py --height 1.8 --name Adventurer`, with no clips. It produces 50k triangles and 0 unweighted vertices. `rig-report.json` records the tool hash.
5. Unreal: `tools/Import-NpcSkeletal.py --rig AutoRig-v001 --name Adventurer --root /Game/Frontier/Characters/Player` creates `SK_Adventurer` with 4 engine LODs and Nanite off (`docs/evidence/npc-nanite-bench.md`).

At runtime, `ASFE1Character` attaches it as `Outfit` and leader-poses it from the hidden mannequin running `ThirdPerson_AnimBP`. It is not loaded on a dedicated server. The body takes the mannequin's bone positions.

## Known limits
- Close up, the generated geometry and texture are faceted, worst in the hair and shirt. It reads well at the default 3.5 m zoom.
- The hair is part of the mesh for every player. Groom hair for the local player only is not done yet.

## Moving to the UE5 skeleton (CHR package)
- Re-run `Build-NpcAutoRig.py` against a UE5 Manny/MetaHuman skeleton JSON instead of `Voidreign/Rider/SK_Mannequin-skeleton.json`. The rig fitting needs UE5's bone set (extra spine bones, twist bones, IK bones).
- Re-import with that skeleton, then swap the leader mesh and AnimBP in `SFE1Character.cpp` for UE5 equivalents. The `/Game/Mannequin` constructor paths and `DirectoriesToAlwaysCook` change with them.
- Add the groom (local player) and card hair (remote) as separate components at the same time, and remove the baked hair from the body mesh.
