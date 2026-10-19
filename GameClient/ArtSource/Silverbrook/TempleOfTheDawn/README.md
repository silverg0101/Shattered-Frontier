# Silverbrook Temple of the Dawn — concept prototype

Reference: `TempleOfTheDawn-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Temple of the Dawn.png`; SHA-256 in `asset-manifest.json`). This is original procedural geometry that interprets the front elevation. It is not an image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_templeofthedawn.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookTempleOfTheDawn.blend -P review_templeofthedawn.py
```

The builder writes `SilverbrookTempleOfTheDawn.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `TempleOfTheDawn-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process. It then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used. Units are metres, Z is up, the front faces -Y, and the origin is at the ground centre.

## Interpretation

- **Size:** the model is 12.3 × 21.6 × 19.94 m, including the steps, the turrets and the star finial. It fits the 14 × 24 × 20 m town slot (`SLOT`; the build asserts both the fit and the triangle budget).
- **Plan:** a hall temple of three parallel gabled bodies. The nave is 5.8 m wide, with a 6.8 m eave and an 11.6 m ridge. Each aisle is 2.6 m wide, with a 5.8 m eave and a 9.6 m ridge. All three run 19 m deep. The front shows the concept's three stone gables, each with a raised parapet, dressed coping and a gilt finial.
- **Nave front:** a sun rose window (blue glass, a gilt sun with 16 rays, stone spokes and a voussoir ring). Two small lancets flank the porch gable and one sits near the apex.
- **Portal:** a deep stone porch gable with a slate top and corner pinnacles. Its 2.6 m pointed archway has three pairs of colonnettes carrying archivolts. Inside is a pointed, ledged double door in oak with iron straps and rings, and a lantern hangs in the arch. The door starts at the landing top (0.6 m), which you reach by two 0.2 m steps between two lantern pedestals.
- **Buttresses:** the two front buttresses carry navy banners with gilt suns and rise into octagonal turrets with slate spires.
- **Aisle fronts:** each has a pair of leaded lancets, a string course and a small sun rose. Round turrets with conical slate spires stand at the outer corners.
- **Bell tower:** it rises through the nave roof. The lower stage has an open lancet on each face with a bronze bell on an iron beam, under a corbelled cornice. The belfry has four corner pinnacles, a second bell and an octagonal slate spire topped by the gilt star of the dawn.
- **Inferred (not in the concept):** the sides, rear and depth. There are five lancets on each aisle wall, with buttresses, an eave cornice and a plinth. The rear has a tall lancet and a plain rose. The tower stands 1.35 m behind the front gable, and both bell stages are open on all four sides.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`, and the maps are in `../Textures`. Three scans were added for this package: `sandstone_blocks_05`, `sandstone_blocks_08` and `denim_fabric_03`.

| Material | Surface | Scan |
|---|---|---|
| `SB_TempleOfTheDawn_Ashlar` | walls, buttresses, turrets, tower | `sandstone_blocks_05` (pale coursed sandstone) |
| `SB_TempleOfTheDawn_Dressed` | coping, hood moulds, voussoirs, colonnettes, cornices, steps, plinth | `sandstone_blocks_08`; the small parts sample single stones |
| `SB_TempleOfTheDawn_Slate` | roof slates, ridges, spires | `grey_roof_tiles_02`, darkened to navy by the `Weathering` vertex colour |
| `SB_TempleOfTheDawn_Banner` | banners | `denim_fabric_03`, darkened to navy |
| `SB_DoorBoards` | door | `old_planks_02` (Cottage1's arguments) |

These are plain parametric materials: `SB_ForgedIron`, `SB_LeadCame`, `SB_LitGlass` (amber lancets) and `SB_LanternGlow`, all with Cottage1's arguments, plus `SB_TempleOfTheDawn_Gilt`, `SB_TempleOfTheDawn_Bronze` (bells) and `SB_TempleOfTheDawn_BlueGlass` (roses).

## Limits

- **Triangles:** 254,748, under the 400k landmark budget. Individual slates make up about 209k of them.
- **Not matched:** the concept shows tall conical slate roofs behind the aisle gables, rising to about 11 m. Here the aisles have plain gable roofs, so the front reads flatter above the aisles. The tracery is simplified to leading, spokes and hood moulds, without cusps or quatrefoils. The concept's small pinnacles in front of the buttress turrets are left out.
- **Unreal:** the asset has not been imported; `../import_silverbrook.py` imports it. There is no interior, and the door is closed. The meshes are not watertight: course prisms, slates and trim overlap where they meet.
- **Shader round-trip and GLB:** the same as Cottage1. The exports carry UVs, the `UVMap`/`WorldUV` layers, material slots and the `Weathering` vertex colour. The GLB has no embedded images, and the FBX references `../Textures` relatively. The review checks that those references resolve.
- **Not modelled:** set dressing (cypresses, shrubs, flower beds, planters).
