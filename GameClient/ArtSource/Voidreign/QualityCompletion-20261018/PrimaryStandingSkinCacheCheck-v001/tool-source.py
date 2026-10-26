"""Read-only all-frame source-cache seams, sole rigidity/contact and ground review."""
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
a=p.parse_args();profile,out=Path(a.profile).resolve(),Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
producer=json.loads(Path(cfg['exchange_report']).read_text());assert producer['status']=='passed-indexed-animated-cache-reimport-only'
assert producer['input_sha256'][cfg['source']]==cfg['input_sha256'][cfg['source']]
points=np.load(cfg['world_points'],mmap_mode='r')
with np.load(cfg['rest_selection']) as a:rest,faces=a['vertices'].astype(float),a['triangles']
with np.load(cfg['semantic_selection']) as a:canonical,labels=a['canonical_node'],a['labels']
patches=np.load(cfg['sole_patches']);first=np.unique(canonical,return_index=True)[1];representative=first[canonical]
feet={};rows=[];max_seam=0.;ground={};limits=cfg['limits']
for name in cfg['feet']:
    ids=patches[name+'.vertices'];f=faces[patches[name+'.faces']];edges=np.unique(np.sort(np.concatenate([f[:,[0,1]],f[:,[1,2]],f[:,[2,0]]]),axis=1),axis=0)
    feet[name]=dict(ids=ids,edges=edges,rest_lengths=np.linalg.norm(rest[edges[:,0]]-rest[edges[:,1]],axis=1),rigidity=0.,hold_slip=0.,min_height=float('inf'),max_height=-float('inf'))
for index,world in enumerate(points):
    frame=index+1;max_seam=max(max_seam,float(np.linalg.norm(world-world[representative],axis=1).max()))
    for name,foot in feet.items():
        e=foot['edges'];error=float(abs(np.linalg.norm(world[e[:,0]]-world[e[:,1]],axis=1)-foot['rest_lengths']).max());foot['rigidity']=max(foot['rigidity'],error)
        if cfg['standing_start_frame']<=frame<=cfg['standing_end_frame']:
            height=float(world[foot['ids'],2].min()-cfg['ground_height_m']);foot['min_height']=min(foot['min_height'],height);foot['max_height']=max(foot['max_height'],height)
            reference=points[cfg['standing_start_frame']-1,foot['ids']];foot['hold_slip']=max(foot['hold_slip'],float(np.linalg.norm(world[foot['ids']]-reference,axis=1).max()))
    minimum=int(world[:,2].argmin())
    if not ground or float(world[minimum,2])<ground['minimum_height_m']:
        ground=dict(frame=frame,vertex=minimum,semantic_label=int(labels[minimum]),minimum_height_m=float(world[minimum,2]-cfg['ground_height_m']))
    if frame==cfg['standing_start_frame']:
        for label in np.unique(labels):
            selected=world[labels==label,2]-cfg['ground_height_m'];rows.append(dict(semantic_label=int(label),minimum_height_m=float(selected.min()),vertices_below_ground_limit=int((selected < -limits['ground_penetration_m']).sum())))
foot_rows=[dict(id=name,minimum_hold_height_m=f['min_height'],maximum_hold_height_m=f['max_height'],maximum_hold_slip_m=f['hold_slip'],maximum_sole_edge_change_m=f['rigidity']) for name,f in feet.items()]
checks=dict(seams=max_seam<=limits['seam_error_m'],sole_rigidity=all(f['rigidity']<=limits['sole_edge_error_m'] for f in feet.values()),
    hold_contacts=all(f['min_height']>=-limits['ground_penetration_m'] and f['max_height']<=limits['sole_contact_height_m'] for f in feet.values()),
    hold_slip=all(f['hold_slip']<=limits['hold_slip_m'] for f in feet.values()))
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
report=dict(schema_version=1,status='needs-review-full-mount-ground' if ground['minimum_height_m'] < -limits['ground_penetration_m'] else 'passed-scoped-standing-cache-checks' if all(checks.values()) else 'needs-review-standing-cache',cpu_only=True,input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),frames=len(points),standing_frames=[cfg['standing_start_frame'],cfg['standing_end_frame']],primary_contact_checks=checks,maximum_seam_error_m=max_seam,feet=foot_rows,full_mount_ground=ground,standing_ground_by_semantic_label=rows,
    limitations=['Planted-hold checks do not establish gait contacts or continuous self/triangle/swept collision.',
      'Whole-mount ground penetration remains failed even when hoof contact checks pass.'])
(out/'standing-cache-report.json').write_text(json.dumps(report,indent=2))
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
print(json.dumps(dict(status=report['status'],checks=checks,ground=ground)))
