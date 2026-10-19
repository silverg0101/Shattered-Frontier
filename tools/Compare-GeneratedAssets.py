"""Create a Blender A/B scene with aligned variants toggled in the Outliner."""
import argparse
from pathlib import Path
import sys
import bpy

p = argparse.ArgumentParser()
p.add_argument('--baseline', required=True, help='Reviewed baseline .blend')
p.add_argument('--baseline-label', default='Baseline 1024 - 12 steps - 300k')
p.add_argument('--clay-viewport', action='store_true', help='Open with uniform solid shading for geometry comparison')
p.add_argument('--candidate', action='append', default=[], help='Reviewed candidate folder, repeatable')
p.add_argument('--output', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
output = Path(a.output).resolve()
if output.exists():
    raise RuntimeError('Choose a new comparison file')
bpy.ops.wm.open_mainfile(filepath=str(Path(a.baseline).resolve()))
baseline = bpy.data.collections['Voidreign | Generated asset']
baseline.name = 'A | '+a.baseline_label
for index, candidate in enumerate(a.candidate, start=1):
    folder = Path(candidate).resolve()
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(folder / 'Voidreign.glb'))
    imported = set(bpy.data.objects) - before
    collection = bpy.data.collections.new(f'{chr(65 + index)} | {folder.name}')
    bpy.context.scene.collection.children.link(collection)
    for obj in imported:
        for old in list(obj.users_collection):
            old.objects.unlink(obj)
        collection.objects.link(obj)
    bpy.context.view_layer.layer_collection.children[collection.name].hide_viewport = True
    collection.hide_render = True
bpy.ops.object.select_all(action='DESELECT')
if a.clay_viewport:
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type=='VIEW_3D':
                shading=area.spaces.active.shading
                shading.type='SOLID';shading.color_type='SINGLE';shading.single_color=(.5,.5,.5)
                shading.light='STUDIO';shading.show_shadows=True;shading.show_cavity=True;shading.cavity_type='BOTH'
for obj in baseline.all_objects:
    if obj.type == 'MESH':
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
bpy.ops.file.pack_all()
bpy.context.scene.render.filepath = str(output.with_suffix('.png'))
bpy.ops.wm.save_as_mainfile(filepath=str(output))
