"""Prepare the CC BY 4.0 Mighty Oak source as a scaled, capped Blender asset.

Run with Blender 5.2.x background and --factory-startup. This script contains no
external Python dependencies and does not execute source-file scripts.
"""
import bpy
import bmesh
import hashlib
import json
import math
import os
import sys
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SOURCE_BLEND = os.path.join(ROOT, "source", "extracted", "source", "oaks.blend")
EXPORT_DIR = os.path.join(ROOT, "exports")
PREVIEW_DIR = os.path.join(ROOT, "previews")
TARGET_FULL_HEIGHT_M = 10.0
LOWER_JOIN_HEIGHT_M = 1.0
WELD_EPSILON_SOURCE_M = 0.0001

os.makedirs(EXPORT_DIR, exist_ok=True)
os.makedirs(PREVIEW_DIR, exist_ok=True)
bpy.context.preferences.filepaths.use_scripts_auto_execute = False
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.open_mainfile(filepath=SOURCE_BLEND)

source_structure = bpy.data.objects["structure.002"]
source_foliage = bpy.data.objects["foliage.002"]


def get_or_create_cutwood_material():
    material = bpy.data.materials.get("M_HarvestOak_CutWood")
    if material is None:
        material = bpy.data.materials.new("M_HarvestOak_CutWood")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (420, 40)
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    shader.location = (120, 40)
    shader.inputs["Roughness"].default_value = 0.8
    color = bpy.data.images.load(
        os.path.join(ROOT, "materials", "T_Oak_Endgrain_BaseColor.png"),
        check_existing=True,
    )
    color.colorspace_settings.name = "sRGB"
    color_node = nodes.new("ShaderNodeTexImage")
    color_node.image = color
    color_node.location = (-220, 100)
    roughness = bpy.data.images.load(
        os.path.join(ROOT, "materials", "T_Oak_Endgrain_Roughness.png"),
        check_existing=True,
    )
    roughness.colorspace_settings.name = "Non-Color"
    rough_node = nodes.new("ShaderNodeTexImage")
    rough_node.image = roughness
    rough_node.location = (-220, -120)
    links.new(color_node.outputs["Color"], shader.inputs["Base Color"])
    links.new(rough_node.outputs["Color"], shader.inputs["Roughness"])
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])
    return material, color, roughness


cutwood_material, cutwood_color, cutwood_roughness = get_or_create_cutwood_material()
if cutwood_material not in source_structure.data.materials[:]:
    source_structure.data.materials.append(cutwood_material)


def transformed_vertices(obj):
    return [obj.matrix_world @ v.co for v in obj.data.vertices]


all_world = transformed_vertices(source_structure) + transformed_vertices(source_foliage)
ground_z = min(v.z for v in all_world)
full_height_source = max(v.z for v in all_world) - ground_z
scale = TARGET_FULL_HEIGHT_M / full_height_source
join_z_source = ground_z + LOWER_JOIN_HEIGHT_M / scale
root_origin = source_structure.matrix_world.translation.copy()


def boundary_loops(bm):
    pending = {e for e in bm.edges if len(e.link_faces) == 1}
    loops = []
    while pending:
        first = pending.pop()
        stack = [first]
        loop = [first]
        while stack:
            edge = stack.pop()
            for vert in edge.verts:
                for candidate in vert.link_edges:
                    if candidate in pending:
                        pending.remove(candidate)
                        stack.append(candidate)
                        loop.append(candidate)
        loops.append(loop)
    return loops


def projected_cap_uv(face, uv_layer):
    coords = [loop.vert.co for loop in face.loops]
    if not coords:
        return
    min_x = min(v.x for v in coords)
    min_y = min(v.y for v in coords)
    for loop in face.loops:
        loop[uv_layer].uv = ((loop.vert.co.x - min_x) * 0.2,
                             (loop.vert.co.y - min_y) * 0.2)


def transform_and_scale(bm):
    for vert in bm.verts:
        vert.co -= root_origin
        vert.co.z -= ground_z
        vert.co *= scale


def mesh_from_bmesh(name, bm, source_mesh):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    for material in source_mesh.materials:
        mesh.materials.append(material)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def make_lower_chunk():
    bm = bmesh.new()
    bm.from_mesh(source_structure.data)
    bm.transform(source_structure.matrix_world)
    bmesh.ops.bisect_plane(
        bm,
        geom=list(bm.verts) + list(bm.edges) + list(bm.faces),
        dist=1e-5,
        plane_co=Vector((0, 0, join_z_source)),
        plane_no=Vector((0, 0, 1)),
        clear_outer=True,
        clear_inner=False,
    )
    # Below this one-meter join, every source opening is either the ground rim
    # or the horizontal cut. Fill only these traced source boundary loops.
    loops = boundary_loops(bm)
    uv_layer = bm.loops.layers.uv.active
    cap_faces = []
    for loop in loops:
        result = bmesh.ops.holes_fill(bm, edges=loop, sides=0)
        cap_faces.extend(result.get("faces", []))
    for face in cap_faces:
        face.material_index = 1
        if uv_layer:
            projected_cap_uv(face, uv_layer)
    bm.normal_update()
    for face in cap_faces:
        if abs(face.calc_center_median().z - join_z_source) <= 1e-4 and face.normal.z < 0:
            face.normal_flip()
        elif face.calc_center_median().z < join_z_source * 0.5 and face.normal.z > 0:
            face.normal_flip()
    # The source has a duplicated coincident vertex seam at the base. Welding
    # only within 0.1 mm closes that zero-volume seam without changing silhouette.
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=WELD_EPSILON_SOURCE_M)
    bmesh.ops.triangulate(
        bm,
        faces=[face for face in bm.faces if face.material_index == 1],
        quad_method="BEAUTY",
        ngon_method="BEAUTY",
    )
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.normal_update()
    bm.verts.ensure_lookup_table()
    bm.edges.ensure_lookup_table()
    bm.faces.ensure_lookup_table()
    if any(not edge.is_manifold for edge in bm.edges):
        raise RuntimeError(
            "Lower trunk is not closed manifold after source-loop caps: "
            f"boundary={sum(len(e.link_faces)==1 for e in bm.edges)}, "
            f"wire={sum(len(e.link_faces)==0 for e in bm.edges)}, "
            f"overused={sum(len(e.link_faces)>2 for e in bm.edges)}"
        )
    overlap_pairs = BVHTree.FromBMesh(bm, epsilon=1e-6).overlap(
        BVHTree.FromBMesh(bm, epsilon=1e-6)
    )
    actual_intersections = 0
    for i, j in overlap_pairs:
        if i < j and set(bm.faces[i].verts).isdisjoint(bm.faces[j].verts):
            actual_intersections += 1
    if actual_intersections:
        raise RuntimeError(
            f"Lower trunk has {actual_intersections} nonadjacent self-overlap pairs"
        )
    triangles = sum(len(face.verts) - 2 for face in bm.faces)
    if triangles > 25000:
        raise RuntimeError(f"Lower trunk triangle budget exceeded: {triangles}")
    join_cap_normals = [face.normal.z for face in bm.faces
                        if face.material_index == 1
                        and abs(face.calc_center_median().z - join_z_source) <= 1e-4]
    transform_and_scale(bm)
    bm.normal_update()
    if not join_cap_normals or any(n <= 0 for n in join_cap_normals):
        raise RuntimeError("Lower join cap normal does not point outward (+Z)")
    obj = mesh_from_bmesh("SM_MightyOak_LowerTrunk", bm, source_structure.data)
    bm.free()
    return obj, {"faces": len(obj.data.polygons), "triangles": triangles,
                 "nonmanifold_edges": 0, "nonadjacent_self_overlap_pairs": 0,
                 "closed_boundary_loops_capped": len(loops),
                 "join_cap_triangles": len(join_cap_normals),
                 "join_cap_normal_z_min": min(join_cap_normals),
                 "join_cap_normal_z_max": max(join_cap_normals),
                 "join_cap_material_index": 1,
                 "preserved_uv_layers": [uv.name for uv in obj.data.uv_layers]}


def make_upper_structure():
    bm = bmesh.new()
    bm.from_mesh(source_structure.data)
    bm.transform(source_structure.matrix_world)
    bmesh.ops.bisect_plane(
        bm,
        geom=list(bm.verts) + list(bm.edges) + list(bm.faces),
        dist=1e-5,
        plane_co=Vector((0, 0, join_z_source)),
        plane_no=Vector((0, 0, 1)),
        clear_outer=False,
        clear_inner=True,
    )
    # Close the two join contours only. Existing terminal twig boundaries are
    # source geometry and are not altered by this bounded lower-trunk operation.
    uv_layer = bm.loops.layers.uv.active
    join_loops = [loop for loop in boundary_loops(bm)
                  if all(abs(v.co.z - join_z_source) <= 1e-4
                         for edge in loop for v in edge.verts)]
    for loop in join_loops:
        result = bmesh.ops.holes_fill(bm, edges=loop, sides=0)
        caps = result.get("faces", [])
        bm.normal_update()
        for face in caps:
            face.material_index = 1
            if uv_layer:
                projected_cap_uv(face, uv_layer)
            if face.normal.z > 0:
                face.normal_flip()
    bmesh.ops.triangulate(
        bm,
        faces=[face for face in bm.faces if face.material_index == 1],
        quad_method="BEAUTY",
        ngon_method="BEAUTY",
    )
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.normal_update()
    upper_cap_normals = [face.normal.z for face in bm.faces
                         if face.material_index == 1
                         and abs(face.calc_center_median().z - join_z_source) <= 1e-4]
    if not upper_cap_normals or any(n >= 0 for n in upper_cap_normals):
        raise RuntimeError("Upper join caps do not face outward (-Z)")
    transform_and_scale(bm)
    obj = mesh_from_bmesh("SM_MightyOak_UpperStructure", bm, source_structure.data)
    bm.free()
    return obj, {"contours": len(join_loops), "cap_triangles": len(upper_cap_normals),
                 "cap_normal_z_min": min(upper_cap_normals),
                 "cap_normal_z_max": max(upper_cap_normals)}


def make_foliage():
    bm = bmesh.new()
    bm.from_mesh(source_foliage.data)
    bm.transform(source_foliage.matrix_world)
    for vert in bm.verts:
        vert.co -= root_origin
        vert.co.z -= ground_z
        vert.co *= scale
    obj = mesh_from_bmesh("SM_MightyOak_Foliage", bm, source_foliage.data)
    bm.free()
    return obj


# Retain the source material node graphs and exact side UVs while deriving.
lower, lower_metrics = make_lower_chunk()
upper, join_caps = make_upper_structure()
foliage = make_foliage()
outputs = [lower, upper, foliage]
for obj in list(bpy.context.scene.objects):
    if obj not in outputs:
        bpy.data.objects.remove(obj, do_unlink=True)

# Restore convenient tree scale in scene metadata; all three output objects have
# identity transform and a common ground-origin coordinate system.
scene = bpy.context.scene
scene.unit_settings.system = "METRIC"
scene.unit_settings.scale_length = 1.0
scene.unit_settings.length_unit = "METERS"
for obj in outputs:
    obj.location = (0, 0, 0)
    obj.rotation_euler = (0, 0, 0)
    obj.scale = (1, 1, 1)
    obj.select_set(False)
for image in bpy.data.images:
    if image.source == "FILE" and image.packed_file is None:
        try:
            image.pack()
        except RuntimeError:
            pass

# Verify the lower piece after serialization into a native Blender mesh.
check = bmesh.new()
check.from_mesh(lower.data)
closed = all(edge.is_manifold for edge in check.edges)
if not closed:
    raise RuntimeError("Serialized lower trunk lost manifold closure")
check.free()


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def mesh_triangles(obj):
    return sum(max(0, len(poly.vertices) - 2) for poly in obj.data.polygons)


def object_metrics(obj):
    return {
        "dimensions_m": [round(float(value), 6) for value in obj.dimensions],
        "triangles": mesh_triangles(obj),
        "uv_layers": [layer.name for layer in obj.data.uv_layers],
        "materials": [mat.name if mat else None for mat in obj.data.materials],
    }


source_texture_details = {
    "bark_color.png": {"role": "bark base color", "color_space": "sRGB", "alpha": False},
    "bark_normal.png": {"role": "bark tangent normal", "color_space": "Non-Color", "alpha": False},
    "bark_roughness.png": {"role": "bark roughness", "color_space": "Non-Color", "alpha": False},
    "oak_foliage_plane.png": {"role": "foliage color and opacity mask", "color_space": "sRGB", "alpha": True},
}


# Store a portable editable source and a game interchange copy.
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(EXPORT_DIR, "MightyOak_v1.blend"))
for obj in outputs:
    obj.select_set(True)
bpy.context.view_layer.objects.active = upper
bpy.ops.export_scene.gltf(
    filepath=os.path.join(EXPORT_DIR, "MightyOak_v1.glb"),
    export_format="GLB",
    use_selection=True,
    export_apply=True,
    export_texcoords=True,
    export_normals=True,
    export_materials="EXPORT",
)
for obj in outputs:
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=os.path.join(EXPORT_DIR, obj.name + ".fbx"),
        use_selection=True,
        object_types={"MESH"},
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_UNITS",
        bake_space_transform=False,
        add_leaf_bones=False,
        mesh_smooth_type="FACE",
        use_tspace=True,
        path_mode="COPY",
        embed_textures=True,
    )

manifest = {
    "title": "Mighty Oak Trees",
    "creator": "Jagobo",
    "source_url": "https://sketchfab.com/3d-models/mighty-oak-trees-4f6ab5594a8a415aba3f958682b9ced5",
    "license": "CC BY 4.0",
    "license_url": "https://creativecommons.org/licenses/by/4.0/",
    "required_credit": "Mighty oak trees by Jagobo, CC BY 4.0",
    "original_archive_sha256": sha256(os.path.join(ROOT, "source", "mighty-oak-trees.zip")),
    "source_blend_sha256": sha256(SOURCE_BLEND),
    "preparation_script": "tools/Prepare-HarvestMightyOak.py",
    "blender": bpy.app.version_string,
    "target_full_tree_height_m": TARGET_FULL_HEIGHT_M,
    "lower_join_height_m": LOWER_JOIN_HEIGHT_M,
    "source_combined_height_m": full_height_source,
    "scale_factor": scale,
    "ground_origin_m": [0.0, 0.0, 0.0],
    "objects": [obj.name for obj in outputs],
    "object_metrics": {obj.name: object_metrics(obj) for obj in outputs},
    "lower_trunk_validation": lower_metrics,
    "upper_join_contours_capped": join_caps,
    "materials": sorted({m.name for obj in outputs for m in obj.data.materials if m}),
    "material_slots": {obj.name: [m.name if m else None for m in obj.data.materials]
                       for obj in outputs},
    "cap_material": "M_HarvestOak_CutWood (slot 1; reused project-authored endgrain basecolor/roughness textures)",
    "cap_textures_sha256": {
        "T_Oak_Endgrain_BaseColor.png": sha256(os.path.join(ROOT, "materials", "T_Oak_Endgrain_BaseColor.png")),
        "T_Oak_Endgrain_Roughness.png": sha256(os.path.join(ROOT, "materials", "T_Oak_Endgrain_Roughness.png")),
    },
    "source_textures": {
        filename: {
            **details,
            "path": "materials/" + filename,
            "sha256": sha256(os.path.join(ROOT, "materials", filename)),
        }
        for filename, details in source_texture_details.items()
    },
    "exports": {
        "blend": {"path": "exports/MightyOak_v1.blend", "sha256": sha256(os.path.join(EXPORT_DIR, "MightyOak_v1.blend"))},
        "glb_review_only": {"path": "exports/MightyOak_v1.glb", "sha256": sha256(os.path.join(EXPORT_DIR, "MightyOak_v1.glb"))},
        "fbx": {obj.name: {"path": "exports/" + obj.name + ".fbx",
                           "sha256": sha256(os.path.join(EXPORT_DIR, obj.name + ".fbx"))}
                for obj in outputs},
    },
    "fbx_unit_scale": "Blender metric meters exported with FBX_SCALE_UNITS for import at Unreal scale 1.0",
    "fbx_validation": "Separate FBX files reimported in Blender 5.2.2; bounds, UVMap, embedded texture images, material slots, and lower manifold topology recorded in validation/import-audit.json.",
    "previews": [
        "previews/MightyOak_v1_FullTree.png",
        "previews/MightyOak_v1_LowerTrunk.png",
        "previews/MightyOak_v1_CutCap.png",
    ],
    "textures_packed_in_blend": True,
    "limitations": [
        "Only Tree 3 was prepared; upper branch mesh remains the source mesh with its original terminal topology.",
        "The lower closed chunk spans ground to 1.0 m; the gameplay hit point around 0.85 m lies inside it.",
        "Hidden cut/base caps use projected UVs and slot 1 M_HarvestOak_CutWood from the existing project endgrain texture pair.",
        "The optional GLB is a review export; the FBX meshes are the tested Unreal-facing interchange files.",
    ],
}
with open(os.path.join(ROOT, "manifest.json"), "w", encoding="utf-8") as stream:
    json.dump(manifest, stream, indent=2)
print(json.dumps(manifest, indent=2))
