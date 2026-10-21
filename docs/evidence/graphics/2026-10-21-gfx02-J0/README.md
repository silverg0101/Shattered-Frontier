# GFX-02 J0: sources and grass cards

J0 source: `docs/decisions/GFX-02-photoreal-ground-pipeline.md`. The five bookmark pairs are 1920 × 1080 baseline `-game` captures at the specified views, with concept left and live right in `*-compare.jpg`. `six-grass-cards-compare.jpg` now shows the forest and gameplay HUD concepts beside all six generated cards on mid-grey and grass-green, each at native 1:1 and projected 3 m (1080p, 60° FOV). Independent concept/result review passed after cards 02/03 were enlarged for 3 m legibility. No Unreal run was made for this redo.

| Technical check | Result |
|---|---|
| Poly Haven source hashes | PASS: 35 assets, 187 files, 0 MD5 mismatches; includes 6 new scans and 2 grass models |
| Card count and LODs | PASS: 6 cards, 18 FBX files; fresh Blender FBX import found 36/12/4 triangles per card, 1 UV layer and 1 vertex-colour layer in each LOD; 0 failures |
| Atlases | PASS: diffuse and normal atlases both 2048 × 1024 px, 6 slots of 512 × 512 px; matching alpha |
| Alpha/mips | PASS: straight alpha with nearest-colour dilation; no black RGB in transparent texels or box-filtered alpha edges from full size through 4 px (minimum channel 22–45) |
| Redo visual review | PASS: all six on both backgrounds at both scales; dense, predominantly fresh green; no visible dark blotches/fringes |
| Live capture size | PASS: 5 captures at 1920 × 1080 px; 5 concept/live JPEG pairs |

The generated card package is `GameClient/ArtSource/Greenlands/NatureFamilies/GrassCards/`; hash provenance is `Sources/provenance.json`. The FBX LOD packaging deviation and Unreal import handoff are recorded in the decision's J0 job log.
