# Player character

Owner decisions (2026-10-23):

- **Bodies:** MetaHuman (UE 5.8 MetaHuman Creator, scripted). There is one base body per gender, shaped by MetaHuman's parametric measurements: Height, Masculine/Feminine, Fat and Muscularity. These match the character creator's height and build controls.
- **Gear:** each piece is made once and fitted to every body with Mutable (Customizable Objects). Height comes from bone scale, build from baked body blend shapes. Fitted chest armour may get separate male and female fits.
- **Look:** realistic, age 30s. `reference-starter-male.webp` is the owner's reference sheet: linen tunic with a stitched neckline and rolled sleeves, dark leggings, leather belt with a pouch, wrist wraps and leather boots. It also includes skin tone swatches.
- **Faces (owner, 2026-10-23):** heroes of the time, with Anglo-Saxon and early-medieval Northern European looks. Think weathered, strong faces; long or tousled hair; full beards, mustaches and braids. See the owner's painting reference of three warriors in hoods, mail and cloaks. Stock MetaHuman presets that don't fit the setting are out. Cast (owner): every face per gender is Caucasian except one, who is a light-skinned Black hero. Custom faces come from generated front-facing hero portraits: Codex portrait, then a Pixal3D head mesh, then MetaHuman `conform_to_target_meshes` with portrait landmarks. Hair, beard and mustache come from MetaHuman's groom library (Beard_L_Full, Beard_L_Messy, Mustache_L_*, Hair_L_*, braids).
- **New character:** tunic and leggings only. These are the base "underclothes" layer.
- **Starter set:** belt and pouch, wrist wraps and boots. The first quest (or a short chain) hands these out as rewards, with the last piece at the end of the chain.

Pipeline: MetaHuman base bodies in game first, then one gear piece end to end (hide jerkin), then the rest. Gear meshes come from Frontier Asset Forge (TRELLIS.2 / Pixal3D) and get the body's skin weights copied on in Blender.

`presets/` holds rendered contact sheets of the MetaHuman preset faces and hairstyles, used to pick the base look.
