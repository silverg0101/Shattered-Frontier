# Skinned Nanite vs LOD: 100 walking beastfolk NPCs

Question: for a crowd of Pixal-generated humanoids, is `SK_BeastfolkWarrior_Nanite` (skinned Nanite, `r.Nanite.AllowSkinnedMeshes=1`)
cheaper than `SK_BeastfolkWarrior` with 4 engine-reduced LODs? Asset: `/Game/Frontier/Characters/NPC/BeastfolkWarrior/V3`
(~286k triangles at LOD0, `M_BeastfolkWarrior`, ThirdPersonWalk looping, random phase and yaw).

## Setup

- Maps `/Game/Frontier/Tests/NpcBench_{LOD,Nanite}_{Near,Far}` from `tools/Build-NpcNaniteBench.py`: 10x10 grid, 1.5 m spacing, floor, sun, sky light; fixed camera 6 m (Near) or 70 m (Far) from the grid edge, PlayerStarts tagged Near/Far.
- `tools/Run-NpcNaniteBench.ps1`: standalone `-game` at 2560x1440 (CSV metadata confirms), `t.MaxFPS 0, r.VSync 0`, `-csvCaptureFrames=6000 -csvGpuStats`. A warm-up pass per map ran first; the first 1500 frames of each capture are dropped, leaving 4501 frames. Summary: `tools/Summarize-NpcBench.py`, raw numbers in `npc-nanite-bench/summary.txt`.
- Machine: i7-14700K, RX 7900 XTX, UE 5.8 Development build, no other GPU work during the captures (the editor was open and idle, other sessions held only headless processes). CSVs and per-run logs stay in git-ignored `Artifacts/NpcBench/`; screenshots (downscaled) are in `npc-nanite-bench/`.

## Results (ms, mean / p95 over 4501 frames)

| Map | Frame | GPU | Render thread | Game thread | ShadowDepths | Basepass |
|---|---|---|---|---|---|---|
| LOD, Near | 2.88 / 3.53 | 2.03 / 2.31 | 2.83 / 3.36 | 2.47 / 3.35 | 0.10 | 0.10 |
| Nanite, Near | 3.51 / 4.21 | 2.52 / 2.78 | 3.50 / 4.19 | 2.59 / 3.28 | 0.39 | 0.09 |
| LOD, Far | 2.86 / 3.64 | 1.94 / 2.20 | 2.76 / 3.31 | 2.53 / 3.52 | 0.09 | 0.07 |
| Nanite, Far | 3.70 / 4.96 | 2.49 / 2.96 | 3.69 / 4.94 | 2.65 / 3.50 | 0.39 | 0.07 |

Screenshots: `npc-nanite-bench/NpcBench_{LOD,Nanite}_{Near,Far}.jpg` (Near and Far look identical between variants; the Nanite mesh renders with the same textured material).

## Reading

- With 100 skinned instances the LOD path is faster in both views: about 0.6-0.8 ms less frame time and 0.5 ms less GPU time. A second full run (earlier, before the resolution fix) gave the same ordering and similar magnitudes (LOD 2.9 / Nanite 3.4-3.5 ms).
- The Nanite deficit sits mostly in shadow depth (0.39 ms vs 0.10 ms) and in render-thread cost; base pass is no cheaper for Nanite here.
- Far does not favour Nanite either: the LOD variant gets cheaper at distance (LOD swaps; GPU 2.03 -> 1.94) while Nanite stays flat or worsens.
- Frame time is render-thread bound at about 350-450 fps, so this scene is light; the comparison shows relative overhead, not a budget. The conclusion holds for this asset and count only: ~286k triangle skinned meshes with a 4-LOD chain. It does not cover denser crowds, Lumen, or virtual shadow maps being the active shadow path.
- Nanite skinned meshes being active was inferred from the ShadowDepths and render-thread difference, not from a Nanite stat dump; a `stat NaniteStats`/`NaniteVisualize` check would confirm it.

Suggested treatment, pending steps 4 and 5 of the NPC task list: keep engine LODs as the default for skinned NPCs.
