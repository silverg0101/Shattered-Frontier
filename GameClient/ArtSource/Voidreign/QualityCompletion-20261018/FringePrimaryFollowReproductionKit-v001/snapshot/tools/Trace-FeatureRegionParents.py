"""Read-only primary support weights and per-root world trajectories."""
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
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists() and 1<=cfg['frame_start']<=cfg['frame_end']<=1000
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
with np.load(cfg['region_roots']) as ar:roots,supports=ar['root_vertices'],ar['support_vertices']
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()));scene=bpy.context.scene;obj=bpy.data.objects[cfg['object']];rig=bpy.data.objects[cfg['primary_rig']]
records=[]
for role,indices in [('roots',roots),('supports',supports)]:
    rows=[]
    for i in indices:
        groups={obj.vertex_groups[g.group].name:g.weight for g in obj.data.vertices[int(i)].groups if g.weight>1e-10}
        primary={n:w for n,w in groups.items() if n in rig.data.bones and rig.data.bones[n].use_deform}
        foreign={n:w for n,w in primary.items() if n not in cfg['allowed_support_bones']}
        rows.append(dict(vertex=int(i),primary_weights=primary,primary_weight_sum=sum(primary.values()),foreign_primary_weights=foreign))
    records.append(dict(role=role,vertices=len(rows),unweighted_primary_vertices=sum(r['primary_weight_sum']<=1e-10 for r in rows),foreign_primary_vertices=sum(bool(r['foreign_primary_weights']) for r in rows),weights=rows))
paths=[]
for frame in range(cfg['frame_start'],cfg['frame_end']+1):
    scene.frame_set(frame);ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    paths.append(np.array([(obj.matrix_world@ev.data.vertices[int(i)].co)[:] for i in np.concatenate([roots,supports])],np.float64))
paths=np.array(paths);assert np.isfinite(paths).all()
step=float(np.linalg.norm(np.diff(paths,axis=0),axis=2).max())
out.mkdir(parents=True);np.savez_compressed(out/'parent-trajectories.npz',frames=np.arange(cfg['frame_start'],cfg['frame_end']+1),root_vertices=roots,support_vertices=supports,world_points=paths)
(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
report=dict(schema_version=1,status='parent-support-trace-needs-review',cpu_only=True,input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),frames=len(paths),maximum_step_m=step,roles=records,limitations=['Source evaluated trajectories, not corrected bindings or simulation acceptance.','Candidate root region/cut is still authored anatomy; exact topology alone is insufficient.','Unknown/unweighted/foreign support needs review before constructing a driver; no nearest-bone assignment.'])
(out/'parent-report.json').write_text(json.dumps(report,indent=2));assert sha(cfg['source'])==cfg['input_sha256'][cfg['source']]
print(json.dumps(dict(status=report['status'],frames=len(paths),roles=[{k:v for k,v in row.items() if k!='weights'} for row in records],maximum_step_m=step)),flush=True)
