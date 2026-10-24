# Player faces: skin tone and hair colour, round 1

Concept: owner brief (early-medieval northern Europe; pale to light brown; red = very pale freckled, blonde = fair, brunette = light-medium, dark brown = light brown) plus `GameClient/ArtSource/Characters/Player/presets/hair-refs/female_medieval_hair_sheet.png`.
Evidence: `heroes/built/faces_all.png`, `heroes/built/<Name>_front.png`, `Saved/Screenshots/WindowsEditor/outfit_Godric_front.png`, `outfit_Ebba_front.png`.
Scope: skin tone, hair colour and pairing, makeup, face-to-body match. Hairstyle shape, identity, clothes and lighting not judged.

Overall: 1 PASS, 8 FAIL.

## Godric (red) — FAIL
Hair and beard read as saturated ginger-orange, and in sunlight they turn bright traffic-cone orange. The reference is a deep copper/auburn. Skin is pinkish-pale, which is the right range, but it has no freckles. In-game, the bare feet are pinker and paler than the face.

## Aelfwyn (red) — FAIL
Hair is orange-tan with yellow streaks, not deep copper. Skin is pale (OK) but has no freckles. Lids and cheeks have a pink-red flush, and with the dark brown eyes it reads as blush and liner.

## Edwin (blonde) — FAIL
Hair, brows and beard are an ash/platinum near-white with grey-green beard tips, so he reads bleached or aged rather than naturally fair. Skin is a warm tan-orange, darker than "fair", and the neck is more orange than the face.

## Hilda (blonde) — FAIL
Hair is pale cream/platinum. Brows are almost white, which looks unnatural against dark lids. Skin is a warm tan-peach, too dark for "fair". There is dark smoky shading around the eyes and the lids look made up.

## Orlando (brunette) — PASS (minor)
Mid brown hair and beard, light-medium skin: the pairing is correct. Minor: there is a blonde streak on the left side of the hair, and the forehead and nose have an orange sheen.

## Wulfric (brunette) — FAIL (minor)
Hair is a correct mid-dark brown, but the beard and moustache are a lighter ash-blonde-brown that doesn't match the hair. Skin is light-medium (OK).

## Eadgyth (brunette) — FAIL
Hair is a believable mid brown with golden highlights. Skin is a saturated orange-tan, closer to "light brown" than "light-medium", so it overlaps the dark-brown group. Lips are red-pink and the lids are darkened (makeup).

## Osric (dark brown) — FAIL
Skin is medium brown, past the "nobody darker than light brown" cap. Hair, brows and beard are black, not dark brown.

## Ebba (dark brown) — FAIL
In the studio render, skin is medium warm brown, past the cap. Hair is black, not dark brown. In-game sunlight lifts the face to light brown and it matches the hands and feet, but the braids pick up copper highlights. Lips are dark and full-tinted, which reads as lipstick.

## Fix list (most visible first)
1. **Godric:** hair and beard are bright orange in-game. Darken and desaturate them to deep copper-auburn, matching ref tiles 2 and 6 (roughly sRGB #7A3A1E base, #A0522D highlights). ART, Engineer (MetaHuman Creator hair colour)
2. **Osric:** skin is too dark. Lighten to light brown (olive-tan, no darker than Eadgyth's current tone), and change hair, brows and beard from black to very dark brown (#3B2A20). ART, Engineer
3. **Ebba:** skin is too dark. Lighten to light brown, the same band as Osric. Make the hair very dark brown, not black, and remove the copper sheen. Neutralise the lip tint to natural skin-toned lips. ART, Engineer
4. **Hilda:** hair is too platinum. Make it a warmer natural flaxen/golden blonde like ref tile 3, with brows a few shades darker than the hair. Lighten the skin to fair with a slight pink undertone, and remove the dark eye shading. ART, Engineer
5. **Edwin:** hair and beard are ash-white. Make them warm golden/flaxen blonde, with the beard the same hue and no grey tips. Lighten the skin from tan to fair, and match the neck to the face. ART, Engineer
6. **Aelfwyn:** hair is orange. Make it deep copper-auburn (the same family as Godric). Add light freckles across the nose and cheeks, and remove the pink lid and cheek flush. ART, Engineer
7. **Godric:** add freckles. Check that the body skin tone equals the face skin tone, since the feet render pinker and paler in sunlight. ART, Engineer
8. **Eadgyth:** desaturate the skin to light-medium (less orange, similar to Orlando), and remove the lip colour and lid darkening. ART, Engineer
9. **Wulfric:** match the beard and moustache colour to his mid-dark brown hair. ART, Engineer
10. **Orlando:** remove the blonde streak on the left side, and tone down the orange sheen on the forehead and nose. ART, Engineer
