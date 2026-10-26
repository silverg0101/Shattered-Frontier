"""Read-only rooted feature skin extension on an existing primary-rig clip."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(8*1024*1024),b''):h.update(block)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
assert 1<=cfg['frame_start']<=cfg['frame_end']<=1000 and 0<cfg['baseline_tolerance_m']<=1e-5 and 0<cfg['solver_tolerance']<=1e-8
assert 1<=len(cfg['support_bones'])<=4
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
with np.load(cfg['rest_selection']) as ar:faces=ar['triangles'];rest=ar['vertices'].astype(float)
with np.load(cfg['semantic_selection']) as ar:canonical=ar['canonical_node']
with np.load(cfg['region_roots']) as ar:vertices,nodes,root_nodes=ar['source_vertices'],ar['canonical_nodes'],ar['root_nodes'];region_faces=ar['source_faces']
first=np.unique(canonical,return_index=True)[1];lookup=np.full(len(first),-1,int);lookup[nodes]=np.arange(len(nodes))
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()));scene=bpy.context.scene;obj=bpy.data.objects[cfg['object']];rig=bpy.data.objects[cfg['primary_rig']]
mods=[m for m in obj.modifiers if m.show_viewport];assert len(mods)==1 and mods[0].type=='ARMATURE' and mods[0].object==rig
assert not mods[0].use_deform_preserve_volume and not mods[0].use_bone_envelopes and all(b.bbone_segments==1 for b in rig.data.bones)
assert np.array_equal(faces,np.array([f.vertices[:] for f in obj.data.polygons]))
names=cfg['support_bones'];bone_index={n:i for i,n in enumerate(names)};weights=np.zeros((len(obj.data.vertices),len(names)));foreign=np.zeros(len(weights),bool)
for v in obj.data.vertices:
    for g in v.groups:
        name=obj.vertex_groups[g.group].name
        if name in bone_index:weights[v.index,bone_index[name]]=g.weight
        elif name in rig.data.bones and rig.data.bones[name].use_deform and g.weight>1e-10:foreign[v.index]=True
root_local=lookup[root_nodes];unknown=np.setdiff1d(np.arange(len(nodes)),root_local);assert 0<len(unknown)<=2000
node_weights=weights[first[nodes]];assert np.max(abs(weights[vertices]-node_weights[lookup[canonical[vertices]]]))<=1e-7
assert np.max(abs(node_weights[root_local].sum(1)-1))<1e-5 and not foreign[vertices].any()
cf=canonical[faces];all_edges=np.unique(np.sort(np.concatenate([cf[:,[0,1]],cf[:,[1,2]],cf[:,[2,0]]]),axis=1),axis=0);all_edges=all_edges[all_edges[:,0]!=all_edges[:,1]]
internal=all_edges[(lookup[all_edges]>=0).all(1)];length=np.linalg.norm(rest[first[internal[:,0]]]-rest[first[internal[:,1]]],axis=1);assert (length>0).all()
laplacian=np.zeros((len(nodes),len(nodes)))
for (i,j),d in zip(lookup[internal],length):
    value=1/d;laplacian[i,i]+=value;laplacian[j,j]+=value;laplacian[i,j]-=value;laplacian[j,i]-=value
matrix=laplacian[np.ix_(unknown,unknown)];rhs=-laplacian[np.ix_(unknown,root_local)]@node_weights[root_local]
solved=np.linalg.solve(matrix,rhs);residual=float(np.linalg.norm(matrix@solved-rhs)/max(np.linalg.norm(rhs),1.));assert residual<=cfg['solver_tolerance']
assert solved.min()>=-1e-10 and solved.max()<=1+1e-7
new_nodes=node_weights.copy();new_nodes[unknown]=np.clip(solved,0,1)
proposed=weights.copy();proposed[vertices]=new_nodes[lookup[canonical[vertices]]]
assert np.max(abs(proposed[vertices].sum(1)-1))<1e-5
changed=np.flatnonzero(np.max(abs(proposed-weights),axis=1)>1e-10);assert len(changed)>0 and not np.intersect1d(changed,first[root_nodes]).size
source_edges=np.unique(np.sort(np.concatenate([faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]]]),axis=1),axis=0)
changed_mask=np.zeros(len(weights),bool);changed_mask[changed]=True;edges=source_edges[changed_mask[source_edges].any(1)]
checked,inverse=np.unique(edges,return_inverse=True);local_edges=inverse.reshape((-1,2))
assert not foreign[checked].any();mass=weights[checked].sum(1);assert np.all((abs(mass-1)<1e-5)|(mass<1e-10))
coords=np.array([v.co[:] for v in obj.data.vertices]);hom=np.column_stack([coords,np.ones(len(coords))])
scene.frame_set(cfg['frame_start']);world=(hom[checked]@np.array(obj.matrix_world).T)[:,:3];rest_lengths=np.linalg.norm(world[local_edges[:,0]]-world[local_edges[:,1]],axis=1);assert (rest_lengths>0).all()
maxima=np.zeros((2,len(edges)));curves=[];baseline_error=0.;paths=[]
# Include the full selected region in trajectory export, even unchanged root vertices.
traced=np.union1d(checked,vertices);region_lookup=np.searchsorted(traced,vertices);edge_lookup=np.searchsorted(traced,checked)
for frame in range(cfg['frame_start'],cfg['frame_end']+1):
    scene.frame_set(frame);matrices=np.array([rig.matrix_world@rig.pose.bones[n].matrix@rig.data.bones[n].matrix_local.inverted()@rig.matrix_world.inverted()@obj.matrix_world for n in names])
    bone_points=np.einsum('bij,vj->vbi',matrices[:,:3],hom[traced]);normal=np.einsum('vb,vbi->vi',weights[traced],bone_points);counter=np.einsum('vb,vbi->vi',proposed[traced],bone_points)
    unweighted=weights[traced].sum(1)<1e-10;normal[unweighted]=(hom[traced[unweighted]]@np.array(obj.matrix_world).T)[:,:3]
    ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());actual=np.array([(obj.matrix_world@ev.data.vertices[int(i)].co)[:] for i in traced]);baseline_error=max(baseline_error,float(np.linalg.norm(actual-normal,axis=1).max()));assert baseline_error<=cfg['baseline_tolerance_m']
    errors=[]
    for points in (normal[edge_lookup],counter[edge_lookup]):errors.append(abs(np.linalg.norm(points[local_edges[:,0]]-points[local_edges[:,1]],axis=1)-rest_lengths))
    maxima=np.maximum(maxima,errors);curves.append(dict(frame=frame,baseline_maximum_edge_change_m=float(errors[0].max()),proposed_maximum_edge_change_m=float(errors[1].max())))
    paths.append([normal[region_lookup],counter[region_lookup]])
paths=np.array(paths);delta=maxima[1]-maxima[0];order=np.argsort(delta)[-10:][::-1]
out.mkdir(parents=True);np.savez_compressed(out/'parent-skin-proposal.npz',source_vertices=vertices,source_faces=region_faces,changed_vertices=changed,bone_names=np.array(names),proposed_weights=proposed[changed],frames=np.arange(cfg['frame_start'],cfg['frame_end']+1),region_world_points=paths,incident_edges=edges,baseline_edge_maxima=maxima[0],proposed_edge_maxima=maxima[1])
(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
report=dict(schema_version=1,status='read-only-parent-skin-proposal-not-a-candidate',cpu_only=True,input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),feature_vertices=len(vertices),changed_vertices=len(changed),original_unweighted_feature_vertices=int((weights[vertices].sum(1)<1e-10).sum()),incident_edges=len(edges),solver_relative_residual=residual,maximum_baseline_position_error_m=baseline_error,baseline_maximum_edge_change_m=float(maxima[0].max()),proposed_maximum_edge_change_m=float(maxima[1].max()),regressed_edges_above_reporting_floor=int((delta>cfg['delta_reporting_floor_m']).sum()),largest_regressions=[dict(vertices=edges[i].tolist(),baseline_m=float(maxima[0,i]),proposed_m=float(maxima[1,i]),increase_m=float(delta[i])) for i in order],curves=curves,limitations=['Only local existing clip and incident edges; no saved skin, source mutation or new gait.','Original geometric roots/outer support weights fixed; anatomy and proposed region boundary need review.','Parent-following linear skin only, not secondary dynamics or collision approval.','Reporting floor is numerical-noise reporting, not acceptance tolerance.'])
(out/'preflight.json').write_text(json.dumps(report,indent=2));assert sha(cfg['source'])==cfg['input_sha256'][cfg['source']]
print(json.dumps({k:report[k] for k in ['status','feature_vertices','changed_vertices','original_unweighted_feature_vertices','baseline_maximum_edge_change_m','proposed_maximum_edge_change_m','regressed_edges_above_reporting_floor']}),flush=True)
