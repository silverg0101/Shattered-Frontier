"""CUDA_VISIBLE_DEVICES=1 blender -b -P sheet.py -- ANIM.glb OUT.png
Contact sheet: 8 side views across the cycle (top two rows) + front views at contact/flight (bottom row),
orthographic, on a ground plane at the rest-pose paw height, with a red line marking that ground."""
import bpy, sys, math, numpy as np
from mathutils import Vector
f, out = sys.argv[sys.argv.index("--") + 1:][:2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.render.fps = 30  # the importer converts key times at the scene rate
bpy.ops.import_scene.gltf(filepath=f)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
for o in list(bpy.data.objects):
    if o.type == "MESH" and o.name.startswith("Icosphere"): bpy.data.objects.remove(o)
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
sc = bpy.context.scene
act = arm.animation_data.action; f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])
paws = ["bone_19", "bone_31", "bone_43", "bone_50"]
ground = min((arm.matrix_world @ arm.data.bones[p].tail_local).z for p in paws)
pts = [mesh.matrix_world @ Vector(c) for c in mesh.bound_box]
lo = Vector([min(p[i] for p in pts) for i in range(3)]); hi = Vector([max(p[i] for p in pts) for i in range(3)])
c = (lo + hi) / 2; size = hi - lo

sc.render.engine = "CYCLES"; sc.cycles.samples = 16; sc.cycles.use_denoising = False
prefs = bpy.context.preferences.addons["cycles"].preferences; prefs.compute_device_type = "CUDA"; prefs.refresh_devices()
for d in prefs.devices: d.use = d.type == "CUDA"
sc.cycles.device = "GPU"
W, H = 520, 360
sc.render.resolution_x, sc.render.resolution_y = W, H; sc.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("w"); world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.6, 0.62, 1); world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
sc.world = world
sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN")); sun.data.energy = 3
sun.rotation_euler = (math.radians(35), 0, math.radians(30)); sc.collection.objects.link(sun)
bpy.ops.mesh.primitive_plane_add(size=6, location=(c.x, c.y, ground))
pl = bpy.context.object; m = bpy.data.materials.new("g"); m.use_nodes = True
m.node_tree.nodes["Principled BSDF"].inputs[0].default_value = (0.35, 0.4, 0.35, 1); pl.data.materials.append(m)
# thin red ground line, visible edge-on from the side
bpy.ops.mesh.primitive_cube_add(size=1, location=(c.x, c.y + 1.5, ground)); ln = bpy.context.object
ln.scale = (6, 0.002, 0.002); mr = bpy.data.materials.new("r"); mr.use_nodes = True
mr.node_tree.nodes["Principled BSDF"].inputs[0].default_value = (1, 0, 0, 1); ln.data.materials.append(mr)

def cam(name, loc, rot, scale):
    o = bpy.data.objects.new(name, bpy.data.cameras.new(name)); o.data.type = "ORTHO"; o.data.ortho_scale = scale
    o.location, o.rotation_euler = loc, rot; o.data.clip_end = 50; sc.collection.objects.link(o); return o
zc = (ground + hi.z) / 2 + 0.03
L = max(size.x * 1.5, (hi.z - ground) * 1.45 * W / H)
side = cam("side", (c.x, c.y - 5, zc), (math.radians(90), 0, 0), L)  # looks +Y: rat's right side, head right
front = cam("front", (c.x + 5, c.y, zc), (math.radians(90), 0, math.radians(90)), L)

def shot(camo, fr):
    sc.camera = camo; sc.frame_set(fr); sc.render.filepath = "/tmp/_hr_shot.png"
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(sc.render.filepath); w, h = img.size
    px = np.empty(w * h * 4, np.float32); img.pixels.foreach_get(px); bpy.data.images.remove(img)
    return px.reshape(h, w, 4)
n = f1 - f0
side_frames = [f0 + round(i * n / 8) for i in range(8)]
tiles = [shot(side, fr) for fr in side_frames]
front_frames = [side_frames[0], side_frames[2], side_frames[4], side_frames[6]]
tiles += [shot(front, fr) for fr in front_frames]
rows = [np.concatenate(tiles[i:i + 4], axis=1) for i in (8, 4, 0)]  # pixel rows go bottom-up
sheet = np.concatenate(rows, axis=0)
img = bpy.data.images.new("sheet", sheet.shape[1], sheet.shape[0]); img.pixels.foreach_set(sheet.ravel())
img.filepath_raw, img.file_format = out, "PNG"; img.save()
print("SHEET", out, "side frames", side_frames, "front frames", front_frames)
