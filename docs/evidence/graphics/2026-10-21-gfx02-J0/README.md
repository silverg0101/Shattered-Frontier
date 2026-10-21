# GFX-02 J0: sources and grass cards

J0 source: `docs/decisions/GFX-02-photoreal-ground-pipeline.md`. The five bookmark pairs are 1920 × 1080 `-game` captures at the specified views, with concept left and live right in `*-compare.jpg`. `six-grass-cards-compare.jpg` pairs the forest concept's foreground grass crop with the Blender render of all six generated cards. No visual verdict is recorded; independent review is pending.

| Technical check | Result |
|---|---|
| Poly Haven source hashes | PASS: 35 assets, 187 files, 0 MD5 mismatches; includes 6 new scans and 2 grass models |
| Card count and LODs | PASS: 6 cards, 18 FBX files; fresh Blender FBX import found 36/12/4 triangles per card, 1 UV layer and 1 vertex-colour layer in each LOD; 0 failures |
| Atlases | PASS: diffuse and normal atlases both 2048 × 1024 px, 6 slots of 512 × 512 px |
| Live capture size | PASS: 5 captures at 1920 × 1080 px; 5 concept/live JPEG pairs |

The generated card package is `GameClient/ArtSource/Greenlands/NatureFamilies/GrassCards/`; hash provenance is `Sources/provenance.json`. The FBX LOD packaging deviation and Unreal import handoff are recorded in the decision's J0 job log.
