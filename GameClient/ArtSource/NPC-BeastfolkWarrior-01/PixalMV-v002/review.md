# BeastfolkWarrior PixalMV v002 review

**Verdict:** Accept the generated mesh and texture as a prototype import candidate. The rig is mechanically exportable, but animation deformation still needs art review before gameplay use.

- Source: `source/BeastfolkWarrior.glb` (SHA-256 `1f2abba5e3716909c14b4769b4f4c5dbbaea197bb8d1202d4e8430397a22aa35`); normalized 1.85 m asset: `BeastfolkWarrior.glb`. `review-sheet.png` uses neutral studio light and correctly labels front, side, back, and front three-quarter. The older renderer's Front/Rear filenames face the opposite way.
- Shape and scale: one mesh, 285,885 triangles, 1.444 × 0.620 × 1.850 m. The silhouette, left pauldron, crossed straps, right pouch, ears, claws, and shin wraps follow `Views1024-v002/raw/`. Compared with v001, the coat, fur, face, and metal read more clearly and the 2048 px material has stronger color/detail. The input front view has darker fur and slightly different strap/pauldron details, so this remains an interpretation.
- Topology: the saved mesh has 18,347 disconnected islands, mostly sculpted fur/detail shells; UV and two 2048 px image dependencies survive saved-source reopen and GLB/FBX reimport. This is not animation-friendly production topology.
- Rig: `Build-NpcAutoRig.py` completed with 49,999 triangles, 0 empty deform groups, 150 unweighted vertices repaired, mannequin Idle/Walk clips, and an FBX. `AutoRig-v002/rig-report.json` deliberately reports `rig-needs-review`. Walk frames show coat/skirt stretching and some fur/foot deformation; contacts and fingers are not solved. The FBX is ready for an **editor import trial**, not approved for a playable NPC.

No Unreal import was performed.
