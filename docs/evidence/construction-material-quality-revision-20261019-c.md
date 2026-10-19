# Construction material quality revision C — attempt exhaustion

This record corrects the original `Model-v009/batch-manifest.json` false success claims. The generated Blender script raised `IndentationError: unexpected indent` at the normal-map node; Blender returned exit code 0. Each of the three per-asset directories contains only generated maps, script, source metadata, log, and a failure report. There is no saved Blender source, render, GLB, or FBX. Corrected per-file inventories and attempt history are in Model-v009.

All three material stages have used their initial attempt plus two repairs (v007, v008, v009) and are now `needs-review` with the stage cap exhausted. Do not restart these stable IDs under the same attempt budget. A future authorized budget must use a repaired generator that compiles the embedded script and checks for tracebacks and required outputs, regardless of Blender's process exit code.

Independent v009 map review found no material pass. Timber knots were nearly invisible, long fissures dominated rough timber, and height scaling left weak normals. The worked-timber UV branch reused rough-timber orientation. Stone courses remained too regular and mortar shoulders hard. Wrap checks also found significant discontinuities: RT height X seam mean/p99 1.931/20.669 versus interior 0.485/4.992; SM basecolor Y 2.670 versus 0.567; SM height X p99 59.923 versus interior 1.257. These values are normalized to an 8-bit comparison scale; they are defects, not acceptance results.

`BLD-RT-CellFull-01`, `BLD-SM-CellFull-01`, and `BLD-WT-CellFull-01` remain incomplete. GEO-01/GEO-02, Unreal 5.8 import, assembly, collision/LOD, and performance gates remain open independently of this material-stage failure.
