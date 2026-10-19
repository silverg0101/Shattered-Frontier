"""Focused saved-source render and empty-scene exchange reimport checks."""
import argparse,hashlib,json,sys
from pathlib import Path
import bpy
import bmesh
from mathutils import Vector
p=argparse.ArgumentParser();p.add_argument('--folder',required=True);p.add_argument('--asset-name',default='Voidreign')
p.add_argument('--dimension-unit',choices=['m','native-normalized'],help='Unit basis for reported bounds')
p.add_argument('--geometry-only',action='store_true')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);folder=Path(a.folder).resolve()
if not a.asset_name or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-' for c in a.asset_name):
    raise ValueError('--asset-name must contain only letters, numbers, underscore or hyphen')
asset_collection=f'{a.asset_name} | Generated asset'
dimension_unit=a.dimension_unit or ('m' if a.asset_name=='Voidreign' else 'native-normalized')
def inspect(meshes):
    assert meshes,'Missing mesh'
    corners=[o.matrix_world@Vector(v) for o in meshes for v in o.bound_box]
    dims=[max(v[i] for v in corners)-min(v[i] for v in corners) for i in range(3)]
    images=[n.image for o in meshes for m in o.data.materials if m and m.use_nodes for n in m.node_tree.nodes if n.type=='TEX_IMAGE' and n.image]
    if not a.geometry_only:
        assert images and all(len(im.pixels)>0 for im in images),'Missing image dependencies'
        assert all(len(o.data.uv_layers)>0 for o in meshes),'Missing UVs'
    records=[]
    for obj in meshes:
        bm=bmesh.new();bm.from_mesh(obj.data);bm.verts.ensure_lookup_table();bm.edges.ensure_lookup_table();bm.faces.ensure_lookup_table()
        vertex_hash=hashlib.sha256()
        for v in bm.verts:vertex_hash.update(bytes(str(tuple(round(float(c),7) for c in v.co)),encoding='utf-8'))
        for e in bm.edges:vertex_hash.update(bytes(str(tuple(sorted(v.index for v in e.verts))),encoding='utf-8'))
        mesh_hash=vertex_hash.hexdigest();seen=set();islands=[];component_by_vertex={}
        for seed in bm.verts:
            if seed.index in seen:continue
            stack=[seed];seen.add(seed.index);verts=[]
            while stack:
                v=stack.pop();verts.append(v)
                for edge in v.link_edges:
                    other=edge.other_vert(v)
                    if other.index not in seen:seen.add(other.index);stack.append(other)
            indices={v.index for v in verts};component_slot=len(islands)
            for index in indices:component_by_vertex[index]=component_slot
            world=[obj.matrix_world@v.co for v in verts]
            lo=[min(v[i] for v in world) for i in range(3)];hi=[max(v[i] for v in world) for i in range(3)]
            packed_indices=','.join(str(i) for i in sorted(indices))
            selection_hash=hashlib.sha256(packed_indices.encode()).hexdigest()[:12]
            islands.append(dict(component_id=f'{obj.name}:{mesh_hash[:12]}:{selection_hash}',vertex_index_count=len(indices),vertex_index_sample=sorted(indices)[:16],
                vertex_count=len(verts),edge_count=0,face_count=0,bounds_world_units=[[round(x,7) for x in lo],[round(x,7) for x in hi]],
                centroid_world_units=[round(sum(v[i] for v in world)/len(world),7) for i in range(3)],
                classification='unresolved',expected_attachment='not yet assigned',nearest_surface_distance='not computed'))
        for edge in bm.edges:islands[component_by_vertex[edge.verts[0].index]]['edge_count']+=1
        for face in bm.faces:islands[component_by_vertex[face.verts[0].index]]['face_count']+=1
        island_count=len(islands)
        face_bins={}
        for island in islands:
            bucket=str(island['face_count']) if island['face_count']<20 else '20+'
            face_bins[bucket]=face_bins.get(bucket,0)+1
        # Keep every connected component counted while bounding report size. The
        # largest 256 retain bounds for visual classification; the histogram
        # exposes the full disconnected-island burden without dumping indices.
        representatives=sorted(islands,key=lambda island:(island['face_count'],island['vertex_count']),reverse=True)[:256]
        records.append(dict(object_name=obj.name,object_type='MESH',mesh_sha256=mesh_hash,vertex_count=len(bm.verts),edge_count=len(bm.edges),face_count=len(bm.faces),
            island_count=island_count,face_count_histogram=face_bins,unresolved_island_count=island_count,largest_islands=representatives))
        bm.free()
    return dict(meshes=len(meshes),dimensions=dims,dimension_unit=dimension_unit,triangles=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in meshes),materials=sorted({m.name for o in meshes for m in o.data.materials if m}),image_sizes=[list(im.size) for im in images],mesh_objects=[o.name for o in meshes],components=records,loose_geometry_status='needs-review until semantic attachment classification and visual inspection are recorded')
bpy.ops.wm.open_mainfile(filepath=str(folder/f'{a.asset_name}.blend'))
source=inspect([o for o in bpy.data.collections[asset_collection].all_objects if o.type=='MESH'])
bpy.context.scene.render.filepath=str(folder/'previews'/'FreshProcess.png');bpy.ops.render.render(write_still=True)
report={'blend':source,'checks':['Fresh process reopen and render'],'exports':{}}
report['material_check']='geometry only; UVs/textures not required or validated' if a.geometry_only else 'UVs and image dependencies checked'
for extension,op in [('glb',bpy.ops.import_scene.gltf),('fbx',bpy.ops.import_scene.fbx)]:
    exchange=folder/f'{a.asset_name}.{extension}'
    if not exchange.exists():
        report['exports'][extension]={'status':'missing','reason':'requested exchange format was not produced'}
        continue
    bpy.ops.wm.read_factory_settings(use_empty=True);op(filepath=str(exchange))
    check=inspect([o for o in bpy.context.scene.objects if o.type=='MESH'])
    assert max(abs(x-y) for x,y in zip(source['dimensions'],check['dimensions']))<.001,extension+' scale changed'
    assert source['triangles']==check['triangles'],extension+' triangle count changed'
    report['exports'][extension]=check
(folder/'validation-report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
