"""Import the fixed Mighty Oak felling collision candidate into a fresh UE5.8 folder.

Run with the full editor and -ExecutePythonScript. The script saves only the new
collision mesh, writes evidence under Artifacts/HarvestArt, then quits the editor.
"""
import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/CollisionCandidate-v1"
SOURCE_FBX = SOURCE_DIR / "SM_MightyOak_FallCollision.fbx"
SOURCE_MANIFEST = SOURCE_DIR / "manifest.json"
SOURCE_README = SOURCE_DIR / "README.md"
CONTENT_ROOT = "/Game/Frontier/Gameplay/Harvest/MightyOakFallV2"
BASE_MESH = "SM_MightyOak_FallCollision"
ASSET_PATH = CONTENT_ROOT + "/" + BASE_MESH
EVIDENCE_DIR = ROOT / "Artifacts/HarvestArt/mighty-felling-v2-import-01"
EVIDENCE_PATH = EVIDENCE_DIR / "import.json"
ACCEPTED_UPPER = "/Game/Frontier/Gameplay/Harvest/MightyOakV1/Meshes/SM_MightyOak_UpperStructure"
ACCEPTED_UPPER_FILE = ROOT / "GameClient/Content/Frontier/Gameplay/Harvest/MightyOakV1/Meshes/SM_MightyOak_UpperStructure.uasset"
ACCEPTED_UPPER_UASSET_SHA256 = "b41e386cd63b086a871874bf2d3c67615df37a14a5ac2a9ffdabe6f65f210ece"
EXPECTED_SOURCE_FBX_SHA256 = "1939fdb4fe70d06583d4fdc3f150afaa6022ec19f92551c915cbb06ac5226d3b"
EXPECTED_HULL_COUNT = 52
SUCCESS_MARKER = "HARVEST_MIGHTY_OAK_FALLING_CANDIDATE_V2_IMPORT_COMPLETE"


def sha256(path):
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(block)
    return hasher.hexdigest()


def require_editor():
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem is None:
        raise RuntimeError("Requires the full Unreal Editor and StaticMeshEditorSubsystem")
    return subsystem


def verify_input_packet():
    for path in (SOURCE_FBX, SOURCE_MANIFEST, SOURCE_README, ACCEPTED_UPPER_FILE):
        if not path.is_file():
            raise FileNotFoundError("Required input is missing: " + str(path))
    manifest = json.loads(SOURCE_MANIFEST.read_text(encoding="utf-8"))
    if manifest.get("schema") != "mighty-oak-felling-collision-candidate-v1":
        raise RuntimeError("Unexpected collision candidate manifest schema")
    if manifest.get("status") != "candidate_for_review_not_runtime_adopted":
        raise RuntimeError("Candidate manifest status is not the expected review state")
    if manifest.get("outputs", {}).get("fbx") != SOURCE_FBX.name:
        raise RuntimeError("Manifest FBX identity does not match the fixed input")
    expected_hash = manifest.get("outputs", {}).get("fbx_sha256")
    if expected_hash != EXPECTED_SOURCE_FBX_SHA256 or sha256(SOURCE_FBX) != expected_hash:
        raise RuntimeError("Candidate FBX hash does not match the fixed manifest hash")
    if manifest.get("construction", {}).get("hull_count") != EXPECTED_HULL_COUNT:
        raise RuntimeError("Candidate manifest does not declare 52 UCX hulls")
    return manifest


def get_convex_count(subsystem, mesh):
    return int(subsystem.get_convex_collision_count(mesh))


def source_snapshot(subsystem):
    if sha256(ACCEPTED_UPPER_FILE) != ACCEPTED_UPPER_UASSET_SHA256:
        raise RuntimeError("Accepted MightyOakV1 upper asset package hash changed")
    source = unreal.EditorAssetLibrary.load_asset(ACCEPTED_UPPER)
    if not isinstance(source, unreal.StaticMesh):
        raise RuntimeError("Accepted MightyOakV1 upper mesh is missing")
    count = get_convex_count(subsystem, source)
    if count != 0:
        raise RuntimeError("Accepted MightyOakV1 upper mesh must retain its original zero convex hulls")
    return {"asset": ACCEPTED_UPPER, "uasset_sha256": sha256(ACCEPTED_UPPER_FILE), "convex_count": count}


def import_task():
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)

    data = options.get_editor_property("static_mesh_import_data")
    settings = {
        "combine_meshes": False,
        "auto_generate_collision": False,
        "one_convex_hull_per_ucx": True,
        "build_nanite": False,
        "bake_pivot_in_vertex": False,
        "transform_vertex_to_absolute": True,
        "convert_scene": True,
        "convert_scene_unit": True,
        "import_uniform_scale": 1.0,
        "import_translation": unreal.Vector(0.0, 0.0, 0.0),
        "import_rotation": unreal.Rotator(0.0, 0.0, 0.0),
    }
    for name, value in settings.items():
        data.set_editor_property(name, value)
    options.set_editor_property("static_mesh_import_data", data)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(SOURCE_FBX))
    task.set_editor_property("destination_path", CONTENT_ROOT)
    task.set_editor_property("destination_name", BASE_MESH)
    task.set_editor_property("options", options)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", False)
    return task


def verify_bounds(mesh, manifest):
    # Compare Unreal to Unreal: FBX conversion changes handedness from Blender.
    # The accepted visual mesh is the runtime attachment's actual coordinate frame.
    source = unreal.EditorAssetLibrary.load_asset(ACCEPTED_UPPER)
    expected = source.get_bounding_box()
    bounds = mesh.get_bounding_box()
    actual_min = [float(bounds.min.x), float(bounds.min.y), float(bounds.min.z)]
    actual_max = [float(bounds.max.x), float(bounds.max.y), float(bounds.max.z)]
    expected_min = [float(expected.min.x), float(expected.min.y), float(expected.min.z)]
    expected_max = [float(expected.max.x), float(expected.max.y), float(expected.max.z)]
    for actual, target in zip(actual_min + actual_max, expected_min + expected_max):
        if abs(actual - target) > 1.0:
            raise RuntimeError("Collision/accepted visual bounds mismatch: " + repr({"actual": [actual_min,actual_max], "accepted": [expected_min,expected_max]}))
    return {"min_cm": actual_min, "max_cm": actual_max, "accepted_min_cm": expected_min, "accepted_max_cm": expected_max}


def main():
    manifest = verify_input_packet()
    subsystem = require_editor()
    library = unreal.EditorAssetLibrary
    if library.does_directory_exist(CONTENT_ROOT) or library.does_asset_exist(CONTENT_ROOT):
        raise RuntimeError("Felling V2 Content destination already exists; refusing overwrite: " + CONTENT_ROOT)
    if EVIDENCE_DIR.exists():
        raise RuntimeError("Felling V2 import evidence path already exists; refusing overwrite: " + str(EVIDENCE_DIR))

    original_before = source_snapshot(subsystem)
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        raise RuntimeError("Cannot configure the legacy FBX importer without an editor world")
    unreal.SystemLibrary.execute_console_command(world, "Interchange.FeatureFlags.Import.FBX 0")
    unreal.log("HARVEST_FBX_INTERCHANGE_DISABLED_FOR_IMPORT_SESSION")

    task = import_task()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported_paths = list(task.get_editor_property("imported_object_paths") or [])
    expected_object_path = ASSET_PATH + "." + BASE_MESH
    if imported_paths != [expected_object_path]:
        raise RuntimeError("FBX import did not produce exactly the requested base mesh: " + repr(imported_paths))
    assets_in_folder = [str(path).split(".", 1)[0] for path in library.list_assets(CONTENT_ROOT, recursive=True, include_folder=False)]
    if assets_in_folder != [ASSET_PATH]:
        raise RuntimeError("Destination contains an unexpected imported asset: " + repr(assets_in_folder))
    mesh = library.load_asset(ASSET_PATH)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Imported base asset is missing or is not a StaticMesh")

    hull_count = get_convex_count(subsystem, mesh)
    if hull_count != EXPECTED_HULL_COUNT:
        raise RuntimeError("Expected exactly 52 imported UCX convex hulls; got " + str(hull_count))
    body = mesh.get_editor_property("body_setup")
    if body is None:
        raise RuntimeError("Imported collision mesh has no BodySetup")
    trace_flag = getattr(unreal.CollisionTraceFlag, "CTF_USE_SIMPLE_AND_COMPLEX", None)
    forbidden_trace_flag = getattr(unreal.CollisionTraceFlag, "CTF_USE_COMPLEX_AS_SIMPLE", None)
    if trace_flag is None or forbidden_trace_flag is None:
        raise RuntimeError("UE Python API does not expose the required collision trace flags")
    body.set_editor_property("collision_trace_flag", trace_flag)
    if body.get_editor_property("collision_trace_flag") == forbidden_trace_flag:
        raise RuntimeError("Complex-as-simple collision is forbidden for this candidate")
    mesh.set_editor_property("allow_cpu_access", False)
    bounds = verify_bounds(mesh, manifest)

    if not library.save_loaded_asset(mesh):
        raise RuntimeError("Could not save the new felling collision mesh")
    new_asset_file = ROOT / "GameClient/Content/Frontier/Gameplay/Harvest/MightyOakFallV2/SM_MightyOak_FallCollision.uasset"
    if not new_asset_file.is_file():
        raise RuntimeError("New felling collision package was not written to disk")
    original_after = source_snapshot(subsystem)
    if original_before != original_after:
        raise RuntimeError("Accepted MightyOakV1 upper mesh changed during candidate import")

    evidence = {
        "schema": "harvest-mighty-oak-felling-candidate-v2-import-v1",
        "status": "imported_candidate_for_review_not_runtime_adopted",
        "source_fbx": str(SOURCE_FBX),
        "source_fbx_sha256": sha256(SOURCE_FBX),
        "source_manifest": str(SOURCE_MANIFEST),
        "source_manifest_sha256": sha256(SOURCE_MANIFEST),
        "source_readme": str(SOURCE_README),
        "destination_asset": mesh.get_path_name(),
        "destination_uasset_sha256": sha256(new_asset_file),
        "imported_asset_count": len(assets_in_folder),
        "convex_hull_count": hull_count,
        "bounds_cm": bounds,
        "import_settings": {
            "import_materials": False,
            "import_textures": False,
            "auto_generate_collision": False,
            "one_convex_hull_per_ucx": True,
            "combine_meshes": False,
            "import_translation": [0.0, 0.0, 0.0],
            "import_rotation": [0.0, 0.0, 0.0],
            "import_uniform_scale": 1.0,
            "allow_cpu_access": False,
            "nanite": False,
            "collision_trace_flag": "CTF_USE_SIMPLE_AND_COMPLEX",
        },
        "accepted_upper_before": original_before,
        "accepted_upper_after": original_after,
        "visual_and_runtime_acceptance": "pending_review",
    }
    EVIDENCE_DIR.mkdir(parents=True, exist_ok=False)
    EVIDENCE_PATH.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    unreal.log(SUCCESS_MARKER + " " + json.dumps({
        "asset": mesh.get_path_name(), "convex_hulls": hull_count,
        "accepted_upper_unchanged": True, "evidence": str(EVIDENCE_PATH),
    }))


try:
    main()
except Exception as error:
    unreal.log_error("HARVEST_MIGHTY_OAK_FALLING_CANDIDATE_V2_IMPORT_FAILED " + str(error))
    raise
finally:
    unreal.SystemLibrary.quit_editor()
