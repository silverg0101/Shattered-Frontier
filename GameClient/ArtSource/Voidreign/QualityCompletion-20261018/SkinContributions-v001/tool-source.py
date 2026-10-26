"""Read-only exact linear-skin edge analysis and declared weight-transfer counterfactual.

No source weights, geometry or animation are changed. Reject unsupported deformers
or a baseline that does not reproduce Blender's evaluated source positions.
"""
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

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
assert 1<=cfg['frame_start']<=cfg['frame_end']<=1000 and 0<cfg['baseline_tolerance_m']<=1e-5
assert 1<=len(cfg['edges'])<=20
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()));scene=bpy.context.scene;obj=bpy.data.objects[cfg['object']];rig=bpy.data.objects[cfg['rig_object']]
active=[m for m in obj.modifiers if m.show_viewport]
assert len(active)==1 and active[0].type=='ARMATURE' and active[0].object==rig
assert not active[0].use_deform_preserve_volume and not active[0].use_bone_envelopes
assert all(b.bbone_segments==1 for b in rig.data.bones)
rows=[]
for edge in cfg['edges']:
    data=[]
    for index in edge['vertices']:
        v=obj.data.vertices[index];weights={obj.vertex_groups[g.group].name:g.weight for g in v.groups if obj.vertex_groups[g.group].name in rig.data.bones and g.weight>1e-10}
        assert abs(sum(weights.values())-1)<1e-5
        alternative={}
        for name,value in weights.items():
            target=edge['transfer'].get(name,name);assert target in rig.data.bones
            alternative[target]=alternative.get(target,0)+value
        assert len(alternative)<=cfg['maximum_influences']
        data.append((index,v.co.copy(),weights,alternative))
    scene.frame_set(cfg['frame_start']);rest_length=(obj.matrix_world@data[0][1]-obj.matrix_world@data[1][1]).length
    curves=[];maximum_baseline=0.
    for frame in range(cfg['frame_start'],cfg['frame_end']+1):
        scene.frame_set(frame);ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());baseline=[];counter=[]
        for index,co,weights,alternative in data:
            bone_points={name:rig.matrix_world@rig.pose.bones[name].matrix@rig.data.bones[name].matrix_local.inverted()@rig.matrix_world.inverted()@obj.matrix_world@co for name in set(weights)|set(alternative)}
            normal=sum((bone_points[n]*w for n,w in weights.items()),Vector((0,0,0)))
            proposed=sum((bone_points[n]*w for n,w in alternative.items()),Vector((0,0,0)))
            actual=obj.matrix_world@ev.data.vertices[index].co
            maximum_baseline=max(maximum_baseline,(normal-actual).length);baseline.append(normal);counter.append(proposed)
        curves.append(dict(frame=frame,baseline_absolute_edge_error_m=abs((baseline[0]-baseline[1]).length-rest_length),counterfactual_absolute_edge_error_m=abs((counter[0]-counter[1]).length-rest_length)))
    assert maximum_baseline<=cfg['baseline_tolerance_m'],maximum_baseline
    rows.append(dict(id=edge['id'],vertices=edge['vertices'],rest_length_m=rest_length,transfer=edge['transfer'],weights=[dict(vertex=i,baseline=w,counterfactual=c) for i,co,w,c in data],maximum_baseline_position_error_m=maximum_baseline,maximum_baseline_edge_error_m=max(r['baseline_absolute_edge_error_m'] for r in curves),maximum_counterfactual_edge_error_m=max(r['counterfactual_absolute_edge_error_m'] for r in curves),curves=curves))
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
report=dict(schema_version=1,status='read-only-skin-counterfactual-not-a-candidate',cpu_only=True,input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),edges=rows,limitations=['Only exact selected edges measured; changes elsewhere, joint volume and style are unknown.','A favorable local counterfactual does not authorize or validate a global remap. No weights or source file changed.'])
(out/'contribution-report.json').write_text(json.dumps(report,indent=2))
print(json.dumps({'status':report['status'],'edges':len(rows)}),flush=True)
