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
assert type(cfg['frame_start']) is int and 1<=cfg['frame_start']<=1000
assert 1<=len(cfg['sources'])<=16 and 1<=len(cfg['edges'])<=32
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
with np.load(cfg['rest_selection']) as ar:rest,faces=ar['vertices'].astype(float),ar['triangles']
for edge in cfg['edges']:
    i,j=edge['vertices'];assert i!=j and 0<=min(i,j)<=max(i,j)<len(rest)
    assert np.any((faces==i).any(1)&(faces==j).any(1)),'Selected pair is not a source triangle edge'
rows=[]
region_rows=[];region_edges=None;region_maxima=[]
if cfg.get('changed_weight_region'):
    region=cfg['changed_weight_region']
    assert region['selection'] in cfg['input_sha256'] and region['retention_report'] in cfg['input_sha256']
    retention=json.loads(Path(region['retention_report']).read_text())
    assert retention['status']=='passed-indexed-rest-retention-only' and all(retention['checks'].values())
    source_hashes={cfg['input_sha256'][s['source']] for s in cfg['sources']}
    assert {retention['source_sha256'],retention['candidate_sha256']}<=source_hashes
    with np.load(region['selection']) as ar:indices=ar[region['key']]
    assert len(indices)==retention['changed_weight_vertex_count'] and indices.min()>=0 and indices.max()<len(rest)
    selected=np.zeros(len(rest),bool);selected[indices]=True
    edges=np.unique(np.sort(np.concatenate([faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]]]),axis=1),axis=0)
    region_edges=edges[selected[edges].any(1)]
    assert len(region_edges)>0
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
    if region_edges is not None:
        baseline=np.linalg.norm(rest[region_edges[:,0]]-rest[region_edges[:,1]],axis=1)
        assert (baseline>0).all()
        maximum=0.;worst=None;curves=[];edge_maximum=np.zeros(len(region_edges))
        for frame in range(len(points)):
            length=np.linalg.norm(points[frame,region_edges[:,0]].astype(float)-points[frame,region_edges[:,1]].astype(float),axis=1)
            change=abs(length-baseline);j=int(np.argmax(change))
            edge_maximum=np.maximum(edge_maximum,change)
            if change[j]>maximum:maximum=float(change[j]);worst=dict(frame=cfg['frame_start']+frame,vertices=region_edges[j].tolist(),rest_length_m=float(baseline[j]))
            curves.append(dict(frame=cfg['frame_start']+frame,maximum_absolute_change_m=float(change.max()),p99_absolute_change_m=float(np.quantile(change,.99)),p99_relative_change=float(np.quantile(change/baseline,.99))))
        region_rows.append(dict(source=source['id'],incident_edges=len(region_edges),changed_weight_vertices=len(indices),maximum_absolute_change_m=maximum,worst=worst,curves=curves))
        region_maxima.append(edge_maximum)
region_deltas=[]
if region_maxima:
    floor=cfg.get('delta_reporting_floor_m',0.)
    assert 0<=floor<=.001
    for source,values in zip(cfg['sources'][1:],region_maxima[1:]):
        delta=values-region_maxima[0];j=int(np.argmax(delta))
        region_deltas.append(dict(reference=cfg['sources'][0]['id'],source=source['id'],reporting_floor_m=floor,
            edges_increased_above_reporting_floor=int((delta>floor).sum()),edges_decreased_above_reporting_floor=int((delta<-floor).sum()),
            maximum_increase_m=float(delta[j]),maximum_increase_edge=region_edges[j].tolist(),
            reference_error_at_maximum_increase_m=float(region_maxima[0][j]),candidate_error_at_maximum_increase_m=float(values[j])))
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
(out/'edge-comparison.json').write_text(json.dumps(dict(schema_version=1,status='checked-cache-selected-edge-diagnostic-only',cpu_only=True,input_sha256=cfg['input_sha256'],profile_sha256=sha(profile),tool_sha256=sha(__file__),rows=rows,changed_weight_region=region_rows,region_deltas=region_deltas,limitations=['Selected indexed edges and optional changed-weight incident region only, not full-source shape acceptance.','Delta reporting floor suppresses numerical noise only; it is not a quality acceptance tolerance.','Matching rest correspondence and intended comparisons remain separate source/provenance gates.','No weights, geometry, limits or source files changed.']),indent=2))
print(json.dumps({'status':'checked-cache-selected-edge-diagnostic-only','rows':len(rows)}))
