# Silverbrook Street Lamp 4 — concept prototype

Reference: `StreetLamp4-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Street Lamp-4.png`; SHA-256 in `asset-manifest.json`). Asset `BLD-GreenlandsStreetLamp-01`, variant 4. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_streetlamp4.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookStreetLamp4.blend -P review_streetlamp4.py
```

The builder writes `SilverbrookStreetLamp4.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `StreetLamp4-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- **Slot 1.5 × 1.5 × 5.0 m, not the default 4.2 m.** The concept labels the lamp 5 m tall, which does not fit in 4.2 m. The model is 1.30 × 1.30 × 5.00 m: the plinth matches the 1.3 m label, and the finial tip is at 5.0 m. The build asserts that it fits the slot and stays under 40k triangles (36,206).
- The concept's two labels disagree on horizontal scale. The plinth follows the 1.3 m label. Everything above the pedestal is drawn at the 5 m label's scale (`K = 0.87`).
- **Pedestal.** It is a square turned 45° with chamfered corners, so a corner faces the viewer as in the concept. From the ground up: a 1.3 m plinth with a sloped wash; a tapered body with a carved fleur-de-lis in a raised frame on each face; an overhanging cap course; an iron band; a stone step; and a second iron band. Both iron bands carry bronze studs.
- **Column.** An iron core carries oak panels between iron stiles. The lowest stage has bronze fleurs-de-lis. Studded bands separate the stages: a flanged collar, and two bands with X-plates.
- **Brackets.** Four iron brackets sit under the lantern. Each has a straight top arm ending in a fleur pinnacle with a drop, a curved strut ending in a scroll, and a stay back to the column.
- **Lantern.** It is hexagonal, with a flat face to the front, and widens towards the top. Each pane has a pointed lancet frame with two sub-arches and a mullion, and leaded diamond quarries below. Above it are a studded cornice, six corner pinnacles on short arms, and a ribbed, flared pyramid roof with a small bronze fleur on each face. On top is a collar, a crossed fleur and a spike.
- **Light:** the glowing glass is a separate solid on its own slot, `SB_StreetLamp4_Glow`. `light_socket_m` in the manifest is the lantern centre, `[0, 0, 3.788]` m in model space. The preview scene has a point light there.
- **Inferred (not visible in the concept):** the back and side faces mirror the front, there are four brackets (the concept shows two in profile and one face-on), and the column's square section. The concept shows a candle flame inside the lantern; it is not modelled, because the glass is an opaque emitter.

## Materials

The stone and timber are CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. No new scans were downloaded.

| Surface | Material | Source |
|---|---|---|
| Pedestal stone, carved panels | `SB_StreetLamp4_Stone` | `medieval_blocks_03` (grey-brown dressed blocks; one block interior per course face), aged darker |
| Column panels | `SB_OakTimber` (Cottage1 arguments) | `weathered_brown_planks`, one plank band per panel |
| Iron frame, bands, roof, brackets | `SB_ForgedIron` (Cottage1 arguments) | parametric |
| Leaded quarries | `SB_LeadCame` (Cottage1 arguments) | parametric |
| Fleurs-de-lis, studs, X-plates, spike | `SB_StreetLamp4_Bronze` | parametric metal |
| Lantern glass | `SB_StreetLamp4_Glow` | parametric, amber emission |

## Limits

- **Unreal:** the mesh is `SM_SilverbrookStreetLamp4`. It has not been imported yet, so there is no `ue-import-report.json`. Unreal should place its point light at `light_socket_m`.
- **Geometry:** fleurs, studs and bracket segments are separate closed solids that interpenetrate their supports, so the meshes are not watertight. The narrowing by `K` also narrows the fleur and stud shapes above the pedestal by 13%.
- **Shader round-trip:** the Blender shader graph does not round-trip to FBX/GLB. The exports carry UVs, material slots and the `Weathering` vertex colours, and `M_SB_Scan` rebuilds the material from the manifest's material table. The review checks that the FBX texture references resolve.
- **GLB textures:** the GLB is exported without images; the FBX references the textures relatively.
