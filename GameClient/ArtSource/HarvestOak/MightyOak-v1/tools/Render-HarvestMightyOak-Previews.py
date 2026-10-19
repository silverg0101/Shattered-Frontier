"""Render neutral source-material previews from the prepared Blender asset."""
import bpy
import os
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ASSET = os.path.join(ROOT, "exports", "MightyOak_v1.blend")
OUT = os.path.join(ROOT, "previews")
os.makedirs(OUT, exist_ok=True)
bpy.context.preferences.filepaths.use_scripts_auto_execute = False
bpy.ops.wm.open_mainfile(filepath=ASSET)
scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE"
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.render.resolution_percentage = 100
scene.render.film_transparent = False
scene.view_settings.view_transform = "Standard"
scene.view_settings.look = "Medium High Contrast"
scene.view_settings.exposure = 0
scene.view_settings.gamma = 1
scene.world = bpy.data.worlds.new("Neutral Source Review")
scene.world.use_nodes = True
background = scene.world.node_tree.nodes.get("Background")
background.inputs["Color"].default_value = (0.20, 0.22, 0.24, 1)
background.inputs["Strength"].default_value = 0.7

for obj in list(scene.objects):
    if obj.type in {"CAMERA", "LIGHT"}:
        bpy.data.objects.remove(obj, do_unlink=True)


def add_area(name, position, energy, size):
    data = bpy.data.lights.new(name, "AREA")
    data.energy = energy
    data.shape = "DISK"
    data.size = size
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    obj.location = position
    obj.rotation_euler = (Vector((0, 0, 4.5)) - obj.location).to_track_quat("-Z", "Y").to_euler()


def render(path, target, camera_position, ortho_scale, resolution):
    data = bpy.data.cameras.new("Review Camera")
    data.type = "ORTHO"
    data.ortho_scale = ortho_scale
    data.lens = 50
    cam = bpy.data.objects.new("Review Camera", data)
    scene.collection.objects.link(cam)
    cam.location = camera_position
    cam.rotation_euler = (Vector(target) - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    scene.render.resolution_x, scene.render.resolution_y = resolution
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam, do_unlink=True)
    bpy.data.cameras.remove(data)


add_area("Key", (5, -8, 9), 1800, 5)
add_area("Fill", (-7, -4, 4), 800, 7)
add_area("Rim", (0, 8, 7), 1200, 5)
render(os.path.join(OUT, "MightyOak_v1_FullTree.png"),
       (0, 0, 5), (7.5, -18, 7), 12.4, (1000, 1200))
render(os.path.join(OUT, "MightyOak_v1_LowerTrunk.png"),
       (0, 0, 0.85), (2.4, -5.5, 1.5), 3.6, (1200, 900))
for name in ("SM_MightyOak_UpperStructure", "SM_MightyOak_Foliage"):
    bpy.data.objects[name].hide_render = True
render(os.path.join(OUT, "MightyOak_v1_CutCap.png"),
       (0, 0, 0.82), (2.2, -2.8, 3.5), 1.8, (1000, 1000))
print("Wrote full-tree, lower-trunk, and isolated cut-cap previews.")
