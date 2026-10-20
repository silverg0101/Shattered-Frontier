# Silverbrook Street Lamp 2 — concept prototype

Reference: `StreetLamp2-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Street Lamp-2.png`; SHA-256 in `asset-manifest.json`). Asset `BLD-GreenlandsStreetLamp-01`, variant 2. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_streetlamp2.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookStreetLamp2.blend -P review_streetlamp2.py
```

The builder writes `SilverbrookStreetLamp2.{blend,fbx,glb}` and `asset-manifest.json`. The review renders `StreetLamp2-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- **Slot 1.5 × 1.5 × 4.5 m, not the default 4.2 m.** The concept labels the lamp 4.5 m tall, so the slot height follows the label (`slot_m`). The model is 1.49 × 0.64 × 4.49 m; the build asserts it fits.
- The two labels disagree with the drawing's proportions (1.5 m spans 435 px, 4.5 m spans 1050 px). Each axis therefore has its own scale: 4.5 m from the ground line to the spear tip, and 1.5 m across the outer lantern bands. Every part is placed in the concept's pixel coordinates (`P()` in the build), so the lanterns are a little slimmer than drawn.
- From the ground up: a bolted plinth with corner blocks; a tapered square pedestal with flared corner pilasters and a bronze-framed fleur-de-lis panel; a cap with studs; an octagonal bell, bolted band and taper; a moulded ring; then the round column with three banded rings.
- At the top is a square section between two bolted collars with pendant bosses and a small fleur-de-lis. Above it are a spindle with two small curls, a fleur-de-lis finial and a spear point.
- On each side, a main scrolled arm springs from the upper collar, rises over a curled crest and loops down to the chain. A lower C-scroll springs from the lower collar and carries a fleur-de-lis.
- Each lantern hangs from three chain links. It is hexagonal: a bronze knob, a pyramid roof with corner spikes, studded top and base bands, and glowing glass behind corner posts. Each face has a pointed-arch and diamond-lattice tracery, and the lantern ends in a cone with a bronze drip.
- **Inferred (not in the concept):** fleur-de-lis panels on all four pedestal faces and on the back of the square section, the second (crossed) finial fleur that gives it depth, the hexagonal plan of the lanterns, and the pedestal depth (made square).

## Materials

The lamp is all metal and glass, so it uses plain parametric materials and no scans.

| Surface | Material |
|---|---|
| Column, pedestal, arms, chain, lantern frames | `SB_ForgedIron` (same arguments as Cottage1) |
| Fleurs-de-lis, studs, bands, spear, spikes, knobs, drips | `SB_StreetLamp2_Bronze` (`kit.solid`, metallic) |
| Lantern glass | `SB_StreetLamp2_Glow` (`kit.solid` with amber emission), on its own slot |

`light_socket_m` in `asset-manifest.json` gives each lantern's light position (model space, metres; the glass centre, ±0.564, 0, 3.03), ready for a point light in Unreal. The preview .blend has two point lights there.

## Limits

- **Unreal:** not imported yet. `../import_silverbrook.py` would build `SM_SilverbrookStreetLamp2` with convex collision. The point lights are not placed in Unreal.
- **Wear:** the concept's rust and worn-bronze edges are not modelled. Both metals are flat parametric colours, and the `Weathering` vertex colour is zero.
- **Glass:** the glass is opaque emissive geometry, with no candle or interior.
- **Triangles:** 23,940 in total (budget 40k), in two meshes, `SM_SBStreetLamp2_Iron` and `SM_SBStreetLamp2_Lanterns`.
- **Textures:** none. The review asserts the FBX references no missing images; the source and FBX image counts are both 0.
