# Silverbrook PropShopSign

Reference: `concept/shop-sign-sheet.png` (Codex sheet, SHA-256 in `asset-manifest.json`). Original procedural geometry interpreting it, not image reconstruction.

```
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propshopsign.py
"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup SilverbrookPropShopSign.blend -P review_propshopsign.py
```

Writes `SilverbrookPropShopSign.{blend,fbx,glb}`, `asset-manifest.json`, `PropShopSign-board-{bread,boots,ale}.png` (512 px board faces cut from the sheet's bottom row); the review writes `PropShopSign-{Beauty,Front,Side,Rear}.png` and `exchange-review.json`. Unreal mesh `SM_SilverbrookPropShopSign`.

Metres, Z up, front -Y. **Origin = centre of the wall plate's back face** (the wall is the plane y=0); the 0.7 m arm projects along -Y, the board hangs below it and faces +-X. Wall plate 0.07 x 0.5 m; board 0.6 x 0.55 m (0.45 m body plus the scalloped point), two rings. 0.07 x 0.70 x 0.83 m, 1.9k triangles.

## Variants

One mesh, one painted-face slot `SB_PropShopSignBoard` (image material, `M_SB_Card` in Unreal, bread). `import_silverbrook.py` also creates `MI_SB_PropShopSignBoard_Boots` and `_Ale` from the manifest `variants`; assign one to the board slot of a placed instance for another trade. Both board faces use the texture (the +X face is mirrored so it reads correctly from either side). Straps, rings and bracket are modelled; the straps also appear painted on the crops.

## Limits

- Shader graph does not round-trip to FBX/GLB; Unreal rebuilds materials from `asset-manifest.json`.
- Flat shaded straight scrolls (10-16 segments per curl); no twisted-bar detail on the arm.
