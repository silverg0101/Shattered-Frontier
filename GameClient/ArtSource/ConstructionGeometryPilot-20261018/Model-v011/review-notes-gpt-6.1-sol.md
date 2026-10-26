# GPT-6.1-Sol visual review — Model-v011

| Stable ID | Status | Findings |
|---|---|---|
| `BLD-WT-CellFull-Ornamented-01` | Failed; attempt cap exhausted 3/3 | No render, source, GLB or FBX. Embedded build script raised `NameError: math is not defined` at line 17 because slope-aware UV logic incorrectly executed on the cell-full cube's top face. Maps exist, and motif itself now renders as nested diamonds with a filled center, but there is no v011 reimport/localization evidence. |
| `BLD-RT-RoofSlopeFace-01` | Failed visual acceptance; attempt cap exhausted 3/3 | Source, GLB/FBX, connected-island/loose-vertex checks pass; texture wrap passes. Preview camera is below the inclined plane z=x and shows the high end/triangular side, hiding the target slope face. |
| `BLD-SM-RoofSlopeFace-01` | Failed visual and wrap acceptance; attempt cap exhausted 3/3 | Source, GLB/FBX and island checks pass. Preview still hides the inclined face; appearance reads blurred. Basecolor and height fail X wrap checks. |

Slope UV0 covers V=1.414 across the 0.707m inclined run, which gives 2048 texels/m. The initially generated `texture-source.json` reported 1448.155 px/m for V; a post-build reconciliation corrected it to 2048 px/m without changing maps or mesh. The original mismatch is retained in each check report. The target slope camera must be positioned above the plane (z offset greater than x offset for this carrier) so the inclined face is visible. The UV branch also needs a shape guard so it only runs on the slope carrier.

All three items consumed the v006 initial attempt plus v010 and v011 repairs. Do not retry these material stages without a new authorized attempt budget. All outputs remain prototype-subset. GEO-01/GEO-02, Unreal 5.8, collision/LOD, assembly contacts and performance gates are open.
