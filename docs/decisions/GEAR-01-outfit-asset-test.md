# GEAR-01: UE 5.8 MetaHuman outfit assets for player garments

Date: 2026-10-24. Status: test complete (works), recommendation below; visuals not yet reviewed or signed off.

Question: can a Chaos Outfit Asset, fitted automatically to each MetaHuman body, replace hand fitting a garment per body (the current Blender + Mutable path)? Test garment: the starter tunic and leggings (`SKM_Tunic_Leggings`).

## Answers

1. **What UE 5.8 offers, scriptable?** Yes, end to end from Python. Plugins ChaosOutfitAsset (beta; enabled through MetaHumanCharacter), ChaosClothAsset, Dataflow, MeshResizing (RBF). Chain: skeletal mesh -> Cloth Asset (Dataflow `SkeletalMeshImport` -> `ClothAssetTerminal`) -> Outfit Asset (template `/ChaosOutfitAsset/MakeResizableOutfitTemplate`, variable `SizedOutfitSource`: garment + source body + size name) -> MetaHuman Wardrobe Item (principal asset = outfit) -> added to the character's Outfits slot -> MetaHuman build fits the outfit to that character's merged head+body mesh. Script: `tools/Create-PlayerOutfitAsset.py` (no argument = author; `build <ids>`).
2. **One reference body for all faces; fit quality?** One size, sourced on Orlando's bare male full body, fitted both tested faces with no per-body work. Edwin (male): shoulders, arms, hips and legs follow the body. Hilda (female, the hard case): bust shaped, shoulders narrowed, hips followed, straight mid-thigh skirt, no skin poke-through. The neckline is the source garment's old wide, jagged scoop on both; that is garment authoring, not fitting. The other seven faces were not rebuilt (stopped once the question was answered).
3. **Skin under the garment?** The wardrobe item's body hidden-face map (`T_Tunic_BodyHide`, black = hide) culls body triangles under the garment at build time. It is rasterised in body UV0 (UDIM 1002, so `u % 1`) from the same patch `Build-GarmentBodyClip.py` computes (`--mask`). No poke-through seen on Edwin or Hilda. A head map was skipped (exported head OBJs have no UVs); add one if the neckline lets the chest bib show.
4. **Mutable; server and packaged builds?** Coexists: the test path sits behind the non-shipping `-SFOutfitAssetTest` switch, which loads faces from `OutfitAsset/Built` and skips the Mutable outfit; `CO_PlayerOutfit` wiring is untouched. The build output is a plain skeletal mesh component in the face's `BP_MHC` (outfits cannot be cooked yet; the pipeline bakes them to `USkeletalMesh` at build), so packaged clients and the dedicated server need no outfit runtime. It can replace Mutable for the single fitted garment; Mutable is still the option if runtime gear mixing is wanted.
5. **Cost per new garment?** Author the garment once on the reference body (the current Blender step, minus per-body refits), run `author` (minutes), then ~4 min machine time per face build. No per-body hand work.

## Bug found and fixed

The first outfit asset was empty (6.5 KB; built outfit meshes 13 KB; bodies rendered with masked skin and no cloth; broken run not kept). Setting the template and `SizedOutfitSource` in one JSON property write left `OverriddenVariableGuids` empty, so the template's empty default won. Fix in the script: set the template first, then `import_text` the variable override including the variable GUID, check the GUID and `CA_Tunic` appear in the exported text, then regenerate. Result: `OA_Tunic` 22.7 MB; Edwin's built outfit 1 LOD, 2 sections (linen, leggings), 11,177 vertices.

## Sizes rule

Each size in an outfit asset must be the exact same garment representation (same mesh), sourced on a different body; the build picks the closest size per character. Add a size only when fitting from one size is not acceptable (Epic's default garment ships nine). `SKM_Tunic_Leggings_F` has a different mesh and cut, so it cannot be a second size as is; a female size would be the same garment refitted to `HildaBare`, or a separate outfit and wardrobe item. Not needed for this tunic.

## Recommendation

Author each garment once on the reference body and fit it to every face through an outfit asset instead of per-body hand fitting. The neckline (high round band, short V per the concept) becomes a one-time garment-authoring fix on the source mesh, carried to all faces by the fit. Next: fix the neckline, rebuild all nine faces, visual review, then decide whether to switch the default from Mutable.

## Evidence

- `docs/evidence/player-outfit-asset/gear01-edwin-fixed.jpg` (face, front, three-quarter, side)
- `docs/evidence/player-outfit-asset/gear01-hilda.jpg`
- Rebuild: `python tools/Run-InEditor.py tools/Create-PlayerOutfitAsset.py` (no argument = author) then the same with `build <ids>` (needs the git-ignored built faces and regenerates `Bodies/MHC_OrlandoBare_FullBody`), capture with `-SFOutfitAssetTest`, sheet with `tools/Make-OutfitAssetSheet.py`.
