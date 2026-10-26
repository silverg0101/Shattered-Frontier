"""Inspect exact reported strain edges against the hash-bound saved Blender skin."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np

def sha(p):
    h=hashlib.sha256()
    with Path(p).open('rb') as stream:
        for block in iter(lambda:stream.read(8*1024*1024),b''):h.update(block)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
profile=Path(a.profile).resolve();cfg=json.loads(profile.read_text());out=Path(a.output).resolve()
assert cfg['schema_version']==1 and not out.exists()
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
strain=json.loads(Path(cfg['strain_report']).read_text())
assert strain['input_sha256'][cfg['source']]==cfg['input_sha256'][cfg['source']]
with np.load(cfg['domains']) as ar:owners=ar['owner_per_vertex']
with np.load(cfg['semantic_selection']) as ar:labels=ar['labels']
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
obj=bpy.data.objects[cfg['object']];scene=bpy.context.scene;rows=[]
for group,metrics in strain['groups'].items():
    for kind in ['absolute','relative']:
        frame=metrics['maximum_'+kind+'_change_frame'];ids=metrics['maximum_'+kind+'_change_edge']
        scene.frame_set(frame);ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());vertices=[]
        for i in ids:
            v=obj.data.vertices[i]
            weights={obj.vertex_groups[g.group].name:g.weight for g in v.groups if g.weight>1e-8 and obj.vertex_groups[g.group].name.startswith(cfg['bone_prefix'])}
            vertices.append(dict(index=i,rest_local_m=list(v.co),world_deformed_m=list(obj.matrix_world@ev.data.vertices[i].co),owner=int(owners[i]),semantic_label=int(labels[i]),primary_weights=weights))
        names=set().union(*(v['primary_weights'] for v in vertices))
        delta=sum(abs(vertices[0]['primary_weights'].get(n,0)-vertices[1]['primary_weights'].get(n,0)) for n in names)
        rows.append(dict(group=group,metric=kind,frame=frame,vertices=vertices,weight_l1_delta=delta))
out.mkdir(parents=True)
(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out/'edge-review.json').write_text(json.dumps(dict(schema_version=1,status='diagnostic-unclassified-no-repair',input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),rows=rows,limitations=['Weight differences do not alone identify a defect or approve a new skin.','Exact edge probes require local surface/joint visual review; no source changes.']),indent=2))
print(json.dumps({'status':'diagnostic-unclassified-no-repair','rows':len(rows)}))
