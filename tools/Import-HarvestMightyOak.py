"""Import the prepared, fixed Mighty Oak v1 packet in an Unreal Editor Python session.

Example (inside UE 5.8, Editor Python):
    py "<project>/tools/Import-HarvestMightyOak.py" --source "<project>/GameClient/ArtSource/HarvestOak/MightyOak-v1"

The script imports only three standalone FBX meshes and their source maps, creates
project-owned materials, and writes a compact provenance manifest under ignored
Artifacts/HarvestArt. It refuses any pre-existing target and never launches Unreal.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shlex
import sys

import unreal


CONTENT_ROOT = "/Game/Frontier/Gameplay/Harvest/MightyOakV1"
MANIFEST_PATH = Path(__file__).resolve().parents[1] / "Artifacts" / "HarvestArt" / "MightyOakV1-import-manifest.json"
DEFAULT_SOURCE = Path(__file__).resolve().parents[1] / "GameClient" / "ArtSource" / "HarvestOak" / "MightyOak-v1"
ENGINE_HELPERS_PATH = Path(__file__).with_name("Import-HarvestArt.py")
SCALE_CM_PER_SOURCE_METER = 100.0
FBX_IMPORT_SCALE = 1.0

MESHES = (
    "SM_MightyOak_LowerTrunk",
    "SM_MightyOak_UpperStructure",
    "SM_MightyOak_Foliage",
)
TEXTURES = {
    "T_MightyOak_Bark_BaseColor": ("materials/bark_color.png", "color"),
    "T_MightyOak_Bark_Normal": ("materials/bark_normal.png", "normal_opengl"),
    "T_MightyOak_Bark_Roughness": ("materials/bark_roughness.png", "data"),
    "T_MightyOak_CutWood_BaseColor": ("materials/T_Oak_Endgrain_BaseColor.png", "color"),
    "T_MightyOak_CutWood_Roughness": ("materials/T_Oak_Endgrain_Roughness.png", "data"),
    "T_MightyOak_Foliage": ("materials/oak_foliage_plane.png", "color_alpha"),
}


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    argv = sys.argv[1:]
    if "--" in argv:
        argv = argv[argv.index("--") + 1:]
    elif not any(token.startswith("--source") for token in argv):
        tokens = shlex.split(unreal.SystemLibrary.get_command_line(), posix=False)
        argv = [token for token in tokens if token == "--source" or token.startswith("--source=")]
    args, unknown = parser.parse_known_args(argv)
    if unknown:
        raise RuntimeError("Unexpected import arguments: " + " ".join(unknown))
    args.source = args.source.resolve()
    return args


def load_helpers():
    spec = importlib.util.spec_from_file_location("harvest_art_import_helpers", ENGINE_HELPERS_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError("Could not load the established harvest import helpers")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def require_file(path):
    if not path.is_file():
        raise FileNotFoundError("Required Mighty Oak source file is missing: " + str(path))
    return path


def digest(path):
    hasher = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            hasher.update(block)
    return hasher.hexdigest()


def verify_source(source):
    manifest_path = require_file(source / "manifest.json")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    if manifest.get("creator") != "Jagobo" or manifest.get("license") != "CC BY 4.0":
        raise RuntimeError("Source manifest does not identify Jagobo and CC BY 4.0")
    if manifest.get("target_full_tree_height_m") != 10.0 or manifest.get("lower_join_height_m") != 1.0:
        raise RuntimeError("Source manifest does not match the prepared 10 m / 1 m Mighty Oak")
    if manifest.get("objects") != list(MESHES):
        raise RuntimeError("Source manifest mesh list differs from the fixed Mighty Oak v1 packet")
    mesh_files = {name: require_file(source / "exports" / (name + ".fbx")) for name in MESHES}
    texture_files = {name: require_file(source / relative) for name, (relative, _) in TEXTURES.items()}
    metrics = manifest.get("object_metrics", {})
    dimensions = {}
    for name in MESHES:
        expected = metrics.get(name, {}).get("dimensions_m")
        if not isinstance(expected, list) or len(expected) != 3:
            raise RuntimeError("Source manifest lacks measured XYZ dimensions for " + name)
        dimensions[name] = expected
    if abs(dimensions[MESHES[0]][2] - 1.0) > 0.001:
        raise RuntimeError("Lower trunk source height is not the measured 1.0 m join chunk")
    return manifest, manifest_path, mesh_files, texture_files, dimensions


def asset_path(folder, name):
    return folder + "/" + name + "." + name


def make_directory(path):
    if not unreal.EditorAssetLibrary.make_directory(path):
        if not unreal.EditorAssetLibrary.does_directory_exist(path):
            raise RuntimeError("Could not create Content folder: " + path)


def import_checked(helpers, tasks):
    helpers.import_tasks(tasks)


def load_imported(folder, names, expected_class):
    assets = {}
    for name in names:
        obj = unreal.EditorAssetLibrary.load_asset(asset_path(folder, name))
        if obj is None or not isinstance(obj, expected_class):
            raise RuntimeError("Imported asset is missing or has wrong type: " + asset_path(folder, name))
        assets[name] = obj
    return assets


def make_material(folder, name, helpers, masked=False, two_sided=False):
    material = helpers.new_material(folder, name, two_sided=two_sided)
    if masked:
        blend = getattr(getattr(unreal, "BlendMode", None), "BLEND_MASKED", None)
        if blend is None:
            raise RuntimeError("UE Python API does not expose the Masked material blend mode")
        material.set_editor_property("blend_mode", blend)
    return material


def create_materials(folder, textures, helpers):
    bark = make_material(folder, "M_MightyOak_Bark", helpers)
    helpers.connect_pbr(
        bark,
        helpers.texture_sample(bark, textures["T_MightyOak_Bark_BaseColor"], -420, 0),
        helpers.texture_sample(bark, textures["T_MightyOak_Bark_Roughness"], -420, 180),
        helpers.texture_sample(bark, textures["T_MightyOak_Bark_Normal"], -420, 360,
                               unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
    )

    wood = make_material(folder, "M_MightyOak_CutWood", helpers)
    helpers.connect_pbr(
        wood,
        helpers.texture_sample(wood, textures["T_MightyOak_CutWood_BaseColor"], -420, 0),
        helpers.texture_sample(wood, textures["T_MightyOak_CutWood_Roughness"], -420, 180),
    )

    foliage = make_material(folder, "M_MightyOak_Foliage", helpers, masked=True, two_sided=True)
    foliage_shading = getattr(getattr(unreal, "MaterialShadingModel", None), "MSM_TWOSIDED_FOLIAGE", None)
    if foliage_shading is not None:
        foliage.set_editor_property("shading_model", foliage_shading)
    sample = helpers.texture_sample(foliage, textures["T_MightyOak_Foliage"], -420, 0)
    helpers.connect_output(foliage, sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    helpers.connect_output(foliage, sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    unreal.MaterialEditingLibrary.recompile_material(foliage)
    unreal.EditorAssetLibrary.save_loaded_asset(foliage)
    return {"bark": bark, "cutwood": wood, "foliage": foliage}, foliage_shading is not None


def assign_fixed_slots(meshes, materials):
    assignments = {}
    expectations = {
        MESHES[0]: ["bark", "cutwood"],
        MESHES[1]: ["bark", "cutwood"],
        MESHES[2]: ["foliage"],
    }
    for name, roles in expectations.items():
        mesh = meshes[name]
        slots = list(mesh.get_editor_property("static_materials"))
        if len(slots) != len(roles):
            raise RuntimeError("Unexpected material slot count on " + name + ": " + str(len(slots)))
        for index, role in enumerate(roles):
            slots[index].set_editor_property("material_interface", materials[role])
        mesh.set_editor_property("static_materials", slots)
        mesh.set_editor_property("allow_cpu_access", name == MESHES[0])
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        assignments[name] = roles
    return assignments


def main():
    args = parse_arguments()
    helpers = load_helpers()
    manifest, source_manifest_path, mesh_sources, texture_sources, expected_dimensions = verify_source(args.source)

    if MANIFEST_PATH.exists():
        raise RuntimeError("Refusing to overwrite import manifest: " + str(MANIFEST_PATH))
    if unreal.EditorAssetLibrary.does_directory_exist(CONTENT_ROOT) or unreal.EditorAssetLibrary.does_asset_exist(CONTENT_ROOT):
        raise RuntimeError("Refusing to overwrite existing Mighty Oak v1 Content target: " + CONTENT_ROOT)

    mesh_dir, texture_dir, material_dir = (CONTENT_ROOT + "/Meshes", CONTENT_ROOT + "/Textures", CONTENT_ROOT + "/Materials")
    for directory in (CONTENT_ROOT, mesh_dir, texture_dir, material_dir):
        make_directory(directory)

    helpers.use_legacy_fbx_importer()
    tasks = [helpers.make_fbx_task(path, mesh_dir, name) for name, path in mesh_sources.items()]
    tasks.extend(helpers.make_texture_task(path, texture_dir, name) for name, path in texture_sources.items())
    import_checked(helpers, tasks)

    meshes = load_imported(mesh_dir, MESHES, unreal.StaticMesh)
    textures = load_imported(texture_dir, list(TEXTURES), unreal.Texture2D)
    texture_roles = {name: role for name, (_, role) in TEXTURES.items()}
    helpers.configure_textures(textures, texture_roles)
    # The foliage image is an sRGB color texture with authored alpha. Keep its
    # alpha data intact for the opacity-mask input.
    textures["T_MightyOak_Foliage"].set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(textures["T_MightyOak_Foliage"])

    verified_dimensions = {}
    for name, mesh in meshes.items():
        bounds = mesh.get_bounding_box()
        actual_cm = [bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z]
        expected_cm = [value * SCALE_CM_PER_SOURCE_METER for value in expected_dimensions[name]]
        for actual, expected in zip(actual_cm, expected_cm):
            if abs(actual - expected) > max(0.5, expected * 0.02):
                raise RuntimeError("FBX dimensions/unit conversion mismatch for " + name +
                                   ": actual_cm=" + str(actual_cm) + " expected_cm=" + str(expected_cm))
        verified_dimensions[name] = {"unreal_dimensions_cm": actual_cm, "expected_source_cm": expected_cm}

    materials, foliage_shading_verified = create_materials(material_dir, textures, helpers)
    assignments = assign_fixed_slots(meshes, materials)

    artifact = {
        "schema": "harvest-mighty-oak-unreal-import-v1",
        "engine_target": "Unreal Engine 5.8",
        "source": {
            "title": manifest["title"],
            "creator": manifest["creator"],
            "license": manifest["license"],
            "required_credit": manifest["required_credit"],
            "source_url": manifest["source_url"],
            "original_archive_sha256": manifest["original_archive_sha256"],
            "source_manifest_sha256": digest(source_manifest_path),
            "imported_inputs_sha256": {str(path.relative_to(args.source)): digest(path)
                                         for path in list(mesh_sources.values()) + list(texture_sources.values())},
        },
        "content_root": CONTENT_ROOT,
        "prepared_scale": {"full_tree_height_m": 10.0, "lower_join_height_m": 1.0,
                           "fbx_import_uniform_scale": FBX_IMPORT_SCALE, "engine_units": "centimetres"},
        "verified_bounds_cm": verified_dimensions,
        "meshes": {name: {"asset": mesh.get_path_name(), "material_roles": assignments[name],
                           "cpu_access": name == MESHES[0], "nanite": False, "collision_generated": False}
                   for name, mesh in meshes.items()},
        "textures": {name: {"asset": textures[name].get_path_name(), "role": role,
                             "source_sha256": digest(texture_sources[name])}
                     for name, role in texture_roles.items()},
        "materials": {role: material.get_path_name() for role, material in materials.items()},
        "foliage": {"blend_mode": "masked", "two_sided": True,
                    "two_sided_foliage_shading_model_available": foliage_shading_verified,
                    "opacity_mask_input": "source texture alpha"},
        "import_policy": {"fbx_importer": "legacy FbxImportUI", "interchange_fbx_feature_flag": 0,
                          "transform_vertex_to_absolute": True, "convert_scene": True,
                          "convert_scene_unit": True, "bake_pivot_in_vertex": False,
                          "nanite": "disabled", "collision": "not generated"},
    }
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.write_text(json.dumps(artifact, indent=2) + "\n", encoding="utf-8")
    unreal.log("HARVEST_MIGHTY_OAK_IMPORT_COMPLETE " + json.dumps({
        "mesh_count": len(meshes), "texture_count": len(textures),
        "content_root": CONTENT_ROOT, "manifest": str(MANIFEST_PATH)}))


if __name__ == "__main__":
    main()
