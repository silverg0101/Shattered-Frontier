"""Headless Blender preview of a generated character GLB: 4 sides plus face, torso, hand and feet close-ups, textured and clay.

blender -b -P tools/Preview-PixalGlb.py -- --glb <file.glb> --out <new folder>
Prints triangle count and image sizes. Workbench engine only (no GPU raytracing, safe beside a Pixal job).
"""
import argparse, math, sys
from pathlib import Path
import bpy
from mathutils import Vector

p = argparse.ArgumentParser()
p.add_argument('--glb', required=True)
p.add_argument('--out', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=a.glb)
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
print('TRIS', sum(len(m.data.polygons) for m in meshes) if all(len(poly.vertices) == 3 for m in meshes for poly in m.data.polygons[:100]) else sum(sum(len(poly.vertices) - 2 for poly in m.data.polygons) for m in meshes))
for im in bpy.data.images:
    print('TEX', im.name, tuple(im.size))
pts = [m.matrix_world @ Vector(c) for m in meshes for c in m.bound_box]
lo = Vector([min(v[i] for v in pts) for i in range(3)]); hi = Vector([max(v[i] for v in pts) for i in range(3)])
print('BOUNDS', tuple(round(x, 3) for x in lo), tuple(round(x, 3) for x in hi))
h = hi.z - lo.z; cx = (lo.x + hi.x) / 2; cy = (lo.y + hi.y) / 2
s = bpy.context.scene
s.render.engine = 'BLENDER_WORKBENCH'
s.render.resolution_x, s.render.resolution_y = 2048, 2048
s.render.film_transparent = False
s.world = bpy.data.worlds.new('w'); s.world.color = (.5, .5, .52)
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); s.collection.objects.link(cam); s.camera = cam
cam.data.type = 'ORTHO'
def shoot(name, yaw, centre, ortho, mode):
    sh = s.display.shading
    if mode == 'tex': sh.light = 'FLAT'; sh.color_type = 'TEXTURE'
    else: sh.light = 'STUDIO'; sh.color_type = 'SINGLE'; sh.single_color = (.75, .72, .68); sh.show_specular_highlight = True
    cam.data.ortho_scale = ortho
    d = 20; r = math.radians(yaw)
    cam.location = (centre.x + d * math.sin(r), centre.y - d * math.cos(r), centre.z)  # camera looks along (-sin r, cos r)
    cam.rotation_euler = (math.radians(90), 0, r)
    s.render.filepath = str(out / f'{name}-{mode}.png'); bpy.ops.render.render(write_still=True)
mid = Vector((cx, cy, lo.z + h / 2))
w = hi.x - lo.x
at = lambda dx, fz: Vector((lo.x + dx * w, cy, lo.z + h * fz))
for mode in ('tex', 'clay'):
    for name, yaw in (('front', 180), ('left', -90), ('back', 0), ('right', 90)):
        shoot(name, yaw, mid, h * 1.08, mode)
    shoot('face', 180, Vector((cx, cy, lo.z + h * .92)), h * .16, mode)
    shoot('torso', 180, Vector((cx, cy, lo.z + h * .68)), h * .3, mode)
    shoot('hand-a', 180, at(.06, .54), h * .16, mode)
    shoot('hand-b', 180, at(.94, .54), h * .16, mode)
    shoot('feet-front', 180, Vector((cx, cy, lo.z + h * .07)), h * .2, mode)
    shoot('feet-back', 0, Vector((cx, cy, lo.z + h * .07)), h * .2, mode)
    shoot('feet-side', -90, Vector((cx, cy, lo.z + h * .07)), h * .2, mode)
