"""Exact topology support proposals for semantic features; never auto-bind."""
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
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
with np.load(cfg['rest_selection']) as ar:points,faces=ar['vertices'],ar['triangles']
with np.load(cfg['semantic_selection']) as ar:canonical,labels=ar['canonical_node'],ar['labels']
with np.load(cfg['domains']) as ar:owners=ar['owner_per_vertex']
first=np.unique(canonical,return_index=True)[1];node_labels=labels[first];node_owners=owners[first]
assert np.array_equal(labels,node_labels[canonical]) and np.array_equal(owners,node_owners[canonical])
cf=canonical[faces];edges=np.unique(np.sort(np.concatenate([cf[:,[0,1]],cf[:,[1,2]],cf[:,[2,0]]]),axis=1),axis=0);edges=edges[edges[:,0]!=edges[:,1]]
arrays={};rows=[]
for feature in cfg['features']:
    label=feature['label'];assert type(label) is int and label>0
    parent=np.arange(len(first))
    def root(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    internal=edges[(node_labels[edges]==label).all(1)]
    for i,j in internal:parent[root(i)]=root(j)
    groups={}
    for i in np.flatnonzero(node_labels==label):groups.setdefault(root(i),[]).append(int(i))
    component=np.full(len(first),-1,int)
    for index,nodes in enumerate(groups.values()):component[nodes]=index
    support_edges=edges[((node_labels[edges[:,0]]==label)&(node_labels[edges[:,1]]==0))|((node_labels[edges[:,1]]==label)&(node_labels[edges[:,0]]==0))]
    by_group={}
    for i,j in support_edges:
        own,other=(i,j) if node_labels[i]==label else (j,i)
        by_group.setdefault(int(component[own]),set()).add(int(other))
    for index,nodes in enumerate(groups.values()):
        node_ids=np.array(sorted(nodes),np.int32);vertices=np.flatnonzero(np.isin(canonical,node_ids));face_ids=np.flatnonzero(np.isin(cf,node_ids).all(1));support=np.array(sorted(by_group.get(index,set())),int)
        values,counts=np.unique(node_owners[support],return_counts=True)
        support_counts={str(int(k)):int(v) for k,v in zip(values,counts)}
        conflicts=[int(v) for v in values if int(v) not in feature['expected_support_owners'] and int(v)!=0]
        digest=hashlib.sha256(node_ids.astype('<i4').tobytes()).hexdigest()[:12];stable='FSA-'+cfg['input_sha256'][cfg['rest_selection']][:10]+'-'+digest
        arrays[stable+'.vertices']=vertices;arrays[stable+'.faces']=face_ids;arrays[stable+'.support_vertices']=first[support]
        flag='support-owner-conflict-proposal' if conflicts else 'no-direct-support-proposal' if not len(support) else 'unresolved-support-proposal' if 0 in values else 'expected-owner-adjacency-only'
        rows.append(dict(id=stable,feature=feature['name'],label=label,status=flag,vertices=len(vertices),faces=len(face_ids),canonical_nodes=len(nodes),support_owner_node_counts=support_counts,conflicting_owner_ids=conflicts,bounds_m=[points[vertices].min(0).tolist(),points[vertices].max(0).tolist()],vertex_key=stable+'.vertices',face_key=stable+'.faces',support_key=stable+'.support_vertices'))
rows.sort(key=lambda r:(r['status']!='support-owner-conflict-proposal',r['feature'],-r['vertices'],r['id']))
out.mkdir(parents=True);np.savez_compressed(out/'support-selections.npz',**arrays)
(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
report=dict(schema_version=1,status='feature-support-adjacency-proposals-needs-review',cpu_only=True,input_sha256=cfg['input_sha256'],profile_sha256=sha(profile),tool_sha256=sha(__file__),components=len(rows),conflicts=sum(r['status']=='support-owner-conflict-proposal' for r in rows),items=rows,limitations=['Exact canonical edge adjacency only; no nearest-owner binding or deletion.','Primary domains and expected support owners are authored priors, not final bones or semantic proof.','Fused false bridges and disconnected intended attachments require context/reference review.','Material class and motion feature ownership are separate contracts; appearance labels do not authorize animation bindings.'])
(out/'inventory.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:report[k] for k in ['status','components','conflicts']}),flush=True)
