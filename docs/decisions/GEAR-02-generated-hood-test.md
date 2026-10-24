# GEAR-02: a generated hood through the outfit-asset pipeline

Date: 2026-10-24. Status: test complete (works end to end, with defects); visuals not reviewed or signed off.

Question: can an Asset Forge garment be cleaned by a committed Blender script and fitted to one MetaHuman face (Edwin) through a Chaos Outfit Asset (GEAR-01 path)? Garment: early-medieval Anglo-Saxon undyed brown wool hood with a short shoulder cape.

## Answer

Yes. Forge views -> Forge 3d -> `tools/Clean-GeneratedGarment.py` (about 50 s, no hand edits) -> `Import-GeneratedGarment.py` -> `Create-PlayerOutfitAsset.py item=Hood` -> Edwin built and captured in game. The face sits in the opening, the hood follows the head, the cape the shoulders. The in-game captures show a crumpled, jagged cape hem and a faceted hood (12k-triangle decimation of a 100k-face mesh); not a shippable look, but the pipeline accepts the mesh.

## Steps and time (wall minutes)

| Step | Min | Notes |
|---|---|---|
| Orlando clay views (`Render-BodyReference.py`) | 5 | Forge image jobs take no reference image, so they were not fed to it (see What broke 1) |
| Image job, 3 views (heroes, `type item`, `Armor/Cloth/Head`) | 2 to 3 per attempt | 3 attempts incl. the failed v1 |
| Forge 3d job (Pixal3D + UltraTex, 100k faces, 2048 tex) | 20 (v1), 18 (v2) | RA-SIEMS GPU 1 |
| Cleanup script | under 1 per run; about 30 to develop | |
| Unreal import / author (OA 22.5 MB) / Edwin build | 0.5 / 0.7 / 3.3 | editor start about 0.5 |
| Capture (front, three-quarter, side, walk) | 1.2 | |

Total about 100 min wall, about 40 of it waiting on the Forge.

## What broke, and the fixes

1. **Image jobs cannot take a reference image** (`mode single|multi`, prompt only), so the Orlando body render could not drive pose and scale. The prompt describes an invisible man in an A-pose; scale and placement are fixed in the cleanup script (`--top/--bottom/--yshift`, defaults tuned on Edwin's head, Orlando's frame).
2. **v1 views had a see-through face opening.** The ghost-mannequin image showed white through the face opening, so Pixal3D built a tunnel: the clay back view showed straight through the hood (`docs/evidence/player-outfit-hood/` keeps only v2). Fix at the input: v2 prompt demands the dark inner lining behind the opening and a closed back and cape. Gate added before every 3d job: `tools/Check-GarmentViews.py` (no background enclosed by the garment; v1 front 3276 px, v2 front/left/right 0). Gate added after it: `Clean-GeneratedGarment.py` casts 783 rays from the head centre to the back (+-40 deg, -45..+40 deg) and exits loudly if any pass through the hood (v1: 34 of 81 missed; v2 at the tuned fit: 0).
3. **glTF vertices are split along UV seams**, so the mesh had 31k boundary edges and decimation shredded it. Fix: weld (0.02 mm) before anything else.
4. **Cutting everything inside the body opened holes in the cape** where it rests on the chest and arms. Fix: below the head mesh the cape vertices are pushed out to the skin plus the offset; only the face and neck zone is cut.
5. **Smart UV fragments the crumpled low shell into hundreds of islands**, and the empty texels were black, which darkened the cloth through the mips in game (it looked grey). Fix: empty texels become the mean cloth colour. A one-seam unwrap was tried and overlapped worse; left as a known defect (`ponytail:` comment in the script).
6. **Import left `WorldGridMaterial` on the mesh**: editing the material slots in place did not stick. Fix: new `SkeletalMaterial` structs, save with `only_if_is_dirty=False`, and a check that reads back (a stale outfit asset cannot be reused: delete `OutfitAsset/Hood` and re-author after changing the mesh).
7. A first Blender run took 10 min: `dict.setdefault(n, new_group())` made a vertex group per vertex. Fixed.

## Does the head fit in v2? (measured on the aligned, uncut mesh, Edwin's Orlando head)

- Head vertices with a hood wall inside them (above 3 mm): 0.0% of 3670, max 0 cm.
- At eye height along y: front rim to the inner back wall 38.6 cm; Orlando's head is 21.0 cm deep; the inner back wall is 4 cm behind the back of the head. A cut-away through x = 0 is `hood_cleaned_section.png`.
- The fit needs the hood scaled so it stands about 16 cm above the head (`--top 206`, cape hem 117): at `--top 197` the back of the skull poked through. Real clearance depends on the scale, so it is a script default, not a property of the mesh.
- The earlier worry, a filled face opening, was a misread: the inner back wall seen through the opening is the hood's lining. v3 (garment worn on a grey mannequin bust) was not needed and was not run.

## Open defects

- Cape hem torn and crumpled in game; faceted hood; chest-side cape folds.
- UV atlas is hundreds of islands (texel density poor).
- Hood is oversized and tall; no neck seal, so the neck is bare skin between hood and chest.
- Back centre of the generated mesh has a thin crease (not an opening: the ray check passes).
- Not reviewed by an independent reviewer; owner sign-off pending.

## Reproduce

```text
blender --factory-startup -b --python tools/Check-GarmentViews.py -- front.png left.png right.png
blender --factory-startup -b --python tools/Clean-GeneratedGarment.py -- --garment GameClient/ArtSource/Characters/Player/outfit-asset/hood/hood.glb --out Artifacts/Characters/PlayerBase/hood/hood.fbx
python tools/Run-InEditor.py tools/Import-GeneratedGarment.py name=Hood
python tools/Run-InEditor.py tools/Create-PlayerOutfitAsset.py item=Hood           (author)
python tools/Run-InEditor.py tools/Create-PlayerOutfitAsset.py item=Hood build Edwin
capture: tools/Capture-PlayerOutfit.py with SF_FACES=Edwin and -SFOutfitAssetTest (builds go to the git-ignored OutfitAsset/Built, replacing Edwin's tunic build)
blender --factory-startup -b --python tools/Render-ClaySheet.py -- <mesh> <out.png> [--section]
```

## Evidence

`docs/evidence/player-outfit-hood/`: `edwin_{front,three_quarter,side,walk_back}.jpg`, `hood_generated_clay.png` (the Forge mesh, 2048 px: front, left, back / right, top, bottom), `hood_cleaned_clay.png`, `hood_cleaned_section.png`, `input_views.png` (front, left, right with guide lines).

## Round 2 (fix round after an independent FAIL of round 1)

Evidence: `docs/evidence/player-outfit-hood/round2/` (`edwin_{front,three_quarter,side,walk_back}.jpg`, `hood_cleaned_clay.png`, `hood_cleaned_section.png`). Not self-judged; goes to a fresh reviewer.

| Reviewer item | Cause and fix |
|---|---|
| 1 Torso and hips invisible | Not the tunic mask: `WI_Hood` is a duplicate of Epic's `WI_DefaultGarment`, whose pipeline carries the shirt-and-shorts `BodyHiddenFaceMapTexture` (`DG_BodyShapeA_BodyMask`). `Create-PlayerOutfitAsset.py` now clears it for the hood (and fails loudly if it is still set). Nothing is hidden. |
| 2 Holes, slit tears | The Forge mesh is a closed solid of two layers 2 to 19 mm apart; decimating it to ~5k faces merged the layers, and the cleanup also deleted faces near the skin. `Clean-GeneratedGarment.py` no longer deletes or solidifies: it welds (0.8 mm), pushes cloth out of the skin with a smooth spatial displacement field (both layers move together), then decimates the closed solid to 19k triangles (collapse, triangulated). Result: one closed continuous cape, 4 boundary edges, back-ray check 0 of 648 missed. |
| 3 Faceting | 19k triangles (was 12k from a 5k-face sheet), smooth shading, cape hem and flutes calmed (`--calm`) before decimation. |
| 4 Hem | `--bottom 128` (mid upper arm), `--flare 1.15` clears the arms, `calm_hem` smooths flutes and the scalloped edge toward an even flare. |
| 5 Hood fit | No more cutting of the face and neck zone (round 1's blocky opening); the hood is uniformly scaled to the cape height (hood 12% smaller, still 0 mm wall inside the head), cape clearance 2.5 cm below the neck (`--body-offset`), hood 0.8 cm. |
| 6 Texture | Smart-project unwrap (constant texel density), bake of the Forge colour, graded to mean sRGB (0.34, 0.24, 0.155) dark brown wool, dark island rims removed, 10 px weave and slub noise added. |
| 7 Walking | Skin weights are averaged over the cloth six times, so the cape moves as one piece. |

Findings: the cleanup is now about 40 s; clay and section sheets use `Render-ClaySheet.py`. The cape still hangs on the shoulders and upper arms only (hem about mid upper arm), so the lower back shows skin below it when running: that is the designed length, not a tear.
