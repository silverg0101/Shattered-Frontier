"""Fresh-open checks for a construction material study's Blender source."""
import argparse, json, sys
from pathlib import Path
import bpy

argv=sys.argv
argv=argv[argv.index('--')+1:] if '--' in argv else []
p=argparse.ArgumentParser(); p.add_argument('--file',required=True); p.add_argument('--asset-id',required=True); p.add_argument('--dimensions',required=True); a=p.parse_args(argv)
bpy.ops.wm.open_mainfile(filepath=a.file)
expected=json.loads(a.dimensions)
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and o.get('asset_id')==a.asset_id]
points=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
actual=[max(v[i] for v in points)-min(v[i] for v in points) for i in range(3)] if points else []
position_ids={}; edges=[]; loose_candidates=set()
for o in meshes:
 ids=[]
 for v in o.data.vertices:
  q=tuple(round((o.matrix_world@v.co)[i]*1_000_000) for i in range(3)); ids.append(position_ids.setdefault(q,len(position_ids)))
 loose_candidates.update(ids); edges.extend((ids[e.vertices[0]],ids[e.vertices[1]]) for e in o.data.edges)
neighbors=[set() for _ in position_ids]
for x,y in edges: neighbors[x].add(y); neighbors[y].add(x)
remaining=set(range(len(neighbors))); comps=0
while remaining:
 comps+=1; todo=[remaining.pop()]
 while todo:
  for n in neighbors[todo.pop()]&remaining: remaining.remove(n); todo.append(n)
loose=sum(not neighbors[i] for i in loose_candidates)
uv=all(len(o.data.uv_layers)>0 for o in meshes)
uv_range=[]
for o in meshes:
 for layer in o.data.uv_layers:
  vals=[tuple(x.uv) for x in layer.data]
  if vals: uv_range.append([min(x[0] for x in vals),min(x[1] for x in vals),max(x[0] for x in vals),max(x[1] for x in vals)])
images={node.image.name for o in meshes for slot in o.material_slots if slot.material and slot.material.use_nodes for node in slot.material.node_tree.nodes if node.type=='TEX_IMAGE' and node.image}
required={'basecolor.png','roughness.png','normal.png'}
missing=sorted(required-images)
err=max([abs(actual[i]-expected[i]) for i in range(3)],default=1)
result={'status':'passed' if len(meshes)==1 and err<1e-4 and comps==1 and loose==0 and uv and not missing else 'failed','file':Path(a.file).name,'asset_id':a.asset_id,'mesh_object_count':len(meshes),'dimensions_m':actual,'expected_dimensions_m':expected,'max_dimension_error_m':err,'position_welded_components':comps,'loose_vertices':loose,'uv0_present':uv,'uv_ranges':uv_range,'material_images':sorted(images),'missing_required_maps':missing,'acceptance_claim':False}
print(json.dumps(result))
if result['status']!='passed': raise RuntimeError(json.dumps(result))
