# Silverbrook Bridge Abutment

`SM_SilverbrookBridgeAbutment`, asset `BLD-SilverbrookBridgeAbutment-01`. Designed to fit Silverbrook crossings; no owner concept (`reference` is null in the manifest). Context: `GameClient/ArtSource/Greenlands/Silverbrook-Crossing-InGame.png` (boulders on narrowed raised banks). It replaces the riprap boulders at each bridge end. Pairs with `../RiverBridge/`.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_bridgeabutment.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookBridgeAbutment.blend -P review_bridgeabutment.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P pair_preview.py   # needs both FBXs; renders Pairing-*.png
```

Outputs: `SilverbrookBridgeAbutment.{blend,fbx,glb}`, `asset-manifest.json`, `BridgeAbutment-{Beauty,Front,Side,Rear}.png`, `exchange-review.json`, and the pairing renders `Pairing-{Beauty,Front,Top}.png` (textures show white there because the headless FBX reimport drops the shader; geometry fit is the point). Blender 5.1.1.

## Orientation and levels

- **Local axes:** X along the bank (across the bridge width), **-Y faces the river**, Z up, metres.
- **Origin:** centre of the top edge on the bank side (y=0, z=0). Everything lies at y<=0 and z<=0 (asserted).
- **Levels:** coping top z=0.00 (bank / deck-end ground level); stone runs to z=-4.0 for 2 to 4 m banks (terrain hides the excess); earth cap z=-0.05 so it never fights the road.
- **Size:** 16.73 x 5.23 x 4.0 m, slot 17 x 5.4 x 4.2 m (asserted).

## Pairing with the bridge end

The bridge deck ends at bridge x=+-11 (z=0). Place the abutment origin at **bridge x=+-14.0, y=0, z=0** (3 m past the deck end), bridge-local:

| Bank | Origin | Rotation so local -Y points at the river |
|---|---|---|
| +X | (14, 0, 0) | -90 deg about Z (local +Y -> bridge +X) |
| -X | (-14, 0, 0) | +90 deg about Z (local +Y -> bridge -X), i.e. the mirror |

The front wall face then lies at bridge |x|=8.9, 0.1 m riverward of the arch springers (|x|=9), so the arches emerge from the wall. The front wall is 6.9 m wide against the bridge's 5.4 m. The bridge end masses are buried in the abutment, and the earth plateau and the road at z=0 continue past |x|=11. Under both banks z=0 is the shared ground level; if the two banks differ, drop each abutment by its own end-of-deck height, as for the bridge.

(Convention in Unreal terms: yaw is applied with the engine's own sign; the rule is the vector mapping above.)

## Interpretation

- Front wall: 0.7 m thick coursed block masonry in running bond (courses 0.30 to 0.40 m), with corner piers that project 0.12 m and dressed coping.
- Two wingwalls 0.6 m thick splay back from the wall ends at about 43 deg, 6.4 m long, stepping down 0.45 m per segment from z=-0.17 to -2.4 with a coping slab on each step.
- Between the wings sits a grass earth ramp (`leafy_grass`) that falls away with the coping, closed at the bank edge. The wing tips go below ground level, so the terrain should cover them.
- **Inferred:** all of it. Vertical walls (no batter), rear sides of the wings are the same coursing.

## Materials

| Material | Surface | Scan |
|---|---|---|
| `SB_RiverAbutment_Dressed` | Wall blocks, coping, piers | `medieval_blocks_03`, each block samples one undistorted block interior (0.45 to 0.65 m long) |
| `SB_RiverAbutment_Grass` | Earth cap and bank edge | `leafy_grass` (new: `acquire_materials.py leafy_grass`), tinted greener |

Damp band is measured from a preview waterline at z=-2.6.

## Limits

- No footings below -4 m, no drainage weep holes, no batter.
- The earth cap is flat-shaded grass texture, not a mesh blended to the terrain; the placer must make sure terrain surrounds it. Not imported into Unreal.
