"""Export exact retained triangle/loop UVs for offline material-mask authoring."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
profile,out=Path(a.profile).resolve(),Path(a.output).resolve()
cfg=json.loads(profile.read_text(encoding='utf-8'))
assert cfg['schema_version']==1 and not out.exists()
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
r=json.loads(Path(cfg['retention']).read_text(encoding='utf-8'))
assert r['status']=='passed-indexed-rest-retention-only'
assert r['candidate_sha256']==cfg['input_sha256'][cfg['source']]
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()))
mesh=bpy.data.objects[cfg['object']].data
faces=np.array([poly.vertices[:] for poly in mesh.polygons],np.int32)
assert faces.ndim==2 and faces.shape[1]==3
assert hashlib.sha256(faces.tobytes()).hexdigest()==r['after']['faces']['sha256']
assert cfg['uv_layer'] in r['after']['uv']
uv=np.empty((len(mesh.loops),2),np.float32)
mesh.uv_layers[cfg['uv_layer']].data.foreach_get('uv',uv.ravel())
assert hashlib.sha256(uv.tobytes()).hexdigest()==r['after']['uv'][cfg['uv_layer']]['sha256']
loop_vertices=np.array([loop.vertex_index for loop in mesh.loops],np.int32)
assert np.array_equal(loop_vertices,faces.ravel()) and np.isfinite(uv).all()
out.mkdir(parents=True)
(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
np.savez_compressed(out/'uv-topology.npz',triangles=faces,loop_uv=uv.reshape(-1,3,2))
report=dict(schema_version=1,status='passed-source-bound-UV-export-only',source=cfg['source'],source_sha256=cfg['input_sha256'][cfg['source']],input_sha256=cfg['input_sha256'],tool_sha256=sha(out/'tool-source.py'),profile_sha256=sha(profile),triangles=len(faces),uv_layer=cfg['uv_layer'],uv_sha256=r['after']['uv'][cfg['uv_layer']]['sha256'],uv_min=uv.min(0).tolist(),uv_max=uv.max(0).tolist(),topology_sha256=sha(out/'uv-topology.npz'),limitations=['Exact UV extraction only; no overlap/density/material/geometry/engine acceptance.','UV origin bottom-left; raster image rows require explicit vertical conversion.'])
(out/'uv-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({k:report[k] for k in ['status','triangles','uv_min','uv_max']}),flush=True)
