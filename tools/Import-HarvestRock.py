"""Import the isolated Harvest Rock revision 02 packet in an Unreal Editor Python session.

Example (inside UE 5.8, Editor Python):
    py "<project>/tools/Import-HarvestRock.py" --source "<project>/GameClient/ArtSource/HarvestRock/rev02"

Imports only the nine standalone FBX meshes and four maps into the fresh RockV2
version, creates the exterior and runtime fracture materials, and records source
provenance under ignored Artifacts/HarvestArt. It never launches Unreal.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shlex
import sys

import unreal


CONTENT_ROOT = "/Game/Frontier/Gameplay/Harvest/RockV2"
MANIFEST_PATH = Path(__file__).resolve().parents[1] / "Artifacts" / "HarvestArt" / "RockV2-import-manifest.json"
DEFAULT_SOURCE = Path(__file__).resolve().parents[1] / "GameClient" / "ArtSource" / "HarvestRock" / "rev02"
HELPERS_PATH = Path(__file__).with_name("Import-HarvestArt.py")
SCALE_CM_PER_SOURCE_METER = 100.0
FBX_IMPORT_SCALE = 1.0

MESHES = (
    "SM_HarvestRock_Intact", "SM_HarvestRock_Mined01", "SM_HarvestRock_Mined02",
    "SM_HarvestRock_Remnant", "SM_HarvestRock_Chip01", "SM_HarvestRock_Chip02",
    "SM_HarvestRock_Chip03", "SM_HarvestRock_Chip04", "SM_HarvestRock_Chip05",
)
TEXTURES = {
    "T_HarvestRock_BaseColor": "color",
    "T_HarvestRock_Normal": "normal_opengl",
    "T_HarvestRock_Roughness": "data",
    "T_HarvestRock_InteriorBaseColor": "color",
}


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    argv = sys.argv[1:]
    if "--" in argv:
        argv = argv[argv.index("--") + 1:]
    elif not any(token.startswith("--source") for token in argv):
        tokens = shlex.split(unreal.SystemLibrary.get_command_line(), posix=False)
        for index, token in enumerate(tokens):
            if token == "--source":
                argv = tokens[index:index + 2]
                break
            if token.startswith("--source="):
                argv = [token]
                break
    args, unknown = parser.parse_known_args(argv)
    if unknown:
        raise RuntimeError("Unexpected import arguments: " + " ".join(unknown))
    args.source = args.source.resolve()
    return args


def load_helpers():
    spec = importlib.util.spec_from_file_location("harvest_art_import_helpers", HELPERS_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError("Could not load the established harvest import helpers")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def require_file(path):
    if not path.is_file():
        raise FileNotFoundError("Required Harvest Rock source file is missing: " + str(path))
    return path


def digest(path):
    hasher = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            hasher.update(block)
    return hasher.hexdigest()


def verify_source(source):
    manifest_path = require_file(source / "manifest.json")
    validation_path = require_file(source / "exchange-validation.json")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    validation = json.loads(validation_path.read_text(encoding="utf-8-sig"))
    entries = {entry["name"]: entry for entry in manifest.get("meshes", [])}
    if set(entries) != set(MESHES):
        raise RuntimeError("Source manifest mesh names differ from the fixed Rock V2 packet")
    validation_fbx = {Path(entry["file"]).stem: entry for entry in validation.get("fbx", [])}
    mesh_files = {name: require_file(source / "Exports" / (name + ".fbx")) for name in MESHES}
    texture_files = {name: require_file(source / "Textures" / (name + ".png")) for name in TEXTURES}
    for name in MESHES:
        if name not in validation_fbx or validation_fbx[name].get("mesh_count") != 1:
            raise RuntimeError("Source exchange audit does not confirm one FBX mesh for " + name)
        if len(entries[name].get("dimensions_metres", [])) != 3:
            raise RuntimeError("Source manifest lacks measured XYZ dimensions for " + name)
    if entries["SM_HarvestRock_Intact"].get("vertices") != 171 or entries["SM_HarvestRock_Intact"].get("triangles") != 338:
        raise RuntimeError("Intact source no longer matches the prepared revision 02 shell")
    return manifest, manifest_path, validation_path, entries, validation_fbx, mesh_files, texture_files


def make_directory(path):
    if not unreal.EditorAssetLibrary.make_directory(path):
        if not unreal.EditorAssetLibrary.does_directory_exist(path):
            raise RuntimeError("Could not create Content folder: " + path)


def load_imported(folder, names, expected_class):
    assets = {}
    for name in names:
        path = folder + "/" + name + "." + name
        obj = unreal.EditorAssetLibrary.load_asset(path)
        if obj is None or not isinstance(obj, expected_class):
            raise RuntimeError("Imported asset is missing or has wrong type: " + path)
        assets[name] = obj
    return assets


def import_and_check(helpers, tasks):
    helpers.import_tasks(tasks)
    for task in tasks:
        paths = task.get_editor_property("imported_object_paths")
        if len(paths) != 1:
            raise RuntimeError("Expected exactly one imported asset from " + task.get_editor_property("filename"))


def main():
    args = parse_arguments()
    helpers = load_helpers()
    (source_manifest, source_manifest_path, validation_path, entries, validation_fbx,
     mesh_sources, texture_sources) = verify_source(args.source)

    if MANIFEST_PATH.exists():
        raise RuntimeError("Refusing to overwrite import manifest: " + str(MANIFEST_PATH))
    if unreal.EditorAssetLibrary.does_directory_exist(CONTENT_ROOT) or unreal.EditorAssetLibrary.does_asset_exist(CONTENT_ROOT):
        raise RuntimeError("Refusing to overwrite existing Harvest Rock V2 target: " + CONTENT_ROOT)

    mesh_dir = CONTENT_ROOT + "/Meshes"
    texture_dir = CONTENT_ROOT + "/Textures"
    material_dir = CONTENT_ROOT + "/Materials"
    for directory in (CONTENT_ROOT, mesh_dir, texture_dir, material_dir):
        make_directory(directory)

    helpers.use_legacy_fbx_importer()
    tasks = [helpers.make_fbx_task(path, mesh_dir, name) for name, path in mesh_sources.items()]
    tasks.extend(helpers.make_texture_task(path, texture_dir, name) for name, path in texture_sources.items())
    import_and_check(helpers, tasks)

    meshes = load_imported(mesh_dir, MESHES, unreal.StaticMesh)
    textures = load_imported(texture_dir, list(TEXTURES), unreal.Texture2D)
    helpers.configure_textures(textures, TEXTURES)

    dimensions = {}
    for name, mesh in meshes.items():
        bounds = mesh.get_bounding_box()
        minimum = [bounds.min.x, bounds.min.y, bounds.min.z]
        maximum = [bounds.max.x, bounds.max.y, bounds.max.z]
        actual_cm = [maximum[i] - minimum[i] for i in range(3)]
        expected_cm = [value * SCALE_CM_PER_SOURCE_METER for value in entries[name]["dimensions_metres"]]
        for actual, expected in zip(actual_cm, expected_cm):
            if abs(actual - expected) > max(0.5, expected * 0.02):
                raise RuntimeError("FBX bounds/unit conversion mismatch for " + name +
                                   ": actual_cm=" + str(actual_cm) + " expected_cm=" + str(expected_cm))
        dimensions[name] = {
            "unreal_local_bounds_min_cm": minimum,
            "unreal_local_bounds_max_cm": maximum,
            "unreal_dimensions_cm": actual_cm,
            "expected_source_cm": expected_cm,
        }

    exterior = helpers.new_material(material_dir, "M_HarvestRockV2_Exterior")
    helpers.connect_pbr(
        exterior,
        helpers.texture_sample(exterior, textures["T_HarvestRock_BaseColor"], -420, 0),
        helpers.texture_sample(exterior, textures["T_HarvestRock_Roughness"], -420, 180),
        helpers.texture_sample(exterior, textures["T_HarvestRock_Normal"], -420, 360,
                               unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
    )
    fracture = helpers.create_rock_interior_material(
        material_dir, textures["T_HarvestRock_InteriorBaseColor"])

    role_materials = {"exterior": exterior, "fracture": fracture}
    assignments = {}
    for name, mesh in meshes.items():
        slots = list(mesh.get_editor_property("static_materials"))
        if name == "SM_HarvestRock_Intact" and len(slots) != 2:
            raise RuntimeError("Expected exterior and fracture material slots on " + name)
        roles = helpers.set_mesh_materials(mesh, role_materials, ["exterior", "fracture"])
        if name == "SM_HarvestRock_Intact" and roles != ["exterior", "fracture"]:
            raise RuntimeError("Material slot order must remain exterior then fracture on " + name)
        mesh.set_editor_property("allow_cpu_access", name == "SM_HarvestRock_Intact")
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        assignments[name] = roles

    input_files = list(mesh_sources.values()) + list(texture_sources.values())
    artifact = {
        "schema": "harvest-rock-v2-unreal-import-v1",
        "engine_target": "Unreal Engine 5.8",
        "source": {
            "folder": str(args.source),
            "revision": source_manifest.get("revision"),
            "coordinates": source_manifest.get("coordinates"),
            "manifest_sha256": digest(source_manifest_path),
            "exchange_validation_sha256": digest(validation_path),
            "imported_inputs_sha256": {str(path.relative_to(args.source)): digest(path) for path in input_files},
        },
        "content_root": CONTENT_ROOT,
        "units": {"source": "metres, Z up", "fbx_import_uniform_scale": FBX_IMPORT_SCALE,
                  "engine": "centimetres", "verification": "Imported XYZ bounds compared with source manifest within 2% or 0.5 cm."},
        "meshes": {
            name: {
                "asset": meshes[name].get_path_name(),
                "source_vertices": entries[name]["vertices"],
                "source_triangles": entries[name]["triangles"],
                "source_nonmanifold_edges": entries[name]["nonmanifold_edges"],
                "source_fbx_mesh_count": validation_fbx[name]["mesh_count"],
                "material_roles": assignments[name],
                "cpu_access": name == "SM_HarvestRock_Intact",
                "nanite": False,
                "collision_generated": False,
                "pivot_convention": "fragment centered" if "_Chip" in name else "resource stage ground-origin",
                "bounds": dimensions[name],
            }
            for name in MESHES
        },
        "textures": {name: {"asset": textures[name].get_path_name(), "role": TEXTURES[name],
                             "source_sha256": digest(texture_sources[name])}
                     for name in TEXTURES},
        "materials": {"exterior": exterior.get_path_name(), "fracture_runtime": fracture.get_path_name()},
        "import_policy": {"fbx_importer": "legacy FbxImportUI", "interchange_fbx_feature_flag": 0,
                          "transform_vertex_to_absolute": True, "convert_scene": True,
                          "convert_scene_unit": True, "bake_pivot_in_vertex": False,
                          "nanite": "disabled", "collision": "not generated"},
    }
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.write_text(json.dumps(artifact, indent=2) + "\n", encoding="utf-8")
    unreal.log("HARVEST_ROCK_V2_IMPORT_COMPLETE " + json.dumps({
        "mesh_count": len(meshes), "texture_count": len(textures),
        "content_root": CONTENT_ROOT, "manifest": str(MANIFEST_PATH)}))


if __name__ == "__main__":
    main()
