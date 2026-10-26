# Voidreign material findings

The current actual-mesh walk source retains one active Material_0 surface shader. Base color is the packed4096x4096 sRGB opposite-side detail atlas. Metallic and roughness use a packed4096x4096 Non-Color map, Blue to Metallic and Green to Roughness. Normal is unlinked. Emission strength is0; the red eye/crack appearance currently comes from color, not an emissive material. There is no linked coat or opacity mask. This is a dependency/channel audit, not a production visual pass.

Next material work: define and verify head bone, saddle leather, metal and eye masks; inspect neutral and opposing-light closeups; author missing relief/normal detail from approved geometry or independent height sources, and an eye-only emission treatment where intended. Do not treat painted lighting or red albedo as physical relief/emission, or expand soft4K detail by merely increasing atlas size. Preserve existing roughness/metal channels and inspect oblique/hidden-side consistency. Head/saddle albedo refinements already exist; this audit does not regenerate or replace them.

Producer/profile/source hashes are in material-audit.json and local copies. No GPU, dependency, source or material modifications were made.
