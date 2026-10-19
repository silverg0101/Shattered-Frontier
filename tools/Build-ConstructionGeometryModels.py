"""Build Blender/export prototypes from bounded canonical GEO-01 payload subsets.

Run inside Blender 5.1.1+:
  blender --background --factory-startup --python tools/Build-ConstructionGeometryModels.py -- \
    --payload-dir Artifacts/ConstructionGeometry/<pilot> --output-dir GameClient/ArtSource/ConstructionGeometryPilot-20261018/Model-v001

This accepts the original three-shape baseline or the six-shape candidate subset
and deliberately marks either result as prototype-subset. It does not manufacture
GEO-01/GEO-02 approval.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

import bpy
from mathutils import Vector

BASELINE = ("cell-full", "slab-half", "slope-1to1")
CANDIDATE = ("cell-full", "slab-half", "slab-quarter", "post-quarter", "slope-1to1", "corner-outer")
CELL_M = 0.5
Q = 65536
REPO_ROOT = Path(__file__).resolve().parents[1]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def args_after_separator():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--payload-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args(argv)
    args.payload_dir = Path(args.payload_dir).resolve()
    args.output_dir = Path(args.output_dir).resolve()
    if not args.payload_dir.is_dir():
        raise FileNotFoundError(args.payload_dir)
    if args.output_dir.exists():
        raise FileExistsError(f"Refusing to overwrite {args.output_dir}")
    pilot = json.loads((args.payload_dir / "manifest.json").read_text(encoding="utf-8"))
    if pilot.get("status") != "prototype-subset" or pilot.get("acceptance_claim") is not False:
        raise ValueError("Input must be the bounded prototype-subset manifest")
    keys = tuple(item["shape_key"] for item in pilot.get("shapes", []))
    if keys not in (BASELINE, CANDIDATE):
        raise ValueError("Input pilot shape list/order differs from the supported baseline or candidate subset")
    return args, pilot


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World")
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 960
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.view_transform = "AgX"
    scene.render.image_settings.color_depth = "8"
    scene.render.filepath = ""
    scene.render.threads_mode = "FIXED"
    scene.render.threads = 4
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.075, 0.085, 0.10, 1.0)
    background.inputs["Strength"].default_value = 0.22
    return scene


def create_preview_rig(scene, center: Vector, extent: float):
    staging = bpy.data.collections.new("STAGING_RENDER_ONLY")
    scene.collection.children.link(staging)

    def link_stage(obj):
        for collection in list(obj.users_collection):
            collection.objects.unlink(obj)
        staging.objects.link(obj)
        return obj

    bpy.ops.mesh.primitive_plane_add(size=3.0, location=(center.x, center.y, -0.006))
    ground = link_stage(bpy.context.object)
    ground.name = "RENDER_ONLY_ground"
    ground.data.materials.append(make_material("RENDER_ONLY_neutral_ground", (0.10, 0.115, 0.13), 0.88))

    bpy.ops.object.camera_add(location=center + Vector((1.05, -1.55, 1.0)) * max(1.25, extent * 3.0))
    camera = link_stage(bpy.context.object)
    camera.name = "RENDER_ONLY_camera"
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = max(0.92, extent * 2.3)
    direction = center - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.camera = camera

    for name, offset, energy, size in (
        ("key", (1.0, -1.0, 2.0), 38, 1.2),
        ("fill", (-1.1, -0.2, 1.0), 15, 1.4),
        ("rim", (0.3, 1.3, 1.7), 27, 1.0),
    ):
        bpy.ops.object.light_add(type="AREA", location=center + Vector(offset) * max(0.6, extent))
        lamp = link_stage(bpy.context.object)
        lamp.name = f"RENDER_ONLY_{name}"
        lamp.data.energy = energy
        lamp.data.shape = "DISK"
        lamp.data.size = size * max(0.6, extent)
        lamp.rotation_euler = (center - lamp.location).to_track_quat("-Z", "Y").to_euler()


def make_material(name, color, roughness=0.78):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    principled = material.node_tree.nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value = (*color, 1.0)
    principled.inputs["Roughness"].default_value = roughness
    return material


def mesh_bounds(mesh_objects):
    points = [obj.matrix_world @ vertex.co for obj in mesh_objects for vertex in obj.data.vertices]
    if not points:
        raise RuntimeError("No mesh vertices in reimport")
    low = [min(point[axis] for point in points) for axis in range(3)]
    high = [max(point[axis] for point in points) for axis in range(3)]
    return {"min_m": low, "max_m": high, "dimensions_m": [high[i] - low[i] for i in range(3)]}


def mesh_topology(mesh_objects):
    components = loose_vertices = boundary_edges = nonmanifold_edges = 0
    weld_m = 0.000001
    for obj in mesh_objects:
        mesh = obj.data
        def point_key(index):
            point = obj.matrix_world @ mesh.vertices[index].co
            return tuple(round(float(point[axis]) / weld_m) for axis in range(3))
        all_points = {point_key(i) for i in range(len(mesh.vertices))}
        adjacency = {}
        edge_uses = {}
        for polygon in mesh.polygons:
            points = [point_key(i) for i in polygon.vertices]
            for point in points:
                adjacency.setdefault(point, set())
            for a, b in zip(points, points[1:] + points[:1]):
                if a == b:
                    continue
                adjacency[a].add(b)
                adjacency[b].add(a)
                edge = tuple(sorted((a, b)))
                edge_uses[edge] = edge_uses.get(edge, 0) + 1
        remaining = set(adjacency)
        while remaining:
            components += 1
            stack = [remaining.pop()]
            while stack:
                current = stack.pop()
                for neighbor in adjacency[current] & remaining:
                    remaining.remove(neighbor)
                    stack.append(neighbor)
        loose_vertices += len(all_points - set(adjacency))
        boundary_edges += sum(count == 1 for count in edge_uses.values())
        nonmanifold_edges += sum(count != 2 for count in edge_uses.values())
    return {"connected_vertex_components": components, "loose_vertex_count": loose_vertices,
            "boundary_edge_count": boundary_edges, "nonmanifold_or_unused_edge_count": nonmanifold_edges,
            "coincident_vertex_weld_tolerance_m": weld_m}


def export_selected(obj, out_folder: Path, stem: str):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    glb = out_folder / f"{stem}.glb"
    bpy.ops.export_scene.gltf(
        filepath=str(glb), export_format="GLB", use_selection=True,
        export_apply=True, export_cameras=False, export_lights=False,
        export_animations=False, export_materials="EXPORT", export_yup=True,
    )
    fbx = out_folder / f"{stem}.fbx"
    bpy.ops.export_scene.fbx(
        filepath=str(fbx), use_selection=True, object_types={"MESH"},
        global_scale=1.0, apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS",
        axis_forward="-Z", axis_up="Y", add_leaf_bones=False,
        use_mesh_modifiers=True, bake_space_transform=False,
    )
    return glb, fbx


def fresh_import_check(path: Path, kind: str, expected_dimensions):
    clear_scene()
    if kind == "glb":
        bpy.ops.import_scene.gltf(filepath=str(path))
    else:
        bpy.ops.import_scene.fbx(filepath=str(path), use_anim=False)
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    bounds = mesh_bounds(meshes)
    topology = mesh_topology(meshes)
    actual = bounds["dimensions_m"]
    errors = [abs(actual[i] - expected_dimensions[i]) for i in range(3)]
    expected_topology = {
            "connected_vertex_components": 1, "loose_vertex_count": 0,
            "boundary_edge_count": 0, "nonmanifold_or_unused_edge_count": 0,
            "coincident_vertex_weld_tolerance_m": 0.000001}
    if max(errors) > 0.0001 or len(meshes) != 1 or topology != expected_topology:
        raise RuntimeError(f"{kind.upper()} import failed bounds/topology checks: bounds={bounds}, topology={topology}")
    return {"status": "passed", "mesh_object_count": len(meshes), "bounds": bounds,
            "maximum_axis_error_m": max(errors), "mesh_faces": sum(len(obj.data.polygons) for obj in meshes),
            "topology": topology}


def run_one(shape_key, shape_info, payload_dir: Path, out_root: Path, tool_hash: str):
    payload_path = payload_dir / f"{shape_key}.json"
    payload_hash = sha256(payload_path)
    if payload_hash != shape_info["payload_sha256"]:
        raise ValueError(f"Payload hash mismatch for {shape_key}")
    payload = json.loads(payload_path.read_text(encoding="utf-8"))
    if payload.get("shape_key") != shape_key or payload.get("shape_version") != 1:
        raise ValueError(f"Unexpected identity in {payload_path}")

    out_folder = out_root / shape_key
    out_folder.mkdir()
    scene = clear_scene()
    vertices = [tuple((coordinate / Q) * CELL_M for coordinate in point) for point in payload["vertices"]]
    faces = [tuple(face) for part in payload["parts"] for face in part]
    mesh = bpy.data.meshes.new(f"GEO01_{shape_key}_v1_mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.update(calc_edges=True)
    obj = bpy.data.objects.new(f"SM_GEO01_{shape_key}_v1_PROTOTYPE", mesh)
    scene.collection.objects.link(obj)
    obj.data.materials.append(make_material("Prototype_Clay_Unpainted", (0.24, 0.29, 0.34), 0.82))
    obj["geometry_key"] = shape_key
    obj["geometry_version"] = 1
    obj["asset_status"] = "prototype-subset; not approved for production"
    obj["canonical_payload_sha256"] = payload_hash
    obj["envelope_units"] = "meters"
    obj["grid_cell_m"] = CELL_M
    obj["origin_pivot"] = "minimum grid corner (0,0,0)"
    obj["orientation"] = "construction-local X/Y, Z up; identity only"
    for polygon in mesh.polygons:
        polygon.use_smooth = False
    local_bounds = mesh_bounds([obj])
    expected_dimensions = local_bounds["dimensions_m"]
    center = Vector([(local_bounds["min_m"][i] + local_bounds["max_m"][i]) * 0.5 for i in range(3)])
    create_preview_rig(scene, center, max(expected_dimensions))
    scene.render.filepath = str(out_folder / "asset-view.png")
    bpy.ops.render.render(write_still=True)

    stem = f"SM_GEO01_{shape_key.replace('-', '_')}_v001"
    blend = out_folder / f"{stem}.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    glb, fbx = export_selected(obj, out_folder, stem)
    # Reimport each exchange format into a fresh, empty Blender scene.
    glb_check = fresh_import_check(glb, "glb", expected_dimensions)
    fbx_check = fresh_import_check(fbx, "fbx", expected_dimensions)
    return {
        "geometry_key": shape_key,
        "geometry_version": payload["shape_version"],
        "status": "prototype-subset",
        "acceptance_claim": False,
        "canonical_payload": payload_path.relative_to(REPO_ROOT).as_posix(),
        "canonical_payload_sha256": payload_hash,
        "vertices": len(vertices),
        "polygons": len(faces),
        "triangle_count": sum(max(1, len(face) - 2) for face in faces),
        "dimensions_m": expected_dimensions,
        "pivot": "minimum grid corner (0,0,0)",
        "allowed_rotations": "canonical payload contains 24 proper cell-centered cube rotations; this prototype source is identity-oriented",
        "files": {path.name: {"sha256": sha256(path), "bytes": path.stat().st_size} for path in (blend, glb, fbx, out_folder / "asset-view.png")},
        "checks": {"glb_fresh_import": glb_check, "fbx_fresh_import": fbx_check,
                   "source_saved": blend.is_file(), "render_saved": (out_folder / "asset-view.png").is_file()},
        "generator_sha256": tool_hash,
    }


def main():
    args, pilot = args_after_separator()
    args.output_dir.mkdir(parents=True)
    tool_path = Path(__file__).resolve()
    tool_hash = sha256(tool_path)
    outputs = {}
    for shape_info in pilot["shapes"]:
        outputs[shape_info["shape_key"]] = run_one(shape_info["shape_key"], shape_info,
                                                    args.payload_dir, args.output_dir, tool_hash)
    source_manifest = args.payload_dir / "manifest.json"
    manifest = {
        "asset_set": f"GEO-01 {len(outputs)}-shape Blender mesh prototype subset",
        "version": "v001" if tuple(outputs) == BASELINE else "v002",
        "status": "prototype-subset",
        "acceptance_claim": False,
        "engine_ready": False,
        "blender_version": bpy.app.version_string,
        "units": "meters",
        "grid_cell_m": CELL_M,
        "source_geometry_manifest": source_manifest.relative_to(REPO_ROOT).as_posix(),
        "source_geometry_manifest_sha256": sha256(source_manifest),
        "source_status": pilot["status"],
        "shapes": outputs,
        "open_gates": [
            "GEO-01 remains prototype-subset; approved production geometry manifests and content registry IDs are absent.",
            "Applicable full GEO-02 compatibility, seam/material binding, collision, GEO-03 engine import, and production performance gates remain open.",
            "Neutral clay prototype only; rough-timber/stone surface treatments, UVs, LODs, collision assets and engine presentation are not included.",
            "The Blender source and GLB/FBX files are fresh-import checked for mesh presence and canonical bounds; full semantic/engine correspondence is not certified.",
        ],
    }
    (args.output_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": "prototype-subset", "output_dir": str(args.output_dir),
                      "shape_count": len(outputs), "acceptance_claim": False}, indent=2))


if __name__ == "__main__":
    main()
