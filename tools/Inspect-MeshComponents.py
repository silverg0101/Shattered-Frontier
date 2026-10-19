"""Read-only CPU inventory of disconnected objects/islands and sampled main-surface gaps.

Run in background Blender. Reports candidates; never deletes or welds geometry.
Component identity is scoped to mesh hash, not stable across topology changes.
"""
import argparse,json,hashlib,sys
from pathlib import Path
import bpy,numpy as np
from mathutils import Vector,Matrix
from mathutils.bvhtree import BVHTree
p=argparse.ArgumentParser();p.add_argument('--source',required=True);p.add_argument('--collection',required=True)
p.add_argument('--assembly-root');p.add_argument('--output',required=True)
p.add_argument('--connectivity',choices=['index','coincident'],default='coincident',help='Coincident links exactly equal local-space positions for analysis only, preserving UV seams on the asset')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);source=Path(a.source).resolve();out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=False)
bpy.ops.wm.open_mainfile(filepath=str(source));scene=bpy.context.scene;scene.frame_set(1)
collection=bpy.data.collections.get(a.collection)
if not collection:raise RuntimeError('Explicit asset collection not found')
inverse=bpy.data.objects[a.assembly_root].matrix_world.inverted() if a.assembly_root else Matrix.Identity(4)
if a.assembly_root and not np.allclose(bpy.data.objects[a.assembly_root].matrix_world.to_scale(),(1,1,1),atol=1e-6):
    raise RuntimeError('Assembly root must be rigid with unit scale for metric distances')
unit_scale=scene.unit_settings.scale_length
objects=sorted([o for o in collection.all_objects if o.type=='MESH'],key=lambda o:o.name)
entries=[];buffers=[]
for obj in objects:
    data=obj.data;n=len(data.vertices);parent=np.arange(n,dtype=np.int32);sizes=np.ones(n,dtype=np.int32)
    if not n:
        selection_file=f'{len(buffers):02}-component-selections.npz';empty=np.empty((0,3),dtype=np.float32)
        np.savez_compressed(out/selection_file,vertices=empty,local_vertices=empty,triangles=np.empty((0,3),dtype=np.int32),component_per_vertex=np.empty(0,dtype=np.int32))
        entries.append(dict(object=obj.name,mesh_hash=hashlib.sha256(b'').hexdigest(),vertex_count=0,triangle_count=0,component_count=0,raw_index_component_count=0,connectivity=a.connectivity,selection_file=selection_file,empty_mesh=True,components=[]));buffers.append(None);continue
    def find(index):
        while parent[index]!=index:parent[index]=parent[parent[index]];index=int(parent[index])
        return index
    for edge in data.edges:
        left,right=map(find,edge.vertices)
        if left!=right:
            if sizes[left]<sizes[right]:left,right=right,left
            parent[right]=left;sizes[left]+=sizes[right]
    local=np.array([v.co[:] for v in data.vertices],dtype=np.float32)
    index_roots=np.array([find(i) for i in range(n)])
    if a.connectivity=='coincident':
        _,representatives,position_groups=np.unique(local,axis=0,return_index=True,return_inverse=True)
        for vertex,group in enumerate(position_groups):
            left,right=find(vertex),find(int(representatives[group]))
            if left!=right:
                if sizes[left]<sizes[right]:left,right=right,left
                parent[right]=left;sizes[left]+=sizes[right]
    roots=np.array([find(i) for i in range(n)])
    matrix=inverse@obj.matrix_world
    world=np.array([matrix@v.co for v in data.vertices],dtype=np.float32)*unit_scale
    data.calc_loop_triangles();triangles=np.array([tuple(t.vertices) for t in data.loop_triangles],dtype=np.int32).reshape(-1,3)
    edges=np.array([tuple(e.vertices) for e in data.edges],dtype=np.int32).reshape(-1,2)
    face_components=roots[triangles[:,0]];edge_components=roots[edges[:,0]]
    unique,face_counts=np.unique(face_components,return_counts=True);count_map=dict(zip(unique.tolist(),face_counts.tolist()))
    unique_roots=np.unique(roots).tolist();unique_roots.sort(key=lambda r:(-count_map.get(r,0),-int((roots==r).sum()),r))
    digest=hashlib.sha256(local.tobytes()+triangles.tobytes()).hexdigest()
    groups=[];labels=np.empty(n,dtype=np.int32)
    for index,root in enumerate(unique_roots):
        selected=np.flatnonzero(roots==root);labels[selected]=index
        groups.append(dict(id=f'{digest[:12]}:C{index:04}',component_index=index,vertices=int(len(selected)),triangles=count_map.get(root,0),edges=int((edge_components==root).sum()),
            bounds_m=[world[selected].min(0).tolist(),world[selected].max(0).tolist()],centroid_m=world[selected].mean(0).tolist(),
            dimensions_m=np.ptp(world[selected],axis=0).tolist(),classification='unresolved',disposition='needs-review'))
    primary=unique_roots[0];main_triangles=triangles[face_components==primary]
    bvh=BVHTree.FromPolygons([Vector(v) for v in world],main_triangles.tolist(),all_triangles=True) if len(main_triangles) else None
    for group,root in zip(groups,unique_roots):
        selected=np.flatnonzero(roots==root)
        if root==primary:group['role']='largest-component reference heuristic';continue
        if bvh is None:
            group.update(sampled_main_surface_gap_m=None,distance_sample_count=0,distance_note='No triangle surface exists in the reference component');continue
        sample=selected if len(selected)<=256 else selected[np.linspace(0,len(selected)-1,256,dtype=int)]
        distances=[float(bvh.find_nearest(Vector(world[i]))[3]) for i in sample]
        group.update(sampled_main_surface_gap_m=min(distances),distance_sample_count=len(sample),distance_note='Minimum over sampled vertices to largest-component triangles; not exact component-to-component minimum or proof of intended attachment.')
    selection_file=f'{len(buffers):02}-component-selections.npz'
    np.savez_compressed(out/selection_file,vertices=world,local_vertices=local,triangles=triangles,component_per_vertex=labels,index_component_roots=index_roots)
    entries.append(dict(object=obj.name,mesh_hash=digest,vertex_count=n,triangle_count=len(triangles),component_count=len(groups),
        raw_index_component_count=len(np.unique(index_roots)),connectivity=a.connectivity,selection_file=selection_file,components=groups))
    buffers.append((world,triangles,labels))
report=dict(schema_version=1,status='needs-review',source=str(source),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    tool='Inspect-MeshComponents.py',tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),blender=bpy.app.version_string,method='Edge connectivity plus optional exact-coincident seam links for analysis; CPU BVH sampled gaps in assembly coordinates; no geometry edits',
    capabilities=dict(disconnected_objects=True,within_mesh_islands=True,loose_vertices_edges=True,semantic_classification=False,thin_bridge_detection=False,deformed_attachment_validation=False),
    scene_unit_scale_length=unit_scale,objects=entries,limitations=['Largest component is a proximity reference, not an approved semantic body.','Exact coincident positions can bridge UV seams or intentional touching parts; this does not prove intended attachment.','All unclassified components remain unresolved, including intentional ornaments.','Cross-object nearest-distance and motion/export attachment correspondence need additional checks.'])
(out/'component-report.json').write_text(json.dumps(report,indent=2))
print(json.dumps(dict(objects=len(entries),components=sum(e['component_count'] for e in entries),output=str(out))),flush=True)
