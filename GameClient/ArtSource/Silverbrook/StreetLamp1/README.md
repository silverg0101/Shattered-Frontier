# Silverbrook Street Lamp 1 — concept prototype

Reference: `StreetLamp1-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Street Lamp-1.png`; SHA-256 in `asset-manifest.json`). Asset ID `BLD-GreenlandsStreetLamp-01`, variant 1. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_streetlamp1.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookStreetLamp1.blend -P review_streetlamp1.py
```

The builder writes `SilverbrookStreetLamp1.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `StreetLamp1-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1. Metres, Z up, front faces -Y, origin at ground centre.

## Interpretation

- The whole model is 1.44 × 0.68 × 4.16 m, which fits the 1.5 × 1.5 × 4.2 m slot (`SLOT`; the build asserts it, and the 40k small-prop budget). About 7.2k triangles. The origin is the slot's ground centre, so the post stands 0.38 m left of it and the lantern hangs 0.45 m right of it.
- A stepped plinth of rock-faced stone (0.68 m at the foot, 0.70 m high) has a riveted iron band. On it stands a 0.22 m oak post that flares at the foot under an iron shoe with face and corner straps.
- The post carries riveted iron bands at 1.6 m, 2.7 m and at the arm, and an iron cap with an octagonal spike finial. The finial tip is 4.16 m; the concept's 4 m dimension ends at the post shoulders.
- The oak arm reaches about 1.1 m from the post centre. It has two iron collars and an iron spike tip, a curved oak brace below, and a forged scroll.
- A four-link chain carries a hexagonal iron lantern: pyramid roof, eave plate, corner posts, top and bottom bands and a drop finial. Each glass face carries a mullion and diamond/pointed-arch cames.
- **Light:** the lantern glass is its own material slot, `SB_StreetLamp1_Glow`, so Unreal can pair it with a point light. The light position is `light_socket_m` in `asset-manifest.json`: (0.45, 0, 2.55) m in model space, the centre of the glass. The preview scene has a point light there; it is not exported.
- **Inferred (not in the concept):** the rear and side faces (the concept is one elevation), the depth of the arm and lantern (centred on the post), and the scroll's exact shape. The concept's gothic tracery is simplified to straight cames.

## Materials

Scanned surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. No new scans were downloaded.

| Surface | Material | Scan |
|---|---|---|
| Plinth | `SB_StreetLamp1_Plinth` | `rough_block_wall` (rock-faced squared blocks), darkened toward the concept's grey-olive |
| Post, flared foot, arm, brace | `SB_OakTimber` (Cottage1's arguments) | `weathered_brown_planks`, each beam confined to one plank's grain band |
| Bands, rivets, straps, finials, scroll, chain, lantern frame | `SB_ForgedIron` (Cottage1's arguments) | parametric |
| Lantern glass | `SB_StreetLamp1_Glow` | parametric amber, emission (1.0, 0.4, 0.08) × 2.5 |

## Limits

- **Unreal:** not imported yet (`../import_silverbrook.py` is the shared importer). The point light is to be added there at `light_socket_m`.
- **Shader round-trip:** the Blender shader graph does not round-trip to FBX/GLB. The exports carry UVs, the `UVMap`/`WorldUV` layers, material slots and the `Weathering` vertex colour. The review checks that the FBX texture references resolve.
- **GLB textures:** the GLB is exported without images; the FBX references the textures relatively.
- **Not modelled:** the concept's moss on the plinth (the shared kit's lichen is the orange slate kind), and the lantern's flame.
