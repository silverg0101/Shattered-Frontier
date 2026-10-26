# Oak and iron geometry boundary review — 2026-10-19

Read-only CPU review of the current source meshes. No mesh, texture, inference, or attempt state was changed. Both assets retain one geometry repair attempt, but no safe selector is ready to spend it.

## Iron-bearing rock

Pinned inputs:

- Blender source SHA-256: `7ebb7138a8ad8f1fb2c191db7ccf7aa4a53ffcce5a9c7df22db2ea945b6b9747`
- Mesh SHA-256: `ab363bc7cc378415deb5c89eaf481265aeacb99fc4f5b6feae3dd686e56ae5f3`
- Component selection NPZ SHA-256: `7a5aab2f81e5438eef4df6a5db645c259d6ac1865a5a9ae4810eda2f84d19392`

White ribbon and skirt defects belong to mixed component `C0000`. CPU BVH first-hit probes from the saved Beauty camera map them to the following paired front/opposed triangle rows (rows are NPZ/source loop-triangle indices, not Blender polygon IDs):

| Feature | Beauty pixel | Front ↔ opposed rows | Inventory-space hit XYZ |
|---|---:|---:|---|
| Upper ribbon | 707,327 | 81185 ↔ 84228 | 0.234857, 0.016742, 0.205088 |
| Upper-left ribbon | 652,370 | 80090 ↔ 93160 | 0.192663, −0.031800, 0.153990 |
| Central ribbon | 690,412 | 56608 ↔ 56702 | 0.255717, −0.032164, 0.116519 |
| Middle ribbon | 693,490 | 20406 ↔ 20322 | 0.302362, −0.073860, 0.040532 |
| Lower-right ribbon | 723,584 | 16221 ↔ 16677 | 0.323480, −0.045486, −0.075764 |
| Lower narrow ribbon | 697,591 | 13511 ↔ 19123 | 0.313143, −0.078042, −0.079477 |
| Bottom ribbon | 680,657 | 13219 ↔ 13319 | 0.319164, −0.112109, −0.151310 |
| Skirt | 768,698 | 9040 ↔ 264 | 0.379342, −0.027111, −0.206743 |
| Upper ribbon | 722,279 | 96546 ↔ 97685 | 0.182005, 0.094339, 0.235310 |

Ordinary brown-rock/ore controls include rows `83249, 48172, 40524, 39639, 13557, 39915, 94456, 71168, 13450`. Defect and control probes have opposed surfaces about 0.00163–0.00250 native units apart. This source behaves like a thin shell in these regions; brightness, thickness, spatial boxes, or component removal cannot safely separate the artifacts. Exact side boundaries where the white ribbons join valid rock/ore remain unclassified. Author closed face boundaries and the intended replacement stone/vein surface before using the remaining geometry attempt.

## Oak

Pinned inputs:

- Blender source SHA-256: `8c87e8090f0a3b3ff2c7589aeeed5da9ae52ef9cc044101826e3542003bca05b`
- Mesh SHA-256: `e883bc0a20079787df37a59bd100aafb41fbf98b5414410cc6a8327f3ed11d3f`
- Component selection NPZ SHA-256: `dce4492bf8618c1b01745b6cc8316e8c9bd865551390c060dab0a99ed593a542`

The remote sheet extension and root flare are continuous in mixed component `C0000`. Sheet diagnostic triangle rows are `9523, 9493, 9502`; these locate the extension but do not define a closed removal boundary. Examined raised root vertices occupy approximately X `[-0.05745, 0.06098]`, Y `[-0.02842, 0.10350]`, Z `[-0.27667, -0.20002]`. A conservative review prism is X `[-0.09, 0.09]`, Y `[-0.05, 0.135]`, Z `[-0.324, -0.20]`; it is only a root-protection aid. Cutting outside it leaves an inner sheet rim; extending a threshold inward risks the root flare. Author a closed root/sheet cut loop and underside closure before using the remaining geometry attempt.

Keep canopy components `C0001–C0009`, especially detached-looking `C0006`, for semantic branch-attachment review. Do not delete by component size or isolation. The remaining `C0010–C0127` stay unclassified.

## Disposition

No safe whole-component or face-level removal is approved by this review. Preserve oak and iron sources, meshes, UVs, textures, and their remaining geometry attempts. These findings narrow future authoring work but do not accept either asset. Material maps, scale/appearance, UE 5.8 import, collision, adaptive scalability, and target-hardware performance remain separate gates.
