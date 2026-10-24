"""Clay front/left/back/right orthographic renders of Orlando (body FBX + head OBJ), 1024 px, white background, flat light.
GEAR-02: the reference pose and scale a generated garment must match; also prints the head/neck landmarks the cleanup aligns to.
  blender --factory-startup -b --python tools/Render-BodyReference.py -- <out dir>
"""
import math
import os
import sys

import bpy
from mathutils import Vector

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(ROOT, 'Artifacts/GEAR02')
os.makedirs(out, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=os.path.join(ROOT, 'Artifacts/Characters/PlayerBase/Orlando_body.fbx'))
body = next(o for o in bpy.data.objects if o.type == 'MESH')
lines = open(os.path.join(ROOT, 'Artifacts/Characters/PlayerBase/Orlando_head.obj')).read().splitlines()  # Unreal cm, y flipped to the FBX frame
vs = [(float(x) * .01, -float(y) * .01, float(z) * .01) for _, x, y, z in (l.split() for l in lines if l.startswith('v '))]
fs = [[int(i.split('/')[0]) - 1 for i in l.split()[1:4]] for l in lines if l.startswith('f ')]
mesh = bpy.data.meshes.new('head'); mesh.from_pydata(vs, [], fs); mesh.update()
head = bpy.data.objects.new('head', mesh); bpy.context.scene.collection.objects.link(head)
for o in (body, head):
    v = [o.matrix_world @ x.co for x in o.data.vertices]
    print('BBOX', o.name, [tuple(round(f(c[i] for c in v), 3) for i in range(3)) for f in (min, max)])
mat = bpy.data.materials.new('clay'); mat.use_nodes = True
mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.6, 0.6, 0.6, 1)
mat.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 1
for o in (body, head):
    o.data.materials.clear(); o.data.materials.append(mat)
sc = bpy.context.scene
sc.render.engine = 'BLENDER_WORKBENCH'
sc.display.shading.light = 'FLAT'; sc.display.shading.color_type = 'MATERIAL'
sc.render.resolution_x = sc.render.resolution_y = 1024
sc.render.film_transparent = False
sc.world = bpy.data.worlds.new('w'); sc.world.color = (1, 1, 1)
sc.view_settings.view_transform = 'Standard'
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.type = 'ORTHO'
pts = [o.matrix_world @ x.co for o in (body, head) for x in o.data.vertices]
lo, hi = Vector(map(min, zip(*pts))), Vector(map(max, zip(*pts)))
ctr = (lo + hi) / 2
cam.data.ortho_scale = (hi.z - lo.z) / 0.9
for name, az in (('front', 0), ('left', 90), ('back', 180), ('right', 270)):
    # front: camera on -Y looking +Y (Blender front); left view: character's left side (+X) faces the camera
    a = math.radians(az)
    d = Vector((math.sin(a) * 1, -math.cos(a), 0))
    cam.location = ctr + d * 5
    cam.rotation_euler = (math.radians(90), 0, math.radians(az))
    sc.render.filepath = os.path.join(out, f'orlando_{name}.png')
    bpy.ops.render.render(write_still=True)
