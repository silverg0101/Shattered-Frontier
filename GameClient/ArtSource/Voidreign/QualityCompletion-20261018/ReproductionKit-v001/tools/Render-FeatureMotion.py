"""CPU Cycles playback evidence from a saved animated source; no source edits."""
import argparse
import hashlib
import json
import sys
from pathlib import Path
import bpy
from mathutils import Vector

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', required=True)
p.add_argument('--object', required=True)
p.add_argument('--output', required=True)
p.add_argument('--frame', action='append', type=int)
p.add_argument('--stride', type=int, default=3)
p.add_argument('--samples', type=int, default=8)
p.add_argument('--diagnostic', action='store_true')
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert 1 <= a.stride <= 15 and 4 <= a.samples <= 128
source, out = Path(a.source).resolve(), Path(a.output).resolve()
out.mkdir(parents=True, exist_ok=False)
bpy.ops.wm.open_mainfile(filepath=str(source.resolve()))
scene = bpy.context.scene
obj = bpy.data.objects[a.object]
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = a.samples
scene.cycles.use_denoising = True
if hasattr(scene.cycles, 'denoising_use_gpu'):
    scene.cycles.denoising_use_gpu = False
scene.render.resolution_x, scene.render.resolution_y = 960, 720
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.world = bpy.data.worlds.new('Motion inspection world')
scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.32, .34, .38, 1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value = .65
scene.view_settings.view_transform = 'AgX'
scene.view_settings.exposure = .8
for other in scene.objects:
    if other.type in ['LIGHT', 'FONT'] or other.name == 'Ground':
        other.hide_render = True
for name, position, power in [('Key', (3, -4, 7), 1600), ('Fill', (-4, -1, 5), 1300),
                               ('Rim', (0, 3, 6), 1000)]:
    data = bpy.data.lights.new('Motion '+name, 'AREA')
    data.energy, data.shape, data.size = power, 'DISK', 5
    light = bpy.data.objects.new(data.name, data)
    scene.collection.objects.link(light)
    light.location = position
    light.rotation_euler = (Vector((0, 0, 1.3))-light.location).to_track_quat('-Z', 'Y').to_euler()
bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -.015))
floor = bpy.context.object
floor.name = 'Motion inspection floor'
mat = bpy.data.materials.new('Motion inspection floor')
mat.diffuse_color = (.19, .21, .24, 1)
mat.use_nodes = True
mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = mat.diffuse_color
mat.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = .85
floor.data.materials.append(mat)
if a.diagnostic:
    mat = bpy.data.materials.new('Actual weighted mesh diagnostic')
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value = .65
    bsdf.inputs['Metallic'].default_value = 0
    base = None
    for feature, color in [('Mane', (.025, .65, .60, 1)), ('Tail', (.9, .25, .025, 1))]:
        attr = nodes.new('ShaderNodeVertexColor')
        attr.layer_name = 'Motion | '+feature
        mix = nodes.new('ShaderNodeMixRGB')
        mix.inputs[1].default_value = (.18, .19, .22, 1)
        mix.inputs[2].default_value = color
        links.new(attr.outputs['Color'], mix.inputs[0])
        if base:
            links.new(base, mix.inputs[1])
        base = mix.outputs[0]
    links.new(base, bsdf.inputs['Base Color'])
    for index in range(len(obj.data.materials)):
        obj.data.materials[index] = mat
camera = bpy.data.objects['Beauty']
scene.camera = camera


def text(name, body, y, size):
    data = bpy.data.curves.new(name, 'FONT')
    data.body, data.size = body, size
    item = bpy.data.objects.new(name, data)
    scene.collection.objects.link(item)
    item.parent = camera
    item.location = (-2.8, y, -4)
    material = bpy.data.materials.get('Motion inspection text')
    if material is None:
        material = bpy.data.materials.new('Motion inspection text')
        material.use_nodes = True
        nodes, links = material.node_tree.nodes, material.node_tree.links
        nodes.clear()
        em = nodes.new('ShaderNodeEmission')
        em.inputs[0].default_value = (.015, .018, .025, 1)
        output = nodes.new('ShaderNodeOutputMaterial')
        links.new(em.outputs[0], output.inputs['Surface'])
    data.materials.append(material)
    return data


text('Motion title', 'ACTUAL MESH | COMPLETE-HAIR RIG CANDIDATE', 1.97, .12)
armature = next(modifier.object for modifier in obj.modifiers if modifier.type == 'ARMATURE')
chains = json.loads(armature['Logical guide chains'])
text('Motion scope', f'{len(chains)} guides / {len(armature.data.bones)} bones | rigid horse harness; legs stay static', 1.78, .095)
phase = text('Motion phase', '', 1.61, .11)
if a.diagnostic:
    text('Motion legend', 'Cyan: weighted mane    Amber: weighted tail    Gray: rigid / anchored', -1.90, .09)
text('Motion review', 'Offline bake. Attachment proposals and surface clearance under review.', -2.08, .085)
frames = a.frame or list(range(scene.frame_start, scene.frame_end+1, a.stride))
assert all(scene.frame_start <= frame <= scene.frame_end for frame in frames)
records = []
for index, frame in enumerate(frames, 1):
    scene.frame_set(frame)
    t = (frame-1)/scene.render.fps
    label = ('SETTLED' if frame < 31 else 'WALK RESPONSE' if frame < 91 else
             'RUN RESPONSE' if frame < 151 else 'TURN RESPONSE' if frame < 211 else
             'STOP / SETTLE' if frame < 271 else 'RIDER CONTACT')
    phase.body = f'{label} | {t:.2f}s'
    scene.render.filepath = str(out/f'{index:04}.png')
    bpy.ops.render.render(write_still=True)
    assert Path(scene.render.filepath).is_file(), 'Render did not reach the requested output path'
    records.append(dict(source_frame=frame, image=Path(scene.render.filepath).name))
    print('CPU motion frame', index, '/', len(frames), '| source', frame, flush=True)
(out/'render-report.json').write_text(json.dumps(dict(source=str(source.resolve()),
    source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    cpu_only=True, engine='Cycles', device='CPU', samples=a.samples,
    diagnostic=a.diagnostic, playback_fps=scene.render.fps/a.stride if not a.frame else None,
    frames=records, scope='Saved-source playback evidence, not a gait/runtime/collision acceptance'), indent=2))
