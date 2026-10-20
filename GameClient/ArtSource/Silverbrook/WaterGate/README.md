# Silverbrook Water Gate - town-wall segment over the river

Reference: none (`reference: null`); designed to match Silverbrook Wall section; no owner concept. Original procedural geometry.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_watergate.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookWaterGate.blend -P review_watergate.py
```

Writes `SilverbrookWaterGate.{blend,fbx,glb}` and `asset-manifest.json` (`unreal_mesh` `SM_SilverbrookWaterGate`, `slot_m` 20 x 3 x 8). The review renders `WaterGate-{Beauty,Front,Side,Rear,Walk,Through}.png` (`Through` is from just above the water, looking through the east arch) and writes `exchange-review.json`. The preview water plane is not exported.

## Interpretation

- **Same as WallSection:** 20 x 3 x 8 m, origin at ground (here the channel bed) centre, front (outside) -Y, ends flat at x = +-10 with identical sections (asserted), turrets at x = +-3.33 and halves at the ends, curtain face y -0.70, parapet, merlons, corbel table, wall-walk (z 5.40, y -0.45..1.15, unchanged). The curtain/turret/parapet code is copied from `build_wallsection.py`, so the two butt seamlessly.
- **Water arches (inferred):** two semi-elliptical openings at x = +-6.667 (outer bays), 3.2 m wide in the barrel, splayed to 3.7 m at both faces over 0.8 m. Spring z 1.8 m, crown z 4.0 m above the bed; the water surface is assumed at z 2.0 m, so the arches spring below the waterline. Dressed voussoir ring and quoins on both faces. The rear ring is flush with the slab recessed 5 cm so nothing leaves the 3 m slot.
- **Grate:** 15 vertical 6 cm square iron bars per opening at 20 cm pitch (14 cm gap; 17 cm to each jamb) from the bed to 10 cm into the soffit, plus six flat rails embedded in the jambs, all in plane y = 0.40 inside the barrel. Closed solid boxes, in the main export as the `SM_SBWaterGate_Grate` object (no separate part), for complex-as-simple collision.
- Dropped from WallSection: the cross arrow loops and string course in the two arch bays (the arch occupies them). The central bay keeps them.

## Materials

Reused from WallSection with identical arguments: `SB_WallSection_Masonry`, `SB_WallSection_Paving`, `SB_WallSection_LoopVoid`. Grate: `SB_ForgedIron` (`kit.solid`, `(.03,.026,.022), .6, .9`, as Cottage1). No new scans.

## Limits

- Not imported to Unreal. The channel bed under the openings is the terrain, not mesh; the pier bases are flat at z 0.
- Bar spacing is a design limit: nothing wider than 17 cm exists in the grate plane. No raising mechanism, no cutwaters (splayed reveals instead).
- The wall-walk is not accessible from the arches; no stairs.
