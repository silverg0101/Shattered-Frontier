"""CPU unlit, fixed-view material guides from an exact saved model revision."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
from mathutils import Vector

p = argparse.ArgumentParser(description=__doc__)
for key in ['source', 'profile', 'output']:
    p.add_argument('--'+key, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
source, profile_path, out = [Path(value).resolve() for value in [a.source, a.profile, a.output]]
profile = json.loads(profile_path.read_text())
assert profile['schema_version'] == 1
assert hashlib.sha256(source.read_bytes()).hexdigest() == profile['source_sha256']
assert type(profile['resolution']) is int and 512 <= profile['resolution'] <= 4096
out.mkdir(parents=True, exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out/'profile.json').write_bytes(profile_path.read_bytes())
bpy.ops.wm.open_mainfile(filepath=str(source))
scene = bpy.context.scene
scene.frame_set(1)
obj = bpy.data.objects[profile['object']]
for modifier in obj.modifiers:
    if modifier.type == 'ARMATURE':
        modifier.object.data.pose_position = 'REST'
for item in scene.objects:
    if item.type in ['MESH', 'FONT', 'CURVE']:
        item.hide_render = item != obj
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 1
scene.cycles.use_denoising = False
if hasattr(scene.cycles, 'denoising_use_gpu'):
    scene.cycles.denoising_use_gpu = False
scene.render.resolution_x = scene.render.resolution_y = profile['resolution']
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.film_transparent = False
scene.view_settings.view_transform = 'Standard'
scene.view_settings.look = 'None'
scene.view_settings.exposure = 0
scene.view_settings.gamma = 1
scene.world = bpy.data.worlds.new('Unlit guide background')
scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.12, .12, .12, 1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value = 1
for material in obj.data.materials:
    assert material.use_nodes
    nodes, links = material.node_tree.nodes, material.node_tree.links
    bsdf = next(node for node in nodes if node.type == 'BSDF_PRINCIPLED')
    socket = bsdf.inputs['Base Color']
    emission = nodes.new('ShaderNodeEmission')
    if socket.is_linked:
        links.new(socket.links[0].from_socket, emission.inputs['Color'])
    else:
        emission.inputs['Color'].default_value = socket.default_value
    output = next(node for node in nodes if node.type == 'OUTPUT_MATERIAL' and node.is_active_output)
    links.new(emission.outputs[0], output.inputs['Surface'])
obj.data.calc_loop_triangles()
import numpy as np
vertices = np.array([v.co[:] for v in obj.data.vertices], dtype=np.float32)
faces = np.array([tuple(t.vertices) for t in obj.data.loop_triangles], dtype=np.int32)
mesh_hash = hashlib.sha256(vertices.tobytes()+faces.tobytes()).hexdigest()
camera = bpy.data.objects.new('Fixed material guide', bpy.data.cameras.new('Fixed material guide'))
scene.collection.objects.link(camera)
scene.camera = camera
records = []
for view in profile['views']:
    assert .1 <= view['scale_m'] <= 10
    target, offset = Vector(view['target_m']), Vector(view['camera_offset_m'])
    assert offset.length > 1
    camera.location = target+offset
    camera.rotation_euler = (-offset).to_track_quat('-Z', 'Y').to_euler()
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = view['scale_m']
    bpy.context.view_layer.update()
    image_path = out/(view['name']+'-BaseColor.png')
    scene.render.filepath = str(image_path)
    bpy.ops.render.render(write_still=True)
    assert image_path.is_file()
    records.append(dict(**view, image=image_path.name, image_sha256=hashlib.sha256(image_path.read_bytes()).hexdigest(),
                        camera_matrix_world=[list(row) for row in camera.matrix_world]))
report = dict(schema_version=1, source=str(source), source_sha256=profile['source_sha256'],
              mesh_hash=mesh_hash, object=obj.name, profile_sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest(),
              resolution=profile['resolution'], cpu_only=True, color_space='sRGB display PNG; unlit base color',
              pose='REST; frame1 assembly transform', views=records)
(out/'projection-guides.json').write_text(json.dumps(report, indent=2))
print(json.dumps(dict(status='guides-only', views=len(records), output=str(out))))
