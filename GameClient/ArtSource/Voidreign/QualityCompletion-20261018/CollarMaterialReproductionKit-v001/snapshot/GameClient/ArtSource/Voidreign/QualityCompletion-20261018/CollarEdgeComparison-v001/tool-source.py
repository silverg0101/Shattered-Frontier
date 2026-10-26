"""Read-only selected-edge curves across hash-bound checked indexed motion caches."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda:stream.read(8*1024*1024),b''):h.update(chunk)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args();profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
assert 1<=len(cfg['sources'])<=16 and 1<=len(cfg['edges'])<=32
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
with np.load(cfg['rest_selection']) as ar:rest,faces=ar['vertices'].astype(float),ar['triangles']
for edge in cfg['edges']:
    i,j=edge['vertices'];assert i!=j and 0<=min(i,j)<=max(i,j)<len(rest)
    assert np.any((faces==i).any(1)&(faces==j).any(1)),'Selected pair is not a source triangle edge'
rows=[]
for source in cfg['sources']:
    for key in ['source','producer_report','world_points']:assert source[key] in cfg['input_sha256']
    report=json.loads(Path(source['producer_report']).read_text())
    assert report['status']=='passed-indexed-animated-cache-reimport-only'
    assert report['input_sha256'][source['source']]==cfg['input_sha256'][source['source']]
    assert report['vertices']==len(rest)
    points=np.load(source['world_points'],mmap_mode='r');assert points.shape==(report['frames'],len(rest),3)
    for edge in cfg['edges']:
        i,j=edge['vertices'];baseline=float(np.linalg.norm(rest[i]-rest[j]))
        lengths=np.linalg.norm(points[:,i].astype(float)-points[:,j].astype(float),axis=1)
        change=abs(lengths-baseline);worst=int(np.argmax(change))
        rows.append(dict(source=source['id'],edge=edge['id'],vertices=[i,j],frames=len(lengths),rest_length_m=baseline,
            maximum_absolute_change_m=float(change[worst]),worst_frame=cfg['frame_start']+worst,
            curves=[dict(frame=cfg['frame_start']+f,length_m=float(lengths[f]),absolute_change_m=float(change[f])) for f in range(len(lengths))]))
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out/'edge-comparison.json').write_text(json.dumps(dict(schema_version=1,status='checked-cache-selected-edge-diagnostic-only',cpu_only=True,input_sha256=cfg['input_sha256'],profile_sha256=sha(profile),tool_sha256=sha(__file__),rows=rows,limitations=['Selected indexed edges only, not full-source shape acceptance.','Matching rest correspondence and intended comparisons remain separate source/provenance gates.','No weights, geometry, limits or source files changed.']),indent=2))
print(json.dumps({'status':'checked-cache-selected-edge-diagnostic-only','rows':len(rows)}))
