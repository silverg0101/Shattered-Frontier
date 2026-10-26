"""Rebuild this source-scoped disconnected nonhair inventory into a NEW folder."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args();cfg=json.loads(Path(a.profile).read_text());out=Path(a.output);assert not out.exists()
for f,h in cfg['input_sha256'].items():assert hashlib.sha256(Path(f).read_bytes()).hexdigest()==h,f
r=np.load(cfg['rest_selection']);d=np.load(cfg['domains']);v=r['vertices'];comp=r['component_per_vertex'];ids=d['unresolved-nonhair']
scope=cfg['input_sha256'][cfg['rest_selection']];rows=[];arrays={}
for c in sorted(np.unique(comp[ids]),key=lambda c:-int((comp[ids]==c).sum())):
    selected=ids[comp[ids]==c];key='C'+str(int(c)).zfill(4);arrays[key]=selected;points=v[selected]
    rows.append(dict(id=cfg['asset_prefix']+'-LOOSE-'+scope[:10]+'-'+key,component_id=int(c),selection_key=key,
        selected_vertices=len(selected),whole_component_vertices=int((comp==c).sum()),bounds_m=[points.min(0).tolist(),points.max(0).tolist()],
        center_m=points.mean(0).tolist(),status='unresolved-no-binding-or-deletion',disposition=None,owning_body_part=None,motion_representation=None,
        review_required=['semantic identity','actual attachment/support','whole component versus subset','intended primary/secondary ownership']))
out.mkdir(parents=True);np.savez_compressed(out/'item-selections.npz',**arrays)
(out/'inventory.json').write_text(json.dumps(dict(schema_version=1,asset_id=cfg['asset_id'],scope_sha256=scope,input_sha256=cfg['input_sha256'],
    selected_unresolved_nonhair_vertices=len(ids),items=rows,limitations=['Unresolved components/subsets are not deletion candidates.','Fused hanging tack needs a separate inventory.']),indent=2))
