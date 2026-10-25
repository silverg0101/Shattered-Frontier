"""One 2048 px clay sheet of a mesh (GLB/FBX/OBJ): front, left, back, right, top, bottom, flat grey with matcap light so bridging, filled openings and baked shadows show.
  blender --factory-startup -b --python tools/Render-ClaySheet.py -- <mesh> <out.png> [--tex]   (--tex also shows the base colour)
"""
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector

src, out = (os.path.abspath(p) for p in sys.argv[sys.argv.index('--') + 1:][:2])
tex = '--tex' in sys.argv
ext = os.path.splitext(src)[1].lower()
bpy.ops.wm.read_factory_settings(use_empty=True)
if ext == '.blend':  # a cleaned garment .blend (cm, skinned): keep the mesh only, in metres, so the sheet and --section frame match the GLB path
    bpy.ops.wm.open_mainfile(filepath=src)
    for o in [o for o in bpy.data.objects if o.type != 'MESH']:
        bpy.data.objects.remove(o)
    for o in bpy.data.objects:
        o.parent = None; o.modifiers.clear(); o.matrix_world.identity(); o.data.transform(__import__('mathutils').Matrix.Scale(0.01, 4))
    tmp = os.path.join(os.environ.get('TEMP', '.'), 'claysheet_tmp.glb')  # round-trip through a GLB: the loaded scene's own render settings garbled the tiles
    bpy.ops.export_scene.gltf(filepath=tmp, export_apply=False)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    src, ext = tmp, '.glb'
{'.glb': lambda: bpy.ops.import_scene.gltf(filepath=src), '.fbx': lambda: bpy.ops.import_scene.fbx(filepath=src),
 '.obj': lambda: bpy.ops.wm.obj_import(filepath=src)}[ext]()
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
pts = [o.matrix_world @ v.co for o in meshes for v in o.data.vertices]
lo, hi = Vector(map(min, zip(*pts))), Vector(map(max, zip(*pts)))
ctr = (lo + hi) / 2
sc = bpy.context.scene
sc.render.engine = 'BLENDER_WORKBENCH'
sh = sc.display.shading
sh.light = 'MATCAP'; sh.studio_light = 'basic_side.exr' if False else sh.studio_light
sh.color_type = 'TEXTURE' if tex else 'SINGLE'; sh.single_color = (0.62, 0.62, 0.62)
sh.show_backface_culling = False
sc.render.resolution_x = sc.render.resolution_y = 682
sc.world = bpy.data.worlds.new('w'); sc.world.color = (1, 1, 1)
sc.view_settings.view_transform = 'Standard'
cam = bpy.data.objects.new('c', bpy.data.cameras.new('c')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.type = 'ORTHO'; cam.data.ortho_scale = max(hi - lo) * 1.15; cam.data.clip_end = 100000; cam.data.clip_start = 0.001
R = max(hi - lo) * 4
views = [('front', (0, -1, 0)), ('left', (1, 0, 0)), ('back', (0, 1, 0)), ('right', (-1, 0, 0)), ('top', (0, 0, 1)), ('bottom', (0, 0, -1))]
tiles = []
for n, d in views:
    d = Vector(d)
    cam.location = ctr + d * R
    cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = out + f'.{n}.png'
    bpy.ops.render.render(write_still=True)
    im = bpy.data.images.load(sc.render.filepath)
    tiles.append(np.array(im.pixels[:]).reshape(682, 682, 4)); bpy.data.images.remove(im); os.remove(sc.render.filepath)
sheet = np.concatenate([np.concatenate(tiles[3:], axis=1), np.concatenate(tiles[:3], axis=1)], axis=0)  # image rows run bottom-up
img = bpy.data.images.new('sheet', 2046, 1364, alpha=False)
img.pixels.foreach_set(sheet.astype(np.float32).ravel()); img.filepath_raw = os.path.abspath(out); img.file_format = 'PNG'; img.save()
print('CLAYSHEET', out)

if '--section' in sys.argv:  # a cut-away through the middle (x = 0, seen from +x): the hood's inside next to Orlando's head, if the mesh is in his frame (metres)
    import bmesh
    for o in meshes:
        bm = bmesh.new(); bm.from_mesh(o.data); bm.transform(o.matrix_world)
        bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(ctr.x, 0, 0), plane_no=(1, 0, 0), clear_outer=True)
        o.parent = None; o.matrix_world.identity(); bm.to_mesh(o.data); bm.free()
    head_obj = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Artifacts/Characters/PlayerBase/Orlando_head.obj')
    lines = open(head_obj).read().splitlines()
    hv = [(float(x) * .01 * (1 if '--cm' not in sys.argv else 1), -float(y) * .01, float(z) * .01) for _, x, y, z in (l.split() for l in lines if l.startswith('v '))]
    hf = [[int(i.split('/')[0]) - 1 for i in l.split()[1:4]] for l in lines if l.startswith('f ')]
    hm = bpy.data.meshes.new('head'); hm.from_pydata(hv, [], hf); hm.update()
    bm = bmesh.new(); bm.from_mesh(hm)
    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0, 0, 0), plane_no=(1, 0, 0), clear_outer=True)
    bm.to_mesh(hm); bm.free()
    ho = bpy.data.objects.new('head', hm); sc.collection.objects.link(ho)
    hmat = bpy.data.materials.new('headmat'); hmat.diffuse_color = (0.9, 0.55, 0.45, 1); ho.data.materials.append(hmat)
    sh.color_type = 'MATERIAL'
    for o in meshes:
        if not o.data.materials:
            m2 = bpy.data.materials.new('m2'); m2.diffuse_color = (0.45, 0.35, 0.2, 1); o.data.materials.append(m2)
    cam.location = Vector((5, ctr.y, ctr.z)); cam.rotation_euler = (math.radians(90), 0, math.radians(90))
    sc.render.resolution_x = sc.render.resolution_y = 1024
    cam.data.ortho_scale = (hi.z - lo.z) * 1.2
    sc.render.filepath = out + '.section.png'; bpy.ops.render.render(write_still=True)
    print('SECTION', sc.render.filepath)
