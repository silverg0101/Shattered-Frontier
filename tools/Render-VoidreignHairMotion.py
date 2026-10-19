"""Render a diagnostic clip of the actual skinned Pixal mesh; no replacement hair."""
import bpy,json,sys,argparse
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--folder',required=True);p.add_argument('--frames-directory',default='diagnostic-frames');a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);folder=Path(a.folder).resolve()
bpy.ops.wm.open_mainfile(filepath=str(folder/'Voidreign-HairPrototype.blend'));s=bpy.context.scene
frames=folder/a.frames_directory;frames.mkdir(exist_ok=False)
obj=next(o for o in bpy.data.collections['Horse | original static context'].all_objects if o.type=='MESH')
mat=obj.data.materials[0].copy();mat.name='DIAGNOSTIC | cyan mane amber tail';obj.data.materials[0]=mat
nodes=mat.node_tree.nodes;links=mat.node_tree.links;bsdf=nodes.get('Principled BSDF');base=None
for name,color in [('Mane',(.025,.65,.6,1)),('Tail',(.75,.20,.025,1))]:
    attr=nodes.new('ShaderNodeVertexColor');attr.layer_name='Motion | '+name
    mix=nodes.new('ShaderNodeMixRGB');mix.inputs[1].default_value=(.085,.085,.095,1);mix.inputs[2].default_value=color
    links.new(attr.outputs['Color'],mix.inputs[0])
    if base:links.new(base,mix.inputs[1])
    base=mix.outputs[0]
links.new(base,bsdf.inputs['Base Color']);bsdf.inputs['Metallic'].default_value=0
for link in list(bsdf.inputs['Metallic'].links):links.remove(link)
camera=s.camera
def text(name,body,x,y,size):
    data=bpy.data.curves.new(name,'FONT');data.body=body;data.size=size;obj=bpy.data.objects.new(name,data);s.collection.objects.link(obj);obj.parent=camera;obj.location=(x,y,-4)
    material=bpy.data.materials.get('Diagnostic white')
    if not material:
        material=bpy.data.materials.new('Diagnostic white');material.use_nodes=True;n=material.node_tree.nodes;n.clear();em=n.new('ShaderNodeEmission');em.inputs[0].default_value=(.8,.8,.8,1);out=n.new('ShaderNodeOutputMaterial');material.node_tree.links.new(em.outputs[0],out.inputs[0])
    if name=='Scope':
        material=material.copy();material.name='Diagnostic dark footer';material.node_tree.nodes.get('Emission').inputs[0].default_value=(.015,.015,.015,1)
    data.materials.append(material);return data
text('Title','ACTUAL PIXAL MESH | HAIR MOTION PROTOTYPE',-2.8,1.95,.12)
text('Legend','Cyan: weighted mane    Amber: weighted tail',-2.8,1.75,.105)
caption=text('Phase','',-2.8,1.56,.12)
text('Scope','Offline physics bake. Rigid horse harness; legs stay static.',-2.8,-2.05,.10)
cfg=json.loads((folder/'config.json').read_text());stride=cfg['render_stride_frames']
for index,frame in enumerate(range(1,s.frame_end+1,stride),1):
    s.frame_set(frame);t=frame/s.render.fps
    phase='SETTLE' if t<=1 else 'WALK RESPONSE' if t<=3 else 'RUN RESPONSE' if t<=5 else 'TURN RESPONSE' if t<=7 else 'STOP / SETTLE' if t<=9 else 'RIDER COLLISION PROBE'
    caption.body=f'{phase} | {t:.1f}s'
    s.render.filepath=str(frames/f'{index:04}.png');bpy.ops.render.render(write_still=True)
print('Diagnostic frames complete',flush=True)
