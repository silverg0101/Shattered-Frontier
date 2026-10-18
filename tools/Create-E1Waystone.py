"""Blender background authoring: one optional Greenlands landmark prototype.

blender --background --factory-startup --python tools/Create-E1Waystone.py -- --output <new folder>
This does not import into Unreal or change the E1 collision fixture.
"""
import argparse
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector


def material(name, color, metallic=0.0, roughness=0.8):
    result = bpy.data.materials.new(name)
    result.use_nodes = True
    bsdf = result.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    result.diffuse_color = (*color, 1)
    return result


def frustum(name, bottom, top, z0, z1, sides, rotation, surface):
    vertices = []
    for radius, z in ((bottom, z0), (top, z1)):
        vertices.extend((radius * math.cos(rotation + i * math.tau / sides),
                         radius * math.sin(rotation + i * math.tau / sides), z) for i in range(sides))
    faces = [tuple(reversed(range(sides))), tuple(range(sides, sides * 2))]
    faces.extend((i, (i + 1) % sides, (i + 1) % sides + sides, i + sides) for i in range(sides))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(surface)
    bevel = obj.modifiers.new("AuthoredEdgeBevel", "BEVEL")
    bevel.width = 0.012
    bevel.segments = 1
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    obj.select_set(False)
    return obj


def aim(obj, point):
    obj.rotation_euler = (Vector(point) - obj.location).to_track_quat("-Z", "Y").to_euler()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    output = args.output.resolve()
    names = ["GreenlandsWaystone.blend", "SM_GreenlandsWaystone.fbx", "GreenlandsWaystone.glb",
             "Waystone-Preview.png", "asset-manifest.json"]
    if any((output / name).exists() for name in names):
        raise RuntimeError("Refusing to overwrite an existing waystone output; choose a new folder.")
    output.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    stone = material("E1_WeatheredStone_Prototype", (0.28, 0.29, 0.25))
    bronze = material("E1_OldBronze_Prototype", (0.38, 0.22, 0.075), 0.72, 0.45)
    dark = material("E1_StoneInset_Prototype", (0.09, 0.105, 0.09))
    moss = material("E1_MossAccent_Prototype", (0.10, 0.15, 0.055))
    meshes = [
        frustum("SM_Waystone_Base", 0.65, 0.63, 0.0, 0.20, 6, math.pi / 6, stone),
        frustum("SM_Waystone_Step", 0.50, 0.47, 0.20, 0.33, 6, math.pi / 6, stone),
        frustum("SM_Waystone_Body", 0.37, 0.28, 0.33, 2.03, 8, math.pi / 8, stone),
        frustum("SM_Waystone_Collar", 0.34, 0.33, 2.03, 2.13, 8, math.pi / 8, stone),
        frustum("SM_Waystone_Crown", 0.31, 0.015, 2.13, 2.40, 8, math.pi / 8, stone),
    ]
    # Front is -Y; the bronze emblem is shallow and decorative, with no gameplay meaning.
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.185, depth=0.012,
                                      location=(0, -0.302, 1.32), rotation=(math.pi / 2, 0, 0))
    inset = bpy.context.object
    inset.name = "SM_Waystone_CompassInset"
    inset.data.materials.append(dark)
    meshes.append(inset)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.177, minor_radius=0.010,
                                   major_segments=32, minor_segments=6,
                                   location=(0, -0.315, 1.32), rotation=(math.pi / 2, 0, 0))
    ring = bpy.context.object
    ring.name = "SM_Waystone_BronzeRing"
    ring.data.materials.append(bronze)
    meshes.append(ring)
    vertices = [(0, -0.328, 1.32)]
    for i in range(16):
        angle = math.tau * i / 16
        radius = 0.16 if i % 4 == 0 else 0.115 if i % 2 == 0 else 0.036
        vertices.append((math.sin(angle) * radius, -0.328, 1.32 + math.cos(angle) * radius))
    compass_mesh = bpy.data.meshes.new("CompassStar")
    compass_mesh.from_pydata(vertices, [], [(0, (i + 1) % 16 + 1, i + 1) for i in range(16)])
    compass_mesh.update()
    star = bpy.data.objects.new("SM_Waystone_BronzeStar", compass_mesh)
    bpy.context.collection.objects.link(star)
    star.data.materials.append(bronze)
    meshes.append(star)
    # Small authored low-poly moss accents, without a texture or geometry dependency.
    for i, angle in enumerate((0.3, 2.4, 4.7)):
        patch = frustum("SM_Waystone_Moss_%d" % i, 0.11, 0.07, 0.20, 0.235, 6, angle, moss)
        patch.location.x = math.cos(angle) * 0.40
        patch.location.y = math.sin(angle) * 0.40
        meshes.append(patch)
    # Keep one stable origin at ground level, and a single named export mesh/material slots.
    bpy.ops.object.select_all(action="DESELECT")
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.join()
    model = bpy.context.object
    model.name = "SM_GreenlandsWaystone"
    scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(island_margin=0.03)
    bpy.ops.object.mode_set(mode="OBJECT")
    model["prototype_only"] = True
    model["reference"] = "docs/Game-Design/concept-art/Concept.png; Waystone-Turnaround.png"
    model["front_axis"] = "-Y; Z up; metres; origin at ground"
    model.data.calc_loop_triangles()
    triangle_count = len(model.data.loop_triangles)
    if triangle_count > 4000 or abs(model.dimensions.z - 2.4) > 0.02:
        raise RuntimeError("Prototype geometry exceeded its declared validation limits")
    # Exports include only the model; studio objects are kept in the native source.
    bpy.ops.export_scene.fbx(filepath=str(output / names[1]), use_selection=True, object_types={"MESH"},
                             apply_unit_scale=True, axis_forward="-Y", axis_up="Z", bake_anim=False)
    bpy.ops.export_scene.gltf(filepath=str(output / names[2]), use_selection=True, export_format="GLB")
    # Neutral preview studio, excluded from both exchange files above.
    ground = material("StudioGround", (0.18, 0.19, 0.17))
    bpy.ops.mesh.primitive_plane_add(size=200)
    bpy.context.object.name = "PREVIEW_Ground"
    bpy.context.object.data.materials.append(ground)
    bpy.ops.object.camera_add(location=(3.5, -5.6, 3.0))
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 3.5
    aim(camera, (0, 0, 1.15))
    scene.camera = camera
    for name, location, energy, size in (("Key", (2, -4, 5), 650, 4), ("Fill", (-3, -1, 3), 400, 3),
                                         ("Rim", (0, 3, 4), 550, 3)):
        bpy.ops.object.light_add(type="AREA", location=location)
        light = bpy.context.object
        light.name = "PREVIEW_" + name
        light.data.energy = energy
        light.data.shape = "DISK"
        light.data.size = size
        aim(light, (0, 0, 1.1))
    scene.world = bpy.data.worlds.new("WaystoneStudio")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs[0].default_value = (0.18, 0.20, 0.22, 1)
    scene.world.node_tree.nodes["Background"].inputs[1].default_value = 0.5
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 32
    scene.render.resolution_x = 768
    scene.render.resolution_y = 768
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(output / names[3])
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(output / names[0]))
    bpy.ops.render.render(write_still=True)
    manifest = {"asset": model.name, "status": "optional art prototype; not imported into Unreal",
                "blender": bpy.app.version_string, "dimensions_metres": list(model.dimensions),
                "triangles": triangle_count, "material_slots": len(model.data.materials),
                "uv_layers": len(model.data.uv_layers), "units": "metres", "up": "Z", "front": "-Y",
                "limitations": ["flat prototype materials; no PBR texture bake", "no production LOD/collision acceptance",
                                "not photogrammetry; no RealityScan reconstruction", "generated concept is a guide, not exact geometry"]}
    (output / names[4]).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("SF_WAYSTONE_CREATED", json.dumps(manifest))


if __name__ == "__main__":
    main()
