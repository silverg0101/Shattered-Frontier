# Silverbrook water, swim, bridge ends, harvest swap, hay (2026-10-26)

Owner playtest defects. Decision and route: [WORLD-silverbrook-water-and-swim](../../../decisions/WORLD-silverbrook-water-and-swim.md). Captures (git-ignored) are in `C:\Projects\Shattered-Frontier\Artifacts\Evidence\silverbrook-water\` as `before-<view>.png`, `after-<view>.png` and `compare\<view>-compare.png`.

## Run order
1. `NatureFamilies/import_ground.py --water-only` (river and pond materials), `tools/Build-SwimAnim.py` (swim clip), Blender `build_prophaybales.py` then `import_silverbrook.py PropHayBales --package-only` (hay), then `Greenlands/water_fx.py` (patches the built map: materials, underwater volumes, abutment fill, props standing in water). A rebuild gets the same through `build_silverbrook.py` and `silverbrook_props.py`.
2. `capture_water.py` (`-game`, `-WaterTag=before|after`; `-HarvestArtShared -NoSwimPose` reproduce the pre-fix code paths; the before set was taken from the committed HEAD content).
3. Checks: `tools/Test-SwimPose.ps1` PASS (and fails with `-Extra -NoSwimPose`); `ShatteredFrontier.World.SilverbrookIntegrity` passes.

## Causes found
- Bridge-end patches: the abutment kit's earth fill (`SB_RiverAbutment_Grass`, leafy_grass tinted bright) between the wing walls; it now takes the deck paving (`water_fx.abutment_earth`).
- A pale box floating beside the main bridge: a hay cart that the street dressing stood on the river bed (props were not checked against water); `wet_fn` now rejects wet spots and the patch removed 3 props.
- Water vanished from below: the old strip material was one-sided.
- Hay: noise-textured cut-end scan on every face plus 330 stiff stalk boxes; rebuilt as rounded, flaked bales with a side-straw scan, twine tubes and short drooping strands.
- Harvest swap: the zone node always showed MightyOak/HarvestRock art. Nodes now show the dressing instance's own mesh.

## Review
Three fresh Opus rounds (`REVIEW.md`, `REVIEW-r2.md`, `REVIEW-r3.md`); round 3: 6 PASS, 8 FAIL. Handed to the owner with the open list in `REVIEW-r3.md` (swimmer reads as a pale slab and rides high, a hard underwater seam, hay still saturated in direct sun, water strips step under bridge arches, side-b ramp paler than the road).
