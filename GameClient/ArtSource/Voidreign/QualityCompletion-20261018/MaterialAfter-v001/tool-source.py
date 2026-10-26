"""CPU Cycles fixed-camera rest-material closeups for a mesh-hashed revision."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Matrix, Vector

p = argparse.ArgumentParser(description=__doc__)
for key in ['source', 'guides', 'output']:
    p.add_argument('--'+key, required=True)
p.add_argument('--opposing-light', action='store_true')
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
source, guides_path, out = [Path(value).resolve() for value in [a.source, a.guides, a.output]]
guides = json.loads(guides_path.read_text())
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
bpy.ops.wm.open_mainfile(filepath=str(source))
scene = bpy.context.scene
scene.frame_set(1)
obj = bpy.data.objects[guides['object']]
obj.data.calc_loop_triangles()
vertices = np.array([v.co[:] for v in obj.data.vertices], dtype=np.float32)
faces = np.array([tuple(t.vertices) for t in obj.data.loop_triangles], dtype=np.int32)
assert hashlib.sha256(vertices.tobytes()+faces.tobytes()).hexdigest() == guides['mesh_hash']
for modifier in obj.modifiers:
    if modifier.type == 'ARMATURE':
        modifier.object.data.pose_position = 'REST'
for item in scene.objects:
    if item.type in ['MESH', 'CURVE', 'FONT', 'LIGHT']:
        item.hide_render = item != obj
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 16
scene.cycles.use_denoising = True
if hasattr(scene.cycles, 'denoising_use_gpu'):
    scene.cycles.denoising_use_gpu = False
scene.render.resolution_x = scene.render.resolution_y = 1024
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.view_settings.view_transform = 'AgX'
scene.view_settings.exposure = .6
scene.world = bpy.data.worlds.new('Material review world')
scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.2, .22, .25, 1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value = .5
camera = bpy.data.objects.new('Detail review', bpy.data.cameras.new('Detail review'))
scene.collection.objects.link(camera)
scene.camera = camera
records = []
for spec in guides['views']:
    camera.matrix_world = Matrix(spec['camera_matrix_world'])
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = spec['scale_m']
    target = Vector(spec['target_m'])
    sign = 1 if a.opposing_light else -1
    active_lights = []
    for name, offset, energy, size in [('Key', (sign*3, -3, 4), 1000, 3),
                                      ('Fill', (-sign*3, 1, 1), 450, 4)]:
        data = bpy.data.lights.new('Detail '+name, 'AREA')
        data.energy, data.size = energy, size
        light = bpy.data.objects.new(data.name, data)
        scene.collection.objects.link(light)
        light.location = target+Vector(offset)
        light.rotation_euler = (target-light.location).to_track_quat('-Z', 'Y').to_euler()
        active_lights.append(light)
    path = out/(spec['name']+'.png')
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    assert path.is_file()
    records.append(dict(view=spec['name'], image=path.name, sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    for light in active_lights:
        bpy.data.objects.remove(light, do_unlink=True)
report = dict(source=str(source), source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
              guides_sha256=hashlib.sha256(guides_path.read_bytes()).hexdigest(),
              opposing_light=a.opposing_light, cpu_only=True, pose='REST', frames=records,
              scope='Material appearance inspection only; inherited geometry/motion defects remain')
(out/'render-report.json').write_text(json.dumps(report, indent=2))
