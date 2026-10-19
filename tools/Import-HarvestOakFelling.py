"""Build collision on a NEW copy of the accepted oak's upper mesh using UE's convex decomposition.

Run in the full UE5.8 editor with -ExecutePythonScript (not the Python commandlet). Never edits the original or overwrites a destination.
Generated hull data is diagnostic; Blender overlays and actual falling contact
must be reviewed before accepting this source for gameplay.
"""
import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = "/Game/Frontier/Gameplay/Harvest/MightyOakV1/Meshes/SM_MightyOak_UpperStructure"
DESTINATION = "/Game/Frontier/Gameplay/Harvest/MightyOakFallV1/SM_MightyOak_FallCollision"
ARTIFACT = ROOT / "Artifacts/HarvestArt/engine-tree-collision-01"
SOURCE_FBX = ROOT / "GameClient/ArtSource/HarvestOak/MightyOak-v1/exports/SM_MightyOak_UpperStructure.fbx"
HULL_COUNT = 48
MAX_HULL_VERTICES = 64
VOXEL_PRECISION = 1_000_000


def vector(v):
    return [float(v.x), float(v.y), float(v.z)]


def main():
    library = unreal.EditorAssetLibrary
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem is None:
        raise RuntimeError("StaticMeshEditorSubsystem requires the full editor, not -run=pythonscript.")
    if library.does_directory_exist("/Game/Frontier/Gameplay/Harvest/MightyOakFallV1"):
        raise RuntimeError("Felling destination already exists; refusing overwrite.")
    source = library.load_asset(SOURCE)
    if not isinstance(source, unreal.StaticMesh) or not SOURCE_FBX.is_file():
        raise RuntimeError("Accepted MightyOak upper source is missing.")
    source_collision_count = subsystem.get_convex_collision_count(source)
    mesh = library.duplicate_asset(SOURCE, DESTINATION)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Collision-source duplication failed.")
    if mesh.get_editor_property("body_setup") == source.get_editor_property("body_setup"):
        raise RuntimeError("Duplicate shares original BodySetup; no collision mutation attempted.")
    if not subsystem.set_convex_decomposition_collisions(mesh, HULL_COUNT, MAX_HULL_VERTICES, VOXEL_PRECISION):
        raise RuntimeError("Engine convex decomposition failed.")
    count = subsystem.get_convex_collision_count(mesh)
    if not 1 <= count <= HULL_COUNT:
        raise RuntimeError("Generated convex count outside the declared content ceiling.")
    if subsystem.get_convex_collision_count(source) != source_collision_count:
        raise RuntimeError("Original collision changed; original source must not be saved.")
    if not library.save_loaded_asset(mesh):
        raise RuntimeError("Could not save new collision mesh.")
    bounds = mesh.get_bounding_box()
    manifest = {
        "schema": "harvest-oak-engine-collision-v1",
        "source_asset": SOURCE,
        "source_fbx_sha256": hashlib.sha256(SOURCE_FBX.read_bytes()).hexdigest(),
        "destination": DESTINATION,
        "method": "UE5.8 StaticMeshEditorSubsystem.set_convex_decomposition_collisions",
        "maximum_hulls": HULL_COUNT,
        "maximum_vertices_per_hull": MAX_HULL_VERTICES,
        "voxel_precision": VOXEL_PRECISION,
        "convex_count": count,
        "source_collision_unchanged": True,
        "bounds_cm": {"min": vector(bounds.min), "max": vector(bounds.max)},
        "visual_acceptance": "pending",
        "hulls": [],
    }
    # Reflection is editor-version dependent. A failed export is explicitly not
    # a geometry review; the saved candidate remains available for native audit.
    try:
        body = mesh.get_editor_property("body_setup")
        aggregate = body.get_editor_property("agg_geom")
        for hull in aggregate.get_editor_property("convex_elems"):
            transform = hull.get_editor_property("transform")
            vertices = [vector(transform.transform_location(v)) for v in hull.get_editor_property("vertex_data")]
            indices = list(hull.get_editor_property("index_data"))
            if not 4 <= len(vertices) <= MAX_HULL_VERTICES or len(indices) % 3:
                raise RuntimeError("Invalid exported hull topology.")
            manifest["hulls"].append({"vertices_cm": vertices, "triangle_indices": indices})
        if len(manifest["hulls"]) != count:
            raise RuntimeError("Exported hull count mismatch.")
        manifest["hull_export"] = "complete"
    except Exception as error:
        manifest["hulls"] = []
        manifest["hull_export"] = "unavailable"
        manifest["export_error"] = str(error)
        unreal.log_warning("HARVEST_TREE_COLLISION_EXPORT_UNAVAILABLE " + str(error))
    ARTIFACT.mkdir(parents=True, exist_ok=True)
    (ARTIFACT / "collision.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    unreal.log("HARVEST_TREE_COLLISION_CANDIDATE_COMPLETE hulls=" + str(count) + " export=" + manifest["hull_export"])


try:
    main()
finally:
    unreal.SystemLibrary.quit_editor()
