"""Fresh-process dimension, connectivity, UV, and material check for a mesh export."""
import argparse, json, sys
from pathlib import Path
import bpy

argv=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
p=argparse.ArgumentParser(); p.add_argument('--file',required=True); p.add_argument('--kind',choices=['glb','fbx'],required=True); p.add_argument('--dimensions',required=True); a=p.parse_args(argv)
expected=json.loads(a.dimensions)
bpy.ops.wm.read_factory_settings(use_empty=True)
if a.kind=='glb': bpy.ops.import_scene.gltf(filepath=a.file)
else: bpy.ops.import_scene.fbx(filepath=a.file,use_anim=False)
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
points=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
actual=[max(v[i] for v in points)-min(v[i] for v in points) for i in range(3)]
position_ids={}; edges=[]; loose_candidates=set(); offset=0
for o in meshes:
 ids=[]
 for v in o.data.vertices:
  q=tuple(round((o.matrix_world@v.co)[i]*1000000) for i in range(3))
  ids.append(position_ids.setdefault(q,len(position_ids)))
 loose_candidates.update(ids)
 edges.extend((ids[e.vertices[0]],ids[e.vertices[1]]) for e in o.data.edges)
neighbors=[set() for _ in position_ids]
for x,y in edges: neighbors[x].add(y); neighbors[y].add(x)
remaining=set(range(len(neighbors))); comps=0
while remaining:
 comps+=1; todo=[remaining.pop()]
 while todo:
  for n in neighbors[todo.pop()]&remaining: remaining.remove(n); todo.append(n)
loose=sum(not neighbors[i] for i in loose_candidates)
uv=all(len(o.data.uv_layers)>0 for o in meshes)
mats=sum(len(o.data.materials) for o in meshes)
err=max(abs(actual[i]-expected[i]) for i in range(3))
result={'status':'passed' if meshes and err<1e-4 and comps==1 and loose==0 and uv and mats>0 else 'failed','file':Path(a.file).name,'kind':a.kind,'blender':bpy.app.version_string,'mesh_object_count':len(meshes),'dimensions_m':actual,'expected_dimensions_m':expected,'max_dimension_error_m':err,'connected_components':comps,'loose_vertices':loose,'uv0_present':uv,'material_slots':mats,'acceptance_claim':False}
print(json.dumps(result))
if result['status']!='passed': raise RuntimeError(json.dumps(result))
