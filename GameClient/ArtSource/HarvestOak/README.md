# Greenlands harvest oak — source study and cutting prototypes

The current full-tree integration source is [MightyOak-v1](MightyOak-v1/README.md), adapted from Jagobo’s CC BY4.0 source with a closed lower trunk and separate upper structure/foliage. Unreal appearance and felling still need review.

`rev04/` contains local editable Blender 5.2.2 source, 10 FBX/GLB meshes, portable PNG materials and inspected previews. **The complete crown fails the professional visual bar**: the structural revision improved curved/fused forks and roots, but foliage still forms horizontal shelves with bare terminal branches. Do not present it as finished environment art. Use the closed lower trunk and small fragments as integration prototypes while selecting a credible crown source. `rev02/` preserves the earlier comparison; rejected/incomplete intermediate outputs are under ignored `Artifacts/Art/HarvestOak/`.

- `SM_HarvestOak_Trunk`: closed continuous lower trunk/root flare, ground–1.75 m; 572 triangles; bark slot 0, wood interior slot 1. The actual saved mesh has 0 nonmanifold edges and 1 UV layer. This is the bounded cutting input, separate from leaves.
- `SM_HarvestOak_Crown`: upper trunk, fused major forks, fine twigs and modeled opaque leaves; matching ground pivot. First branch starts 2.15 m above ground. Crown remains unaccepted.
- `SM_HarvestOak_Intact`, `Cut01`, `Cut02`, `Stump`, `Felled`: comparison geometry. Current interaction direction is hit-based cutting; fixed cut stages do not decide player hit locations. Felled is upright with its local pivot at the original 0.85 m cut plane.
- `SM_HarvestOak_Splinter01`, `Splinter02`, `BarkChip01`: closed irregular cosmetic pieces with centered local pivots, approximately 0.22 m / 0.14 m / 0.11 m long. They mint no resources and have no authority/collision policy.

Metres, +Z up, comparison cuts face −Y; ground origins with slightly buried roots. Exports contain asset meshes only. The `.blend` packs all material images and preserves named editable states and preview lighting. Exact counts/dimensions/hashes are in `asset-manifest.json`; actual-source checks are in `asset-checks.json`.

Named materials are `M_HarvestOak_Bark`, `CutWood`, `Leaf`, `Longgrain` (all share the `M_HarvestOak_` prefix). Assign by name: exporters can omit unused slots. Set Unreal leaf materials Two Sided. Bark basecolor uses the GPT-generated reference retained with its prompt in `../HarvestReferences/`; normal and roughness are aligned luminance-derived approximations, not scanned PBR/true displacement. Blender preview normal strength is 0.30. Geometry, UVs, endgrain, longgrain and leaf maps are project-authored procedural work; no downloads/paid asset inputs.

Reproduce into a new empty folder:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --background --threads 4 --factory-startup --python tools/Create-HarvestOak.py -- --output GameClient/ArtSource/HarvestOak/rev05 --bark-basecolor GameClient/ArtSource/HarvestReferences/OakBark-BaseColor-v1.png
```

Reviewed `Oak-Beauty.png` and targeted trunk/stump/fragment closeups. Redundant whole-crown renders were stopped after its visual failure. No LODs, wind, collision, contact animation, audio, in-engine performance or multiplayer visual acceptance are supplied. Further crown work should address structure/foliage distribution or use a suitable source asset rather than treating texture changes as sufficient.

Rejected canopy/comparison outputs remain local and are excluded by this folder’s `.gitignore`. Git retains the compact `rev04/HarvestOak-Cuttable.blend`, the five lower-trunk/stump/fragment FBXs, maps and relevant close-ups; the full rejected crown is not a shipped asset.
