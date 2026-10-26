"""CPU depth-tested rest guide coverage views; read-only diagnostic source."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Vector

p = argparse.ArgumentParser(description=__doc__)
for key in ['source','guide-report','coverage','output']:
    p.add_argument('--'+key, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
source, guide_path, coverage_path, out = map(Path, [a.source,a.guide_report,a.coverage,a.output])
guide=json.loads(guide_path.read_text(encoding='utf-8'))
coverage=json.loads(coverage_path.read_text(encoding='utf-8'))
assert hashlib.sha256(source.read_bytes()).hexdigest()==guide['source_sha256']==coverage['source_sha256']
assert guide['mesh_hash']==coverage['mesh_hash']
assert hashlib.sha256((coverage_path.parent/'coverage.npz').read_bytes()).hexdigest()==coverage['coverage_file_sha256']
bpy.ops.wm.open_mainfile(filepath=str(source.resolve()))
scene=bpy.context.scene
scene.frame_set(1)
assert scene.unit_settings.scale_length==1., 'Metre adapter required'
obj=bpy.data.objects[guide['object']]
for modifier in obj.modifiers:
    assert modifier.type=='ARMATURE', 'Unsupported diagnostic modifier'
    modifier.object.data.pose_position='REST'
for other in scene.objects:
    if other.type in {'MESH','CURVE','FONT','SURFACE','META','LIGHT'}:
        other.hide_render=other!=obj
bpy.context.view_layer.update()
obj.data.calc_loop_triangles()
local=np.array([v.co[:] for v in obj.data.vertices],dtype=np.float32)
faces=np.array([tuple(t.vertices) for t in obj.data.loop_triangles],dtype=np.int32)
assert hashlib.sha256(local.tobytes()+faces.tobytes()).hexdigest()==guide['mesh_hash']
selection_path=Path(guide['selection_path'])
assert hashlib.sha256(selection_path.read_bytes()).hexdigest()==guide['selection_sha256']
selection=json.loads(selection_path.read_text(encoding='utf-8'))
with np.load(selection_path.parent/selection['selection_file'],allow_pickle=False) as data:
    canonical=data['canonical_node']
with np.load(coverage_path.parent/'coverage.npz',allow_pickle=False) as data:
    fields={key:data[key] for key in data.files}
attribute=obj.data.color_attributes.new(name='Diagnostic | rest guide distance',type='FLOAT_COLOR',domain='POINT')
material=bpy.data.materials.new('Diagnostic | rest coverage; not asset material')
material.use_nodes=True
nodes=material.node_tree.nodes; nodes.clear()
output=nodes.new('ShaderNodeOutputMaterial')
emission=nodes.new('ShaderNodeEmission')
color=nodes.new('ShaderNodeVertexColor');color.layer_name=attribute.name
material.node_tree.links.new(color.outputs['Color'],emission.inputs['Color'])
material.node_tree.links.new(emission.outputs[0],output.inputs['Surface'])
obj.data.materials.clear();obj.data.materials.append(material)
for face in obj.data.polygons:face.material_index=0
points=np.array([tuple(obj.matrix_world@Vector(p)) for p in obj.bound_box])
center=Vector((points.min(0)+points.max(0))/2)
extent=points.max(0)-points.min(0)
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=4
scene.cycles.seed=0;scene.cycles.use_denoising=False
scene.render.threads_mode='FIXED';scene.render.threads=4
scene.render.resolution_x=768;scene.render.resolution_y=640;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.render.film_transparent=False
scene.world.color=(.08,.08,.08)
scene.view_settings.view_transform='Standard'
scene.view_settings.look='None';scene.view_settings.exposure=0.;scene.view_settings.gamma=1.
camera_data=bpy.data.cameras.new('Coverage diagnostic camera');camera_data.type='ORTHO'
camera_data.ortho_scale=float(max(extent[2]*1.25,extent[1]*1.25*640/768))
camera=bpy.data.objects.new('Coverage diagnostic camera',camera_data);scene.collection.objects.link(camera);scene.camera=camera
out.mkdir(parents=True,exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
renders=[]
for variant in ['original','proposed']:
    colors=np.tile([.23,.23,.23,1.],(len(local),1))
    for feature in coverage['features']:
        name=feature['feature'];ids=fields[name+'_node_ids'];eligible=fields[name+'_eligible']
        values=fields[name+'_'+variant+'_distance_m']
        per_node=np.tile([.05,.15,.5,1.],(len(np.unique(canonical)),1))
        selected=np.array([[.04,.65,.12,1.] if value<=.05 else [.9,.55,.02,1.] if value<=.10 else [.8,.03,.02,1.] if value<=.20 else [.65,.02,.7,1.] for value in values])
        per_node[ids[eligible]]=selected[eligible]
        member=np.isin(canonical,ids)
        colors[member]=per_node[canonical[member]]
    attribute.data.foreach_set('color',colors.astype(np.float32).ravel())
    bpy.context.view_layer.update()
    for view,direction in [('side-posX',(1,0,0)),('side-negX',(-1,0,0)),('rear',(0,1,0))]:
        camera.location=center+Vector(direction)*8
        camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
        path=out/(variant+'-'+view+'.png');scene.render.filepath=str(path.resolve())
        bpy.ops.render.render(write_still=True)
        renders.append(str(path))
report=dict(status='rest-point-coverage-views-only',cpu_only=True,source_sha256=guide['source_sha256'],mesh_hash=guide['mesh_hash'],
    coverage_sha256=hashlib.sha256(coverage_path.read_bytes()).hexdigest(),tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),renders=renders,
    legend={'gray':'outside selected features','blue':'protected or existing excluded root band','green':'distance<=50mm','amber':'50-100mm','red':'100-200mm','magenta':'>200mm'},
    limitations=['Depth-tested rest point distance only; not a rendered motion/collision pass or semantic/guide path approval.',
        'Diagnostic materials replace source materials in memory only; source file is not saved.','No guide curves drawn; color shows point-to-nearest-segment distances.'])
(out/'render-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report))
