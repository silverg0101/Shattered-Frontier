"""CPU unsigned vertex-to-support diagnostics for a frozen disconnected-item inventory.

This proposes nearby support only. It neither classifies intended ornaments nor
proves triangle separation, solid contact, attachment ownership or safe removal.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import numpy as np
from mathutils.bvhtree import BVHTree

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda:stream.read(8*1024*1024),b''):h.update(chunk)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and cfg['units']=='metres' and not out.exists()
assert 1<=cfg['maximum_items']<=1000 and .0001<=cfg['review_distance_m']<=.1
for key in ['rest_selection','domains','semantic_selection','inventory','item_selections']:assert cfg[key] in cfg['input_sha256']
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
with np.load(cfg['rest_selection']) as ar:points,faces=ar['vertices'],ar['triangles']
with np.load(cfg['domains']) as ar:owners=ar['owner_per_vertex']
with np.load(cfg['semantic_selection']) as ar:labels=ar['labels']
inventory=json.loads(Path(cfg['inventory']).read_text());assert len(inventory['items'])<=cfg['maximum_items']
assert inventory['scope_sha256']==cfg['input_sha256'][cfg['rest_selection']]
valid=np.isin(owners,cfg['support_owner_ids'])&np.isin(labels,cfg['support_semantic_labels'])
face_ids=np.flatnonzero(valid[faces].all(1));assert len(face_ids)>0
tree=BVHTree.FromPolygons(points.tolist(),faces[face_ids].tolist(),all_triangles=True)
rows=[]
with np.load(cfg['item_selections']) as selections:
    for item in inventory['items']:
        indices=selections[item['selection_key']];assert len(indices)==item['selected_vertices']
        assert not valid[indices].any(),'Item must be excluded from its proposed support surface'
        nearest=[]
        for index in indices:
            location,normal,face,distance=tree.find_nearest(points[index]);assert location is not None
            nearest.append((float(distance),int(index),int(face_ids[face]),list(location)))
        nearest.sort(key=lambda row:row[0]);distances=np.array([r[0] for r in nearest]);closest=nearest[0]
        rows.append(dict(id=item['id'],selection_key=item['selection_key'],sampled_vertices=len(indices),
            minimum_sampled_vertex_distance_m=float(distances.min()),median_sampled_vertex_distance_m=float(np.median(distances)),maximum_sampled_vertex_distance_m=float(distances.max()),
            closest_source_vertex=closest[1],closest_support_source_face=closest[2],closest_support_point_m=closest[3],
            support_face_owner_ids=owners[faces[closest[2]]].tolist(),
            review_flag='sampled-gap-review' if closest[0]>cfg['review_distance_m'] else 'near-surface-still-unattached',
            disposition=None,semantic_owner=None))
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
report=dict(schema_version=1,status='support-distance-diagnostic-no-semantic-acceptance',cpu_only=True,input_sha256=cfg['input_sha256'],tool_sha256=sha(__file__),profile_sha256=sha(profile),support_faces=len(face_ids),review_distance_m=cfg['review_distance_m'],items=rows,
    limitations=['Unsigned nearest distances sample item vertices only. Triangle interiors may approach or intersect support more closely.','Owner labels select a proposed support set and may themselves include tack or anatomical errors.','Disconnected topology does not imply unwanted debris. No binding, deletion or pass/fail asset acceptance.'])
(out/'support-report.json').write_text(json.dumps(report,indent=2))
print(json.dumps({'status':report['status'],'items':len(rows),'sampled_gap_review':sum(r['review_flag']=='sampled-gap-review' for r in rows)}),flush=True)
