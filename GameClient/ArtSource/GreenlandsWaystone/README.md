# Greenlands waystone prototype

Optional arrival-landmark art study, not imported into Unreal and not a biome or gameplay definition. The E1 graybox map and its collision remain the acceptance fixture.

`Waystone-Turnaround.png` was generated with the built-in imagegen tool using `docs/Game-Design/concept-art/Concept.png` as a mood/material reference. The exact prompt is in `Waystone-Turnaround.prompt.txt`. This is newly generated concept art, not a reconstruction or an approved production asset. The back/front ornament and simple silhouette guide the authored geometry; the model does not reproduce all depicted weathering.

`GreenlandsWaystone.blend` is the editable Blender5.2.2 source, including a preview studio. `SM_GreenlandsWaystone.fbx` and `GreenlandsWaystone.glb` contain only the model, with four flat prototype materials and one UV layer. Ground origin, metres, Z up, front -Y; height2.4m,1140 triangles. `Waystone-Preview.png` is an inspected Blender render. Both exchange files were reimported into empty Blender scenes and retained one mesh, a UV layer and height2.4m. This is not Unreal import/cook or visual runtime acceptance.

Regenerate geometry/export/preview into a **new** output folder from the repository root:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' --background --factory-startup --python-exit-code 1 --python tools/Create-E1Waystone.py -- --output "$PWD\Artifacts\Art\Waystone-Review"
```

The generator refuses existing output filenames. It does not regenerate the imagegen reference, import into Unreal, modify the map, or contact a service. Geometry invariants are reproducible; native/exchange file bytes are not promised identical across Blender versions.

Before promotion: artist silhouette/material review, PBR textures/bake, UV texel-density/lightmap review, LOD and collision selection, Unreal scale/material/import/cook verification, artifact/provenance approval under DEC-07. No RealityScan reconstruction was attempted: a generated turnaround is not a set of overlapping photographs. No additional paid tool is required for this prototype.
