"""CPU diagnostic copies of frozen skin proposals; never save/edit source skin."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Vector

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda:stream.read(8*1024*1024),b''):h.update(chunk)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
assert 1<=len(cfg['cases'])<=8 and 512<=cfg['resolution']<=1024 and 1<=cfg['samples']<=32
assert .05<=cfg['context_radius_m']<=.3 and 0<cfg['baseline_tolerance_m']<=1e-5
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
contract=json.loads(Path(cfg['contract']).read_text())
assert sha(contract['selection'])==contract['selection_sha256'] and sha(contract['rest_selection'])==contract['rest_selection_sha256']
parts=np.load(contract['selection'])
with np.load(contract['rest_selection']) as ar:faces=ar['triangles']
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
scene=bpy.context.scene;obj=bpy.data.objects[cfg['object']];rig=bpy.data.objects[cfg['rig_object']]
mods=[m for m in obj.modifiers if m.show_viewport]
assert len(mods)==1 and mods[0].type=='ARMATURE' and mods[0].object==rig
assert not mods[0].use_deform_preserve_volume and not mods[0].use_bone_envelopes
assert all(b.bbone_segments==1 for b in rig.data.bones)
assert len(obj.data.polygons)==len(faces) and np.array_equal(faces,np.array([p.vertices[:] for p in obj.data.polygons]))
names=[b.name for b in rig.data.bones if b.use_deform];lookup={n:i for i,n in enumerate(names)}
weights=np.zeros((len(obj.data.vertices),len(names)))
for v in obj.data.vertices:
    for g in v.groups:
        name=obj.vertex_groups[g.group].name
        if name in lookup:weights[v.index,lookup[name]]=g.weight
coords=np.array([v.co[:] for v in obj.data.vertices]);hom=np.column_stack([coords,np.ones(len(coords))])
def deform(frame,indices,field):
    scene.frame_set(frame)
    matrices=np.array([rig.matrix_world@rig.pose.bones[n].matrix@rig.data.bones[n].matrix_local.inverted()@rig.matrix_world.inverted()@obj.matrix_world for n in names])
    points=np.einsum('bij,vj->vbi',matrices[:,:3],hom[indices])
    result=np.einsum('vb,vbi->vi',field[indices],points)
    unweighted=field[indices].sum(1)<=1e-10
    result[unweighted]=(hom[indices[unweighted]]@np.array(obj.matrix_world).T)[:,:3]
    return result
prepared=[];arrays={}
for case in cfg['cases']:
    report=json.loads(Path(case['report']).read_text());assert report['source_sha256']==cfg['input_sha256'][cfg['source']]
    assert report['status']=='read-only-rigid-skin-preflight-not-a-candidate'
    with np.load(case['arrays']) as ar:
        assert names==ar['bone_names'].tolist()
        proposed=weights.copy();proposed[ar['changed_vertices']]=ar['weights']
    edge=np.array(case['vertices'],int);assert edge.shape==(2,)
    assert np.any((faces==edge[0]).any(1)&(faces==edge[1]).any(1))
    rest_length=float(np.linalg.norm(coords[edge[0]]-coords[edge[1]]))
    curve=[]
    for frame in range(cfg['frame_start'],cfg['frame_end']+1):
        point=deform(frame,edge,proposed)
        curve.append(abs(float(np.linalg.norm(point[0]-point[1]))-rest_length))
    frame=cfg['frame_start']+int(np.argmax(curve))
    center=coords[edge].mean(0)
    selected=np.linalg.norm(coords-center,axis=1)<=cfg['context_radius_m']
    face_ids=np.flatnonzero(selected[faces].all(1));assert len(face_ids)>0
    vertices,inverse=np.unique(faces[face_ids],return_inverse=True);local_faces=inverse.reshape((-1,3))
    mass=weights[vertices].sum(1)
    assert np.isin(edge,vertices).all() and np.all((abs(mass-1)<1e-5)|(mass<=1e-10))
    baseline=deform(frame,vertices,weights);counter=deform(frame,vertices,proposed)
    ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    actual=np.array([(obj.matrix_world@ev.data.vertices[int(i)].co)[:] for i in vertices])
    error=float(np.linalg.norm(actual-baseline,axis=1).max());assert error<=cfg['baseline_tolerance_m'],error
    rest_world=(hom[vertices]@np.array(obj.matrix_world).T)[:,:3]
    label=np.zeros(len(coords),int)
    for foot in contract['feet']:
        label[parts[foot['core_key']]]=1;label[parts[foot['collar_key']]]=2
    face_labels=np.zeros(len(local_faces),int)
    for i,f in enumerate(faces[face_ids]):face_labels[i]=int(np.bincount(label[f],minlength=3).argmax())
    local_edge=np.searchsorted(vertices,edge)
    prefix=case['id'];arrays[prefix+'.source_vertices']=vertices;arrays[prefix+'.source_faces']=face_ids
    arrays[prefix+'.rest_world']=rest_world;arrays[prefix+'.baseline_world']=baseline;arrays[prefix+'.proposal_world']=counter
    prepared.append(dict(id=prefix,frame=frame,edge=edge.tolist(),rest_length_m=rest_length,baseline_edge_change_m=abs(float(np.linalg.norm(baseline[local_edge[0]]-baseline[local_edge[1]]))-rest_length),proposal_edge_change_m=curve[frame-cfg['frame_start']],baseline_position_error_m=error,unweighted_context_vertices=int((mass<=1e-10).sum()),vertices=vertices,faces=local_faces,labels=face_labels,points=[rest_world,baseline,counter],edge_local=local_edge))
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes());np.savez_compressed(out/'source-indexed-pose-copies.npz',**arrays)
# Hide the loaded scene; render isolated exact-index copies only.
for ob in scene.objects:ob.hide_render=True
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=cfg['samples'];scene.cycles.use_denoising=False
scene.render.resolution_x=scene.render.resolution_y=cfg['resolution'];scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
scene.world=bpy.data.worlds.new('Diagnostic world');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.3,.3,.3,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.7
materials=[]
for name,color in [('Unchanged exterior',(.35,.35,.35,1)),('Rigid core',(.03,.4,.7,1)),('Transition collar',(.9,.32,.03,1)),('Measured edge',(.9,.015,.015,1))]:
    mat=bpy.data.materials.new(name);mat.diffuse_color=color;mat.use_nodes=True;bsdf=mat.node_tree.nodes.get('Principled BSDF');bsdf.inputs['Base Color'].default_value=color;bsdf.inputs['Roughness'].default_value=.7;materials.append(mat)
bpy.ops.object.camera_add();camera=bpy.context.object;scene.camera=camera;camera.data.type='ORTHO';camera.data.ortho_scale=cfg['context_radius_m']*2.6
bpy.ops.object.light_add(type='AREA');light=bpy.context.object;light.data.energy=100;light.data.size=.5
records=[]
for row in prepared:
    local_edge=row['edge_local'];images=[]
    for kind,points in zip(['rest','baseline','proposal'],row['points']):
        mesh=bpy.data.meshes.new('Exact indexed pose copy');mesh.from_pydata(points.tolist(),[],row['faces'].tolist());mesh.update()
        ob=bpy.data.objects.new('Diagnostic '+row['id']+' '+kind,mesh);scene.collection.objects.link(ob)
        for mat in materials:mesh.materials.append(mat)
        for poly,label in zip(mesh.polygons,row['labels']):poly.material_index=int(label)
        start,end=(Vector(points[int(i)]) for i in local_edge);delta=end-start
        bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=.0007,depth=delta.length,location=(start+end)/2)
        marker=bpy.context.object;marker.rotation_euler=delta.to_track_quat('Z','Y').to_euler();marker.data.materials.append(materials[3])
        target=Vector(points.mean(0));side=1 if target.x>=0 else -1
        camera.location=target+Vector((side*.65,-.3,.2));camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
        light.location=target+Vector((side*.4,-.3,.5));light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
        path='previews/'+row['id']+'-'+kind+'.png';scene.render.filepath=str(out/path);bpy.ops.render.render(write_still=True);images.append(path)
        bpy.data.objects.remove(ob,do_unlink=True);bpy.data.objects.remove(marker,do_unlink=True)
    records.append({k:v for k,v in row.items() if k not in ['vertices','faces','labels','points','edge_local']}|dict(images=images,faces=len(row['faces']),vertices=len(row['vertices'])))
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
(out/'pose-review.json').write_text(json.dumps(dict(schema_version=1,status='diagnostic-pose-copies-needs-review',cpu_only=True,input_sha256=cfg['input_sha256'],profile_sha256=sha(profile),tool_sha256=sha(__file__),cases=records,limitations=['Temporary exact-index isolated copies, not a saved skin/rig/gait candidate.','Blue core, orange collar, gray exterior; majority face color is diagnostic only.','Context sphere cuts geometry and may include overlapping separate shells; not physical part approval.','Selected frame maximizes proposed error on the declared edge; baseline shown at the same frame, not its independent maximum.']),indent=2))
print(json.dumps(dict(status='diagnostic-pose-copies-needs-review',cases=len(records))),flush=True)
