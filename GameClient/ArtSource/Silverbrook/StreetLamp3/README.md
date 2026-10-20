# Silverbrook Street Lamp 3 — concept prototype

Reference: `StreetLamp3-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Street Lamp-3.png`; SHA-256 in `asset-manifest.json`). Asset `BLD-GreenlandsStreetLamp-01`, variant 3. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_streetlamp3.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookStreetLamp3.blend -P review_streetlamp3.py
```

The builder writes `SilverbrookStreetLamp3.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `StreetLamp3-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The concept's labels disagree with each other: measured on the 4 m height label, the 1.2 m width label would be 1.6 m. The build follows both labels on their own axes: the post top is at 3.82 m and the finial tip at 4.11 m, and the lantern-to-banner span is about 1.2 m. With the banner rod's spear tip, the model is 1.45 × 0.72 × 4.11 m. That fits the 1.5 × 1.5 × 4.2 m slot (`SLOT`; the build asserts it, and also asserts ≤ 40k triangles).
- A 0.2 m square oak post with an iron-capped pyramidal finial stands on a stepped stone plinth: a ring of eight blocks, a core stone and a two-block cap. Four timber shores with iron shoes brace the post on all four sides.
- The post carries three iron bands with rivets and three rope lashings: at the arm, at the brace foot and low on the post.
- A braced gallows arm reaches out to the left (-X). It ends in an iron strap and hangs a chain from an eye plate.
- On the chain hangs a hexagonal forged lantern: glowing glass panels, a corner-barred cage with crossed lattice, a stepped cap with corner spikes and a hanging loop, and a pointed base.
- To the right, an iron rod with a spear tip and a diagonal strut hangs a wooden batten on three ringed chains. The navy banner below has a pointed foot, gilt edging and a gilt fringe, with a gilt stag head and a fleur-de-lis.
- **Light socket:** `light_socket_m` in the manifest is the centre of the lantern glass in model space: [-0.558, 0, 2.43]. Pair the point light there with the `SB_StreetLamp3_Glow` slot.
- **Inferred (not in the concept):** the depth (shores on all four sides, a square plinth), the rear face of the banner (it repeats the stag and fleur), and the lattice pattern, which is simplified to crossed bars. The concept's candle inside the lantern is not modelled; the glass is a single emissive slot.

## Materials

| Surface | Material | Source |
|---|---|---|
| Post, arm, brace, shores, batten | `SB_OakTimber` | `weathered_brown_planks`, each beam confined to one plank's grain band |
| Plinth stones | `SB_Ashlar` | `stone_wall_04`, each block samples a single stone |
| Rope lashings | `SB_StreetLamp3_Rope` | `hessian_380`, aged darker |
| Banner cloth | `SB_StreetLamp3_Banner` | `denim_fabric_03`, darkened to navy |
| Iron | `SB_ForgedIron` | parametric |
| Stag, fleur, edging, fringe | `SB_StreetLamp3_Gilt` | parametric |
| Lantern glass | `SB_StreetLamp3_Glow` | parametric, emissive (1.0, 0.36, 0.07) at strength 3 |

All the scans are CC0 Poly Haven textures that were already in `../materials-provenance.json`; none were downloaded. Damp at the base uses the exported `Weathering` vertex colour.

## Limits

- **Unreal:** the asset has not been imported yet. The mesh name is `SM_SilverbrookStreetLamp3`. There is no light in the exported files; the preview `LanternLight` lives only in the .blend's Preview collection.
- **Shader round-trip:** the exports carry UVs, material slots and `Weathering` vertex colours. The Blender shader graph does not round-trip. The review checks that the FBX texture references resolve.
- **GLB textures:** the GLB is exported without images; the FBX references `../Textures` relatively.
- **Simplified:** the rope is a rippled lathe, not twisted strands, and there is no knot. The banner is flat with no cloth folds. The brace under the banner rod is straight, not scrolled.
