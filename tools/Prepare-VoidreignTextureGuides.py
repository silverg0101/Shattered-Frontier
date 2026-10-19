"""Render fixed orthographic base-color projection guides for local texture edits."""
import bpy,json,sys,argparse
from pathlib import Path
from mathutils import Vector
p=argparse.ArgumentParser();p.add_argument('--source',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);out=Path(a.output);out.mkdir(parents=True,exist_ok=False)
bpy.ops.wm.open_mainfile(filepath=str(Path(a.source).resolve()))
s=bpy.context.scene;s.render.resolution_x=s.render.resolution_y=1024;s.render.resolution_percentage=100
s.view_settings.view_transform='Standard';s.view_settings.look='None';s.view_settings.exposure=0;s.view_settings.gamma=1
for obj in s.objects:
    if obj.type=='MESH' and obj.name=='Ground':obj.hide_render=True
for mat in bpy.data.materials:
    if not mat.use_nodes:continue
    nodes=mat.node_tree.nodes;bsdf=nodes.get('Principled BSDF')
    if not bsdf:continue
    socket=bsdf.inputs['Base Color'];links=list(socket.links)
    emission=nodes.new('ShaderNodeEmission')
    if links:mat.node_tree.links.new(links[0].from_socket,emission.inputs['Color'])
    else:emission.inputs['Color'].default_value=socket.default_value
    output=next(n for n in nodes if n.type=='OUTPUT_MATERIAL')
    mat.node_tree.links.new(emission.outputs[0],output.inputs['Surface'])
s.world.color=(.12,.12,.12)
specs=[dict(name='Head',target=[0,-1.15,2.02],scale=1.65),dict(name='Saddle',target=[0,-.05,1.65],scale=1.35)]
for spec in specs:
    camera=bpy.data.objects['Side'];target=Vector(spec['target']);camera.location=target+Vector((7,0,0))
    camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.ortho_scale=spec['scale']
    s.camera=camera;s.render.filepath=str(out/(spec['name']+'-BaseColor.png'))
    bpy.ops.render.render(write_still=True)
    spec['camera_matrix_world']=[list(row) for row in camera.matrix_world]
(out/'projection-guides.json').write_text(json.dumps(specs,indent=2))
