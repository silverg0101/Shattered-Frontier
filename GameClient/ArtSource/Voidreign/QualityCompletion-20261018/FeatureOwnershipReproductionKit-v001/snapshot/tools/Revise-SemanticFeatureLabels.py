"""Apply a source-bound reviewed label correction; invalidate derived rig data.

Writes labels/canonical indices only. This is intentionally not a drop-in copy
of parent guide/root/weight data and never modifies a Blender source.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(8*1024*1024),b''):h.update(block)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args();profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and not out.exists() and 1<=len(cfg['changes'])<=32
assert cfg['review'] in cfg['input_sha256']
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
review=json.loads(Path(cfg['review']).read_text());assert review['status']=='author-reviewed-motion-owner-correction-only'
with np.load(cfg['parent_selection']) as ar:original,canonical,protected=ar['labels'],ar['canonical_node'],ar['explicit_protected']
labels=original.copy();changed=np.zeros(len(labels),bool);first=np.unique(canonical,return_index=True)[1]
selection=np.load(cfg['change_selection']);rows=[]
for change in cfg['changes']:
    assert change['id'] in review['reviewed_component_ids']
    vertices=selection[change['vertex_key']];assert len(vertices)==change['expected_vertices']
    assert type(change['new_label']) is int and change['new_label'] in cfg['declared_labels']
    assert (original[vertices]==change['original_label']).all() and not changed[vertices].any() and not protected[vertices].any()
    nodes=np.unique(canonical[vertices]);complete=np.flatnonzero(np.isin(canonical,nodes));assert np.array_equal(np.sort(vertices),complete),'Partial coincident seam correction'
    labels[vertices]=change['new_label'];changed[vertices]=True
    rows.append(dict(id=change['id'],vertices=len(vertices),old_label=change['original_label'],new_label=change['new_label']))
assert np.array_equal(labels[~changed],original[~changed]) and np.array_equal(labels,labels[first][canonical])
assert set(np.unique(labels).tolist())<=set(cfg['declared_labels'])
out.mkdir(parents=True);np.savez_compressed(out/'label-only-selections.npz',labels=labels,canonical_node=canonical,changed_vertices=np.flatnonzero(changed))
(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
report=dict(schema_version=1,status='reviewed-label-correction-derived-motion-invalid',cpu_only=True,input_sha256=cfg['input_sha256'],profile_sha256=sha(profile),tool_sha256=sha(__file__),changes=rows,changed_vertices=int(changed.sum()),label_definitions=cfg['label_definitions'],invalidation=['Parent root distances, root nodes, guides, attachments, skin weights and simulations do not describe this new label contract.','No derived arrays copied except canonical index correspondence. Rebuild/review support roots and per-feature motion adapters before using corrected labels.','Existing material prototype uses its frozen old selection; material appearance class is independent of this motion-owner correction.'],limitations=['Correction covers the reviewed misowned vertices, not the full physical fringe or all unknown feature coverage.','No Blender mesh/rig/source/animation changed; no primary/gait/material attempt consumed.','Consumer must explicitly support every nonzero label; unknown labels cannot silently become rigid body or be discarded.'])
(out/'revision.json').write_text(json.dumps(report,indent=2));print(json.dumps(dict(status=report['status'],changed_vertices=int(changed.sum()))),flush=True)
