# GPT-6.1-Sol visual review — Model-v010

Review scope: direct catalog packages, manifests, Model-v010 previews, materials and exported meshes. This is quality evidence only; it does not approve GEO-01/GEO-02 or Unreal readiness.

| Stable ID | Decision | Evidence and required repair |
|---|---|---|
| `BLD-WT-CellFull-Ornamented-01` | Fail | Front color motif is an X with extra diagonal strokes rather than nested diamonds and a filled center. Preview shows the back border and hides +Y. Border reaches the silhouette instead of being inset. GLB material assignment correctly localizes color to +Y/-Y; normal and roughness bytes are identical within v010. However all four shared base maps differ from the v009 plain WT master. Final attempt must use exact master map bytes and modify color only.
| `BLD-RT-RoofSlopeFace-01` | Fail | Preview hides the inclined face, preventing scale/grain review. Visible triangle has overly regular corrugation and the vertical end has abrupt grain transition. Inclined 0.707m run maps into a 0.5m tile, causing 1.414x UV stretch; physical normal derivative used the wrong V texel size.
| `BLD-SM-RoofSlopeFace-01` | Fail | Preview hides the inclined face. Stone is dark gray-brown with exaggerated sinusoidal joints. Direct clean view is continuous pale stone; concept sheet supports courses, but does not support this color or exaggerated joint motion.

## Attempt disposition

Model-v010 is revision D's initial attempt 1/3 after v006. It passes fresh source/GLB/FBX and connected-island/loose-vertex checks. Visual quality and wrap checks fail; no asset is complete. v011 is the single remaining repair (attempt 3/3) for the three stable IDs. No more material attempt is permitted after v011; preserve exact blocker if it still fails.

All outputs remain prototype-subset. Canonical carriers lack approved GEO-01 manifests and applicable GEO-02 assembly evidence; Unreal 5.8 import, collision/LOD, contacts, and performance gates are open.
