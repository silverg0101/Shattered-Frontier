"""Render identical head and saddle views for reviewed texture variants."""
import argparse,json,sys,hashlib
from pathlib import Path
import bpy
from mathutils import Matrix
p=argparse.ArgumentParser();target=p.add_mutually_exclusive_group(required=True);target.add_argument('--folder',action='append');target.add_argument('--source');p.add_argument('--output');p.add_argument('--guides',required=True)
p.add_argument('--frame',type=int,default=1);p.add_argument('--samples',type=int,default=32)
p.add_argument('--channel',choices=['shaded','base-color','roughness','metallic'],default='shaded');p.add_argument('--material')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);specs=json.loads(Path(a.guides).read_text())
assert 1<=a.frame<=10000 and 1<=a.samples<=128
assert a.channel=='shaded' or (a.source and a.material),'Channel diagnostics require a named material and new output'
if a.source:
    assert a.output and not Path(a.output).exists()
    destinations=[(Path(a.source).resolve(),Path(a.output).resolve())]
else:
    assert not a.output
    destinations=[(Path(folder)/'Voidreign.blend',Path(folder)) for folder in a.folder]
for source,folder in destinations:
    bpy.ops.wm.open_mainfile(filepath=str(source));s=bpy.context.scene
    if a.source:
        folder.mkdir(parents=True);(folder/'tool-source.py').write_bytes(Path(__file__).read_bytes());(folder/'guides.json').write_bytes(Path(a.guides).read_bytes())
        s.frame_set(a.frame);s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=a.samples
    channel_trace=None
    if a.channel!='shaded':
        mat=bpy.data.materials[a.material];assert mat.use_nodes
        tree=mat.node_tree
        outputs=[n for n in tree.nodes if n.type=='OUTPUT_MATERIAL' and n.is_active_output];assert len(outputs)==1
        surface=outputs[0].inputs['Surface'];assert len(surface.links)==1
        shader=surface.links[0].from_node;assert shader.type=='BSDF_PRINCIPLED','Direct Principled surface required'
        socket=shader.inputs[{'base-color':'Base Color','roughness':'Roughness','metallic':'Metallic'}[a.channel]]
        emission=tree.nodes.new('ShaderNodeEmission');emission.inputs['Strength'].default_value=1
        if socket.is_linked:
            assert len(socket.links)==1
            link=socket.links[0];tree.links.new(link.from_socket,emission.inputs['Color'])
            channel_trace=dict(node=link.from_node.name,socket=link.from_socket.name)
        else:
            value=socket.default_value
            emission.inputs['Color'].default_value=(*([float(value)]*3),1) if isinstance(value,(int,float)) else value
            channel_trace=dict(constant=list(emission.inputs['Color'].default_value))
        tree.links.new(emission.outputs['Emission'],surface)
        # Data preview: no tone mapping or exposure. Image-source color spaces retained.
        s.view_settings.view_transform='Standard';s.view_settings.look='None';s.view_settings.exposure=0;s.view_settings.gamma=1
    s.render.resolution_x=s.render.resolution_y=1024;s.render.resolution_percentage=100
    s.view_settings.exposure=0
    lights={o.name:o.matrix_world.copy() for o in s.objects if o.type=='LIGHT'};views=[]
    for spec in specs:
        for name,matrix in lights.items():bpy.data.objects[name].matrix_world=matrix
        if spec.get('mirror_lights_axis') is not None:
            from mathutils import Vector
            axis=spec['mirror_lights_axis'];assert axis in [0,1,2]
            for name in lights:
                light=bpy.data.objects[name];point=light.matrix_world.translation.copy();point[axis]=2*spec['target'][axis]-point[axis]
                light.location=light.parent.matrix_world.inverted()@point if light.parent else point
                direction=Vector(spec['target'])-point;world=light.matrix_world.copy();world.translation=point
                world=direction.to_track_quat('-Z','Y').to_matrix().to_4x4();world.translation=point;light.matrix_world=world
        camera=bpy.data.objects.new('Closeup '+spec['name'],bpy.data.cameras.new('Closeup '+spec['name']))
        s.collection.objects.link(camera);camera.matrix_world=Matrix(spec['camera_matrix_world']);camera.data.type='ORTHO';camera.data.ortho_scale=spec['scale']
        s.camera=camera;s.render.filepath=str(folder/'previews'/('Detail-'+spec['name']+'.png'));bpy.ops.render.render(write_still=True)
        views.append(dict(name=spec['name'],image='previews/Detail-'+spec['name']+'.png',camera_matrix_world=[list(r) for r in camera.matrix_world],mirror_lights_axis=spec.get('mirror_lights_axis')))
    if a.source:
        def sha(path):
            h=hashlib.sha256()
            with Path(path).open('rb') as stream:
                for chunk in iter(lambda:stream.read(8*1024*1024),b''):h.update(chunk)
            return h.hexdigest()
        (folder/'render-report.json').write_text(json.dumps(dict(status='saved-source-material-closeups-needs-review',cpu_only=True,source=str(source),source_sha256=sha(source),guides_sha256=sha(a.guides),tool_sha256=sha(__file__),frame=a.frame,samples=a.samples,channel=a.channel,material=a.material,channel_trace=channel_trace,views=views,limitations=['Diagnostic cameras/lights/shader routing in memory only; source unchanged.','Opposing light positions do not constitute calibrated material or full coverage acceptance.','Channel emission previews retain source image color spaces, occlusion and camera projection; not a UV bake or normal/height derivation.']),indent=2))
