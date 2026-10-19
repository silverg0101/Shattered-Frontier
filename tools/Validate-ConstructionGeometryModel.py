"""Fresh-process source check for one saved construction geometry prototype.

Run with Blender opening a saved .blend before this script:
  blender --background <asset.blend> --python tools/Validate-ConstructionGeometryModel.py
"""
from __future__ import annotations

import json
from pathlib import Path

import bpy

Q = 65536
CELL_M = 0.5
source = Path(bpy.data.filepath).resolve()
if not source.is_file():
    raise FileNotFoundError("Blender did not open a saved source file")
mesh_objects = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
asset_objects = [obj for obj in mesh_objects if obj.get("asset_status") == "prototype-subset; not approved for production"]
staging_meshes = [obj for obj in mesh_objects if obj.name.startswith("RENDER_ONLY_")]
nonstaging_nonassets = [obj for obj in mesh_objects if obj not in asset_objects and obj not in staging_meshes]
if len(asset_objects) != 1 or nonstaging_nonassets:
    raise RuntimeError(f"Expected one prototype mesh and only named render staging meshes; asset={len(asset_objects)}, other={len(nonstaging_nonassets)}")
obj = asset_objects[0]
unexpected_objects = [item.name for item in bpy.context.scene.objects
                      if item is not obj and not item.name.startswith("RENDER_ONLY_")]
if unexpected_objects:
    raise RuntimeError(f"Unexpected non-staging objects in source: {unexpected_objects}")
key = obj.get("geometry_key")
if not key:
    raise RuntimeError("Mesh has no canonical geometry key")
payload_path = source.parent.parent / "geometry-source" / f"{key}.json"
if not payload_path.is_file():
    # The source can also be validated in a staged Artifacts output folder.
    payload_path = source.parent.parent.parent / "mesh-prototypes-20261018-v001" / f"{key}.json"
payload = json.loads(payload_path.read_text(encoding="utf-8"))
expected = [
    (max(vertex[axis] for vertex in payload["vertices"]) - min(vertex[axis] for vertex in payload["vertices"]))
    / Q * CELL_M
    for axis in range(3)
]
points = [obj.matrix_world @ vertex.co for vertex in obj.data.vertices]
actual = [max(p[axis] for p in points) - min(p[axis] for p in points) for axis in range(3)]
error = max(abs(actual[i] - expected[i]) for i in range(3))
adjacency = [set() for _ in obj.data.vertices]
for edge in obj.data.edges:
    a, b = edge.vertices
    adjacency[a].add(b)
    adjacency[b].add(a)
remaining = set(range(len(adjacency)))
components = []
while remaining:
    root = remaining.pop()
    stack = [root]
    component = {root}
    while stack:
        current = stack.pop()
        for neighbor in adjacency[current] & remaining:
            remaining.remove(neighbor)
            component.add(neighbor)
            stack.append(neighbor)
    components.append(component)
loose_vertices = sum(1 for neighbors in adjacency if not neighbors)
result = {
    "status": "passed" if error <= 0.000001 else "failed",
    "source_file": source.name,
    "blender_version": bpy.app.version_string,
    "geometry_key": key,
    "canonical_payload_sha256": obj.get("canonical_payload_sha256"),
    "asset_mesh_object_count": len(asset_objects),
    "render_staging_mesh_object_count": len(staging_meshes),
    "render_staging_object_count": sum(1 for item in bpy.context.scene.objects if item.name.startswith("RENDER_ONLY_")),
    "vertices": len(obj.data.vertices),
    "polygons": len(obj.data.polygons),
    "connected_vertex_components": len(components),
    "loose_vertex_count": loose_vertices,
    "dimensions_m": actual,
    "expected_dimensions_m": expected,
    "maximum_axis_error_m": error,
    "staging_collection_present": "STAGING_RENDER_ONLY" in bpy.data.collections,
    "acceptance_claim": False,
}
report = source.parent / "source-reopen-check.json"
report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps(result, indent=2))
if result["status"] != "passed":
    raise RuntimeError("Saved source does not match canonical dimensions")
