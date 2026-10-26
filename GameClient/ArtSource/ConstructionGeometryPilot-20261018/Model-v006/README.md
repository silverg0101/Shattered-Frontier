# Construction surface material models v001

Six materialized Blender/GLB/FBX prototypes apply appearance-only PBR skins to the exact `cell-full` or `slope-1to1` meshes from Model-v001. Existing reference images guide color and surface style; they are not used as texture atlases. Base color, roughness, and shallow height maps are supplied at 1024 square. Texture relief is a shader bump only and does not change the canonical mesh, silhouette, dimensions, or contacts.

Status remains `prototype-subset`, `acceptance_claim=false`, `engine_ready=false`. Open gates include fresh export reimport checks, Unreal import/material validation, assembled seam review, and LOD/collision/performance evidence. See batch-manifest.json for per-file SHA-256 and reference provenance.
