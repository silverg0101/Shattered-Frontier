# Silverbrook Stone Bridge — concept prototype

Reference: `StoneBridge-reference.png` (copy of `docs/Game-Design/concept-art/Silverbrook/Stone Bridge.png`; SHA-256 in `asset-manifest.json`). Asset `BLD-GreenlandsStoneBridge-01`. Original procedural geometry interpreting that elevation, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_stonebridge.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookStoneBridge.blend -P review_stonebridge.py
```

The builder writes `SilverbrookStoneBridge.{blend,fbx,glb}` and `asset-manifest.json`. The review script renders `StoneBridge-{Beauty,Front,Side,Rear}.png` from the saved .blend in a fresh process, then reimports the GLB and FBX into empty scenes and writes `exchange-review.json`. Blender 5.1.1 was used.

## Orientation and levels

- **The span runs along X.** The parapet faces look -Y (front) and +Y (rear), and you walk across in ±X. Z is up, units are metres.
- **The origin is at the centre of the span, at riverbank ground level.** `silverbrook-town-layout.json` gives a 24 m river and a 30 × 7 m bridge. `heightfield.py` puts the bank 1.5 m above the water and the channel bed 2 m below the water. So z=0 is the bank, the water is at z=-1.5 and the bed is at z=-3.5.
- **The deck is one continuous flagstone surface, 30 m long and 5.8 m wide between the parapets.** It meets the bank at z=0 at x=±15 and is flat where it does, so there is no step. It rises to z=3.6 at the crown along `z = 3.6·(0.5·(1-u²) + 0.25·(1+cos πu))` with u=|x|/15. Its steepest grade is 0.32 (18°). The build casts rays down three lines along the deck and asserts that every hit lands on the deck mesh, at the profile height, within that grade.
- Bounds are 30.0 × 6.98 × 9.74 m (z from -3.5 to 6.24), inside the slot of 30 × 7 × 10 m (`SLOT`; the build asserts it). The layout gives no height. The 10 m allows for the concept's 6 m above the water plus the pier footings down to the bed.

## Interpretation

- Three semicircular arches spring 0.2 m above the water: a 7.8 m centre arch and two 4.2 m side arches. Each has a ring of dressed voussoirs and a projecting keystone. The arch barrels run the full width of the bridge.
- The piers are 3.2 m wide, with pilasters on both faces. The pilasters rise into tall dressed pillars carrying carved lozenge plaques with a fleur, a cornice, a pyramid cap and a forged-iron spike. Below the plaques hang navy banners with gilt edging and a gilt stag, as in the concept.
- The parapet is 1.05 m high: a rock-faced base course, a dressed arcade of small pointed lancets, and a projecting coping. It sits on a string course over the rock-faced spandrels.
- Smaller pillars with caps and spikes stand at the ends, and low buttresses on the abutments.
- **Inferred (not in the concept):** the rear face (a mirror of the front, banners included), the deck and its paving, and everything below the water.
- **Changed from the concept:** the concept's deck is nearly level, high above its arch bases. Here the deck has to reach bank level at both ends, so it is humped instead. For the same reason the arches are a little lower than the concept's relative to the deck.

## Materials

All surfaces use CC0 Poly Haven scans at their measured physical size. Sources and hashes are in `../materials-provenance.json`; the maps are in `../Textures`. Two scans were added for this package: `stone_brick_wall_001` and `medieval_blocks_03`.

| Material | Surface | Scan |
|---|---|---|
| `SB_StoneBridge_Masonry` | Rock-faced spandrels, piers, abutments, parapet base | `stone_brick_wall_001`, cooled to the concept's grey |
| `SB_StoneBridge_Dressed` | Voussoirs, keystones, pillars, plaques, caps, coping, arcade, string course | `medieval_blocks_03`; voussoirs, cornices and plaques sample single blocks |
| `SB_StoneBridge_Paving` | Deck flagstones | `medieval_blocks_03` |
| `SB_StoneBridge_Banner` | Banners | `denim_fabric_03`, navy |

`SB_ForgedIron` (Cottage1's arguments) and `SB_StoneBridge_Gilt` are plain parametric materials. The stone is damp in a band 0.65 m above the waterline. The build writes this into the `Weathering` vertex colour itself (`rewet`), because the kit measures damp from z=0, which here is the bank.

## Limits

- **Not modelled:** the ivy and moss in the concept are set dressing. The concept's stepped, battered end buttresses are reduced to shallow pilasters, because the 7 m width leaves no room for them to spread outward. The paired small pinnacles at the ends are single end pillars.
- **Unreal:** it has not been imported yet; the intended mesh is `SM_SilverbrookStoneBridge`. The town build (`build_silverbrook.py`) should place the origin at the bank height where the road meets the river, with X along the road.
- **Overlapping geometry:** pillars, pilasters and voussoirs are separate solids that intersect the body. Faces where parts touch sit back to back and are hidden. The meshes are not watertight unions.
- **Shader round-trip and GLB textures:** same as Cottage1. The exports carry UVs, `UVMap`/`WorldUV`, material slots and `Weathering`. The GLB is written without images, and the FBX references `../Textures` relatively; the review asserts that those references resolve.
