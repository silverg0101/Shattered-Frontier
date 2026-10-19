"""Import the bounded oak and ironstone harvesting art packet into a new UE content version.

Run inside the UE 5.8 Editor Python environment, passing:
    --oak-source <oak revision folder> --rock-source <HarvestRock folder> --version <new name>

The script never overwrites an existing Oak or Rock destination. It imports standalone
FBX meshes and supplied PNG maps, then creates project-owned PBR materials. It does not
import scenes, create gameplay actors, or launch Unreal.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import sys

import unreal


CONTENT_ROOT = "/Game/Frontier/Gameplay/Harvest"
MANIFEST_ROOT = Path(__file__).resolve().parents[1] / "Artifacts" / "HarvestArt"
OAK_MESHES = (
    "SM_HarvestOak_Trunk", "SM_HarvestOak_Stump",
    "SM_HarvestOak_Splinter01", "SM_HarvestOak_Splinter02", "SM_HarvestOak_BarkChip01",
)

ROCK_MESHES = (
    "SM_HarvestRock_Intact", "SM_HarvestRock_Mined01", "SM_HarvestRock_Mined02",
    "SM_HarvestRock_Remnant", "SM_HarvestRock_Chip01", "SM_HarvestRock_Chip02",
    "SM_HarvestRock_Chip03", "SM_HarvestRock_Chip04", "SM_HarvestRock_Chip05",
)
OAK_TEXTURES = {
    "T_Oak_Bark_BaseColor": "color",
    "T_Oak_Bark_Normal": "normal",
    "T_Oak_Bark_Roughness": "data",
    "T_Oak_Endgrain_BaseColor": "color",
    "T_Oak_Endgrain_Roughness": "data",
    "T_Oak_Leaf_BaseColor": "color",
    "T_Oak_Longgrain_BaseColor": "color",
}
ROCK_TEXTURES = {
    "T_HarvestRock_BaseColor": "color",
    "T_HarvestRock_Normal": "normal_opengl",
    "T_HarvestRock_Roughness": "data",
    "T_HarvestRock_InteriorBaseColor": "color",
}
SCALE_CM_PER_SOURCE_METER = 100.0
# transform_vertex_to_absolute applies the FBX scene's metre-to-centimetre unit transform.
# A further 100x import multiplier would double-convert the source and overscale by 100x.
FBX_IMPORT_SCALE = 1.0


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--oak-source", required=True, type=Path)
    parser.add_argument("--rock-source", required=True, type=Path)
    parser.add_argument("--version", required=True)
    argv = sys.argv[1:]
    if "--" in argv:
        argv = argv[argv.index("--") + 1:]
    elif not any(token.startswith("--oak-source") for token in argv):
        # UE's ExecutePythonScript command line may keep script arguments outside sys.argv.
        command_line = unreal.SystemLibrary.get_command_line()
        tokens = shlex.split(command_line, posix=False)
        argv = [token for token in tokens if token.startswith("--oak-source") or
                token.startswith("--rock-source") or token.startswith("--version") or
                token.startswith("--") and token in ("--oak-source", "--rock-source", "--version")]
    args, unknown = parser.parse_known_args(argv)
    if unknown:
        raise RuntimeError("Unexpected import arguments: " + " ".join(unknown))
    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.version):
        raise ValueError("Version must contain only letters, digits, underscore, and hyphen")
    args.oak_source = args.oak_source.resolve()
    args.rock_source = args.rock_source.resolve()
    return args


def require_file(path):
    if not path.is_file():
        raise FileNotFoundError("Required harvest source file is missing: " + str(path))
    return path


def input_packet(root, mesh_names, texture_roles):
    exports = root / "Exports"
    textures = root / "Textures"
    if not root.is_dir():
        raise FileNotFoundError("Source folder does not exist: " + str(root))
    meshes = {}
    for name in mesh_names:
        meshes[name] = require_file(exports / (name + ".fbx"))
    maps = {name: (require_file(textures / (name + ".png")), role)
            for name, role in texture_roles.items()}
    return meshes, maps


def asset_path(folder, name):
    return folder + "/" + name + "." + name


def make_directory(path):
    if not unreal.EditorAssetLibrary.make_directory(path):
        if not unreal.EditorAssetLibrary.does_directory_exist(path):
            raise RuntimeError("Could not create Content folder: " + path)


def import_tasks(tasks):
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for task in tasks:
        imported = task.get_editor_property("imported_object_paths")
        if not imported:
            raise RuntimeError("Import produced no assets for " + task.get_editor_property("filename"))


def use_legacy_fbx_importer():
    """Keep this import session on FbxImportUI, where scale/pivot options apply."""
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        raise RuntimeError("Cannot disable Interchange FBX without an editor world")
    unreal.SystemLibrary.execute_console_command(world, "Interchange.FeatureFlags.Import.FBX 0")
    unreal.log("HARVEST_FBX_INTERCHANGE_DISABLED_FOR_IMPORT_SESSION")


def make_fbx_task(filename, destination, name):
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    data = options.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", False)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("build_nanite", False)
    data.set_editor_property("bake_pivot_in_vertex", False)
    data.set_editor_property("transform_vertex_to_absolute", True)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("convert_scene_unit", True)
    data.set_editor_property("import_uniform_scale", FBX_IMPORT_SCALE)
    options.set_editor_property("static_mesh_import_data", data)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(filename))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("options", options)
    task_options = task.get_editor_property("options")
    task_data = task_options.get_editor_property("static_mesh_import_data")
    unreal.log("HARVEST_FBX_SETTINGS " + name + " " + json.dumps({
        "uniform_scale": task_data.get_editor_property("import_uniform_scale"),
        "convert_scene_unit": task_data.get_editor_property("convert_scene_unit"),
        "transform_vertex_to_absolute": task_data.get_editor_property("transform_vertex_to_absolute"),
    }))
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    return task


def make_texture_task(filename, destination, name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(filename))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    return task


def load_imported(folder, names, expected_class):
    assets = {}
    for name in names:
        obj = unreal.EditorAssetLibrary.load_asset(asset_path(folder, name))
        if obj is None or not isinstance(obj, expected_class):
            raise RuntimeError("Imported asset is missing or has the wrong type: " + asset_path(folder, name))
        assets[name] = obj
    return assets


def configure_textures(textures, roles):
    for name, role in roles.items():
        texture = textures[name]
        texture.set_editor_property("srgb", role == "color")
        if role == "normal_opengl":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("flip_green_channel", True)
        elif role == "data":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        unreal.EditorAssetLibrary.save_loaded_asset(texture)


def new_material(folder, name, two_sided=False):
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError("Could not create material " + folder + "/" + name)
    material.set_editor_property("two_sided", two_sided)
    return material


def expression(material, class_name, x, y):
    cls = getattr(unreal, class_name, None)
    if cls is None:
        raise RuntimeError("UE Python API does not expose " + class_name)
    node = unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)
    if node is None:
        raise RuntimeError("Could not create material expression " + class_name)
    return node


def connect_output(material, node, output, material_property):
    if not unreal.MaterialEditingLibrary.connect_material_property(node, output, material_property):
        raise RuntimeError("Could not connect material output " + str(material_property))


def texture_sample(material, texture, x, y, sampler=None):
    sample = expression(material, "MaterialExpressionTextureSample", x, y)
    sample.set_editor_property("texture", texture)
    if sampler is not None:
        sample.set_editor_property("sampler_type", sampler)
    return sample


def scalar_constant(material, value, x, y):
    node = expression(material, "MaterialExpressionConstant", x, y)
    node.set_editor_property("r", value)
    return node


def connect_pbr(material, base_sample, rough_sample=None, normal_sample=None, roughness=0.8):
    connect_output(material, base_sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if rough_sample is not None:
        connect_output(material, rough_sample, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        connect_output(material, scalar_constant(material, roughness, 0, 220), "", unreal.MaterialProperty.MP_ROUGHNESS)
    if normal_sample is not None:
        connect_output(material, normal_sample, "RGB", unreal.MaterialProperty.MP_NORMAL)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)


def create_oak_materials(folder, textures):
    materials = {
        "bark": new_material(folder, "M_HarvestOak_Bark"),
        "wood": new_material(folder, "M_HarvestOak_CutWood"),
        "leaf": new_material(folder, "M_HarvestOak_Leaf", two_sided=True),
        "longgrain": new_material(folder, "M_HarvestOak_Longgrain"),
    }
    connect_pbr(materials["bark"],
                texture_sample(materials["bark"], textures["T_Oak_Bark_BaseColor"], -420, 0),
                texture_sample(materials["bark"], textures["T_Oak_Bark_Roughness"], -420, 180),
                texture_sample(materials["bark"], textures["T_Oak_Bark_Normal"], -420, 360,
                               unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL))
    connect_pbr(materials["wood"],
                texture_sample(materials["wood"], textures["T_Oak_Endgrain_BaseColor"], -420, 0),
                texture_sample(materials["wood"], textures["T_Oak_Endgrain_Roughness"], -420, 180))
    connect_pbr(materials["leaf"],
                texture_sample(materials["leaf"], textures["T_Oak_Leaf_BaseColor"], -420, 0),
                roughness=0.86)
    connect_pbr(materials["longgrain"],
                texture_sample(materials["longgrain"], textures["T_Oak_Longgrain_BaseColor"], -420, 0),
                roughness=0.82)
    return materials


def component_mask(material, source, channel, x, y):
    node = expression(material, "MaterialExpressionComponentMask", x, y)
    for component in "rgba":
        node.set_editor_property(component, component == channel.lower())
    if not unreal.MaterialEditingLibrary.connect_material_expressions(source, "", node, ""):
        raise RuntimeError("Could not wire world-projection component mask")
    return node


def append_uv(material, position, first_channel, second_channel, x, y):
    first = component_mask(material, position, first_channel, x, y)
    second = component_mask(material, position, second_channel, x, y + 100)
    append = expression(material, "MaterialExpressionAppendVector", x + 130, y)
    for source, input_name in ((first, "A"), (second, "B")):
        if not unreal.MaterialEditingLibrary.connect_material_expressions(source, "", append, input_name):
            raise RuntimeError("Could not build projected texture coordinates")
    scale = scalar_constant(material, 0.02, x + 130, y + 140)
    multiply = expression(material, "MaterialExpressionMultiply", x + 300, y)
    for source, input_name in ((append, "A"), (scale, "B")):
        if not unreal.MaterialEditingLibrary.connect_material_expressions(source, "", multiply, input_name):
            raise RuntimeError("Could not scale projected texture coordinates")
    return multiply


def add_node(material, class_name, x, y, input_a, input_b):
    node = expression(material, class_name, x, y)
    for source, input_name in ((input_a, "A"), (input_b, "B")):
        if not unreal.MaterialEditingLibrary.connect_material_expressions(source, "", node, input_name):
            raise RuntimeError("Could not wire triplanar material expression " + class_name)
    return node


def create_rock_interior_material(folder, texture):
    """Use world-space triplanar UVs; the fracture faces never sample the exterior atlas."""
    material = new_material(folder, "M_HarvestRock_FractureRuntime")
    position = expression(material, "MaterialExpressionWorldPosition", -1000, 0)
    normal = expression(material, "MaterialExpressionPixelNormalWS", -1000, 540)
    absolute = expression(material, "MaterialExpressionAbs", -820, 540)
    if not unreal.MaterialEditingLibrary.connect_material_expressions(normal, "", absolute, ""):
        raise RuntimeError("Could not build triplanar normal weights")
    weights = [component_mask(material, absolute, channel, -650, 520 + i * 90)
               for i, channel in enumerate(("R", "G", "B"))]
    sum_xy = add_node(material, "MaterialExpressionAdd", -450, 620, weights[0], weights[1])
    sum_xyz = add_node(material, "MaterialExpressionAdd", -270, 620, sum_xy, weights[2])
    normalized = []
    for i, weight in enumerate(weights):
        normalized.append(add_node(material, "MaterialExpressionDivide", -80, 520 + i * 110,
                                   weight, sum_xyz))

    # Z-facing faces use XY; X-facing use YZ; Y-facing use XZ. Blend by absolute
    # world-normal components to avoid stretching one planar map around vertical cuts.
    axes = (("R", "G", 2), ("G", "B", 0), ("R", "B", 1))
    weighted = []
    for i, (first, second, weight_index) in enumerate(axes):
        uv = append_uv(material, position, first, second, 150, i * 330)
        sample = texture_sample(material, texture, 500, i * 330)
        if not unreal.MaterialEditingLibrary.connect_material_expressions(uv, "", sample, ""):
            raise RuntimeError("Could not connect triplanar texture coordinates")
        weighted.append(add_node(material, "MaterialExpressionMultiply", 700, i * 330,
                                 sample, normalized[weight_index]))
    blend = add_node(material, "MaterialExpressionAdd", 900, 80, weighted[0], weighted[1])
    blend = add_node(material, "MaterialExpressionAdd", 1080, 80, blend, weighted[2])
    connect_output(material, blend, "", unreal.MaterialProperty.MP_BASE_COLOR)
    connect_output(material, scalar_constant(material, 0.88, 900, 300), "",
                   unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def set_mesh_materials(mesh, material_roles, fallback_roles, two_sided=False):
    slots = list(mesh.get_editor_property("static_materials"))
    if not slots:
        raise RuntimeError("Imported mesh has no material slots: " + mesh.get_name())
    resolved = []
    for index, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property("material_slot_name")).lower()
        role = next((key for key in material_roles if key in slot_name), None)
        if role is None:
            if index >= len(fallback_roles):
                raise RuntimeError("Unknown material slot on " + mesh.get_name() + ": " + slot_name)
            role = fallback_roles[index]
        if role not in material_roles:
            raise RuntimeError("Invalid material mapping for " + mesh.get_name())
        resolved.append(role)
        slot.set_editor_property("material_interface", material_roles[role])
        slots[index] = slot
    mesh.set_editor_property("static_materials", slots)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return resolved


def digest(path):
    hasher = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            hasher.update(block)
    return hasher.hexdigest()


def main():
    args = parse_arguments()
    oak_mesh_sources, oak_map_sources = input_packet(args.oak_source, OAK_MESHES, OAK_TEXTURES)
    rock_mesh_sources, rock_map_sources = input_packet(args.rock_source, ROCK_MESHES, ROCK_TEXTURES)

    version_root = CONTENT_ROOT + "/" + args.version
    oak_root, rock_root = version_root + "/Oak", version_root + "/Rock"
    manifest_path = MANIFEST_ROOT / (args.version + "-import-manifest.json")
    if manifest_path.exists():
        raise RuntimeError("Refusing to overwrite import manifest: " + str(manifest_path))
    for destination in (oak_root, rock_root):
        if unreal.EditorAssetLibrary.does_directory_exist(destination):
            raise RuntimeError("Refusing to overwrite existing harvest Content folder: " + destination)
    if unreal.EditorAssetLibrary.does_asset_exist(oak_root) or unreal.EditorAssetLibrary.does_asset_exist(rock_root):
        raise RuntimeError("Refusing to overwrite a harvest asset at the requested version")

    oak_mesh_dir, oak_texture_dir, oak_material_dir = (oak_root + "/Meshes", oak_root + "/Textures", oak_root + "/Materials")
    rock_mesh_dir, rock_texture_dir, rock_material_dir = (rock_root + "/Meshes", rock_root + "/Textures", rock_root + "/Materials")
    for directory in (version_root, oak_root, rock_root, oak_mesh_dir, oak_texture_dir,
                      oak_material_dir, rock_mesh_dir, rock_texture_dir, rock_material_dir):
        make_directory(directory)

    use_legacy_fbx_importer()
    tasks = []
    for name, source in oak_mesh_sources.items():
        tasks.append(make_fbx_task(source, oak_mesh_dir, name))
    for name, (source, _) in oak_map_sources.items():
        tasks.append(make_texture_task(source, oak_texture_dir, name))
    for name, source in rock_mesh_sources.items():
        tasks.append(make_fbx_task(source, rock_mesh_dir, name))
    for name, (source, _) in rock_map_sources.items():
        tasks.append(make_texture_task(source, rock_texture_dir, name))
    import_tasks(tasks)

    oak_meshes = load_imported(oak_mesh_dir, OAK_MESHES, unreal.StaticMesh)
    oak_textures = load_imported(oak_texture_dir, OAK_TEXTURES, unreal.Texture2D)
    rock_meshes = load_imported(rock_mesh_dir, ROCK_MESHES, unreal.StaticMesh)
    rock_textures = load_imported(rock_texture_dir, ROCK_TEXTURES, unreal.Texture2D)
    # Reject double unit conversion before any runtime visual use.
    dimensions = {}
    for packet, manifest_name, entries_key, dimension_key, meshes in (
        (args.oak_source, "asset-manifest.json", "objects", "dimensions_m", oak_meshes),
        (args.rock_source, "manifest.json", "meshes", "dimensions_metres", rock_meshes)):
        source_manifest = json.loads((packet / manifest_name).read_text(encoding="utf-8-sig"))
        expected = {entry["name"]: entry[dimension_key] for entry in source_manifest[entries_key]}
        for name, mesh in meshes.items():
            bounds = mesh.get_bounding_box()
            actual = [bounds.max.x-bounds.min.x, bounds.max.y-bounds.min.y, bounds.max.z-bounds.min.z]
            target = [value * SCALE_CM_PER_SOURCE_METER for value in expected[name]]
            if abs(actual[2]-target[2]) > max(0.5,target[2]*0.02):
                raise RuntimeError("FBX height/unit conversion mismatch: " + name + " actual="+str(actual)+" expected_cm="+str(target))
            dimensions[name] = {"unreal_dimensions_cm": actual, "expected_source_cm": target}

    configure_textures(oak_textures, OAK_TEXTURES)
    configure_textures(rock_textures, ROCK_TEXTURES)

    oak_materials = create_oak_materials(oak_material_dir, oak_textures)
    rock_exterior = new_material(rock_material_dir, "M_HarvestRock_Exterior")
    connect_pbr(rock_exterior,
                texture_sample(rock_exterior, rock_textures["T_HarvestRock_BaseColor"], -420, 0),
                texture_sample(rock_exterior, rock_textures["T_HarvestRock_Roughness"], -420, 180),
                texture_sample(rock_exterior, rock_textures["T_HarvestRock_Normal"], -420, 360,
                               unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL))
    rock_fracture = create_rock_interior_material(
        rock_material_dir, rock_textures["T_HarvestRock_InteriorBaseColor"])

    oak_slot_map = {"bark": oak_materials["bark"], "wood": oak_materials["wood"], "leaf": oak_materials["leaf"], "longgrain": oak_materials["longgrain"]}
    oak_fallback = ["bark", "wood", "leaf"]
    oak_assignments = {}
    for name, mesh in oak_meshes.items():
        # Some FBX imports omit Crown's unused wood slot. If slot names are absent,
        # the remaining two source slots are bark then leaf.
        fallback = oak_fallback
        if name.endswith("_Crown") and len(mesh.get_editor_property("static_materials")) == 2:
            fallback = ["bark", "leaf"]
        oak_assignments[name] = set_mesh_materials(mesh, oak_slot_map, fallback)
    rock_roles = {"exterior": rock_exterior, "fracture": rock_fracture}
    rock_assignments = {}
    for name, mesh in rock_meshes.items():
        roles = set_mesh_materials(mesh, rock_roles, ["exterior", "fracture"])
        if name == "SM_HarvestRock_Intact" and roles != ["exterior", "fracture"]:
            raise RuntimeError("Rock material slots must remain exterior at index 0 and fracture at index 1: " + name)
        rock_assignments[name] = roles
        if name == "SM_HarvestRock_Intact":
            mesh.set_editor_property("allow_cpu_access", True)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    oak_trunk = oak_meshes["SM_HarvestOak_Trunk"]
    oak_trunk.set_editor_property("allow_cpu_access", True)
    unreal.EditorAssetLibrary.save_loaded_asset(oak_trunk)

    manifest = {
        "schema": "harvest-art-unreal-import-v1",
        "verified_bounds_cm": dimensions,
        "engine_target": "Unreal Engine 5.8",
        "version": args.version,
        "content_root": version_root,
        "units": {
            "source": "metres, Z up",
            "fbx_import_uniform_scale": FBX_IMPORT_SCALE,
            "engine": "centimetres",
            "verification": "All imported mesh heights matched source-manifest metres multiplied by 100 within 2% or 0.5 cm.",
            "pivot": "Preserve source pivot; bake_pivot_in_vertex=false and transform_vertex_to_absolute=true",
        },
        "collision": "No collision generated by import; visual-only mesh assets",
        "nanite": "Disabled on imported harvest meshes",
        "cpu_access": [oak_trunk.get_path_name(), rock_meshes["SM_HarvestRock_Intact"].get_path_name()],
        "oak": {
            "source": str(args.oak_source),
            "mesh_count": len(oak_meshes),
            "texture_count": len(oak_textures),
            "meshes": {name: {"asset": mesh.get_path_name(), "material_roles": oak_assignments[name],
                               "cpu_access": name == "SM_HarvestOak_Trunk"}
                       for name, mesh in oak_meshes.items()},
            "materials": {role: material.get_path_name() for role, material in oak_materials.items()},
            "textures": {name: {"asset": texture.get_path_name(), "role": OAK_TEXTURES[name],
                                "source_sha256": digest(oak_map_sources[name][0])}
                         for name, texture in oak_textures.items()},
        },
        "rock": {
            "source": str(args.rock_source),
            "mesh_count": len(rock_meshes),
            "texture_count": len(rock_textures),
            "meshes": {name: {"asset": mesh.get_path_name(), "material_roles": rock_assignments[name],
                               "cpu_access": name == "SM_HarvestRock_Intact"}
                       for name, mesh in rock_meshes.items()},
            "materials": {"exterior": rock_exterior.get_path_name(), "fracture_runtime": rock_fracture.get_path_name()},
            "textures": {name: {"asset": texture.get_path_name(), "role": ROCK_TEXTURES[name],
                                "source_sha256": digest(rock_map_sources[name][0])}
                         for name, texture in rock_textures.items()},
        },
        "source_sha256": {
            **{str(path.relative_to(args.oak_source)): digest(path) for path in list(oak_mesh_sources.values()) +
               [row[0] for row in oak_map_sources.values()]},
            **{str(path.relative_to(args.rock_source)): digest(path) for path in list(rock_mesh_sources.values()) +
               [row[0] for row in rock_map_sources.values()]},
        },
    }
    MANIFEST_ROOT.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    unreal.log("HARVEST_ART_IMPORT_COMPLETE " + json.dumps({
        "version": args.version, "oak_meshes": len(oak_meshes), "rock_meshes": len(rock_meshes),
        "oak_textures": len(oak_textures), "rock_textures": len(rock_textures),
        "manifest": str(manifest_path)}))


if __name__ == "__main__":
    main()
