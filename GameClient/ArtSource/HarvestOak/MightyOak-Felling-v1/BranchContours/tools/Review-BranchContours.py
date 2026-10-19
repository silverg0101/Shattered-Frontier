"""Read-only geodesic ring/centerline feasibility audit on accepted oak wood."""
import bpy,bmesh,hashlib,json,math,os,heapq
from collections import defaultdict,deque
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),".."))
ASSET=os.path.abspath(os.path.join(ROOT,"..","..","MightyOak-v1","exports","MightyOak_v1.blend"))
SOURCE_FBX=os.path.abspath(os.path.join(ROOT,"..","..","MightyOak-v1","exports","SM_MightyOak_UpperStructure.fbx"))
OUT=os.path.join(ROOT,"CorrectedRootLinks","previews"); os.makedirs(OUT,exist_ok=True)
CORRECTED_OUT=os.path.dirname(OUT); os.makedirs(CORRECTED_OUT,exist_ok=True)
INTERVAL=0.75
MIN_RADIUS=0.07
bpy.context.preferences.filepaths.use_scripts_auto_execute=False
bpy.ops.wm.open_mainfile(filepath=ASSET)
scene=bpy.context.scene
src=bpy.data.objects.get("SM_MightyOak_UpperStructure")
if not src or src.type!="MESH": raise RuntimeError("Accepted upper wood source missing")
fol=bpy.data.objects.get("SM_MightyOak_Foliage")
source_hash=hashlib.sha256(open(SOURCE_FBX,"rb").read()).hexdigest()

def face_components(bm):
    unseen=set(bm.faces); comps=[]
    while unseen:
        seed=min(unseen,key=lambda f:f.index); unseen.remove(seed); q=[seed]; comp=[seed]
        while q:
            f=q.pop()
            for e in f.edges:
                for n in e.link_faces:
                    if n in unseen: unseen.remove(n); q.append(n); comp.append(n)
        comps.append(comp)
    return comps

def component_loops(component_id,faces):
    bedges={e for f in faces for e in f.edges if len(e.link_faces)==1}
    adj=defaultdict(set)
    for e in bedges:
        for v in e.verts: adj[v].add(e)
    unseen=set(bedges); loops=[]; irregular=[]
    while unseen:
        seed=min(unseen,key=lambda e:e.index); unseen.remove(seed); stack=[seed]; group={seed}
        while stack:
            e=stack.pop()
            for v in e.verts:
                for n in adj[v]:
                    if n in unseen: unseen.remove(n); group.add(n); stack.append(n)
        verts={v for e in group for v in e.verts}
        deg={v:sum(v in e.verts for e in group) for v in verts}
        if all(x==2 for x in deg.values()):
            # Deterministic cycle order.
            first=min(verts,key=lambda v:v.index); ordered=[first]; prev=None; cur=first
            while True:
                opts=[e for e in adj[cur] if e in group and (e.verts[0] if e.verts[1]==cur else e.verts[1])!=prev]
                if not opts: break
                edge=min(opts,key=lambda e:min(v.index for v in e.verts))
                nxt=edge.verts[0] if edge.verts[1]==cur else edge.verts[1]
                if nxt==first: break
                ordered.append(nxt); prev,cur=cur,nxt
                if len(ordered)>len(verts): break
            pts=[v.co.copy() for v in ordered]
            area_vec=Vector((0,0,0))
            for i,p in enumerate(pts): area_vec+=p.cross(pts[(i+1)%len(pts)])
            perimeter=sum((pts[i]-pts[(i+1)%len(pts)]).length for i in range(len(pts)))
            loops.append({"component":component_id,"ordered_vertices":ordered,"area_m2":area_vec.length*.5,"perimeter_m":perimeter,"vertex_count":len(ordered),"center_m":[sum(p[k] for p in pts)/len(pts) for k in range(3)]})
        else:
            irregular.append({"component":component_id,"edge_count":len(group),"vertices":len(verts),"boundary_degree_histogram":{str(d):sum(n==d for n in deg.values()) for d in sorted(set(deg.values()))}})
    return loops,irregular

def dijkstra(component_faces,root_vertices):
    verts={v for f in component_faces for v in f.verts}
    edges={e for f in component_faces for e in f.edges}
    adj=defaultdict(list)
    for e in sorted(edges,key=lambda e:e.index):
        a,b=e.verts; w=(a.co-b.co).length
        adj[a].append((b,w,e)); adj[b].append((a,w,e))
    dist={v:float('inf') for v in verts}; prev={}
    heap=[]
    for v in root_vertices:
        dist[v]=0.0; heapq.heappush(heap,(0.0,v.index,v))
    while heap:
        d,_,v=heapq.heappop(heap)
        if d!=dist[v]: continue
        for n,w,e in adj[v]:
            nd=d+w
            if nd<dist[n]-1e-12:
                dist[n]=nd; prev[n]=v; heapq.heappush(heap,(nd,n.index,n))
    return dist,prev,adj,edges

def point_for_edge(a,b,level):
    da,db=a[1],b[1]
    if abs(da-level)<1e-9: return ("v",a[0]),a[2].copy()
    if abs(db-level)<1e-9: return ("v",b[0]),b[2].copy()
    if (da-level)*(db-level)>=0: return None,None
    t=(level-da)/(db-da); lo,hi=sorted((a[0],b[0]))
    return ("e",lo,hi),a[2].lerp(b[2],t)

def contour_at_level(component_id,faces,dist,level):
    points={}; segments=[]; edge_node_map={}
    for f in faces:
        hits=[]
        for e in f.edges:
            a,b=e.verts
            key,p=point_for_edge((a.index,dist[a],a.co),(b.index,dist[b],b.co),level)
            if key is not None:
                points[key]=p; hits.append(key)
                if key[0]=='e': edge_node_map[(key[1],key[2])]=None
        hits=list(dict.fromkeys(hits))
        if len(hits)==2 and hits[0]!=hits[1]: segments.append((hits[0],hits[1],f.index))
        elif len(hits)>2:
            # Degenerate iso-vertex cases are retained as evidence, not paired arbitrarily.
            segments.append((hits[0],hits[1],f.index))
    node_segments=defaultdict(list)
    for si,(a,b,fi) in enumerate(segments): node_segments[a].append(si); node_segments[b].append(si)
    unseen=set(range(len(segments))); clusters=[]; segment_cluster={}
    while unseen:
        s=min(unseen); unseen.remove(s); q=[s]; inds=[s]
        while q:
            si=q.pop(); a,b,_=segments[si]
            for node in (a,b):
                for ns in node_segments[node]:
                    if ns in unseen: unseen.remove(ns); q.append(ns); inds.append(ns)
        nodes={n for si in inds for n in segments[si][:2]}
        degrees={n:sum(1 for si in node_segments[n] if si in inds) for n in nodes}
        pts=[points[n] for n in nodes]
        center=sum(pts,Vector((0,0,0)))/max(1,len(pts))
        area_vec=Vector((0,0,0)); perimeter=0.0
        closed=bool(nodes) and all(d==2 for d in degrees.values())
        ordered=[]
        if closed:
            start=min(nodes,key=str); ordered=[start]; prev=None; cur=start
            while True:
                nxts=[segments[si][1] if segments[si][0]==cur else segments[si][0] for si in node_segments[cur] if si in inds]
                nxt=next((n for n in nxts if n!=prev),None)
                if nxt is None or nxt==start: break
                ordered.append(nxt); prev,cur=cur,nxt
                if len(ordered)>len(nodes): break
            if len(ordered)==len(nodes):
                for i,n in enumerate(ordered):
                    p=points[n]; pn=points[ordered[(i+1)%len(ordered)]]
                    area_vec+=p.cross(pn); perimeter+=(pn-p).length
        radius_max=max(((p-center).length for p in pts),default=0.0)
        area=area_vec.length*.5 if closed and len(ordered)==len(nodes) else 0.0
        radius=math.sqrt(area/math.pi) if area>0 else math.sqrt(sum((p-center).length_squared for p in pts)/max(1,len(pts)))
        cid=len(clusters)
        keys=sorted(nodes,key=str)
        clusters.append({"id":cid,"component":component_id,"distance_m":level,"center_m":list(center),"equivalent_radius_m":radius,"max_radius_m":radius_max,"area_m2":area,"perimeter_m":perimeter,"closed_ring":closed and len(ordered)==len(nodes),"node_count":len(nodes),"segment_count":len(inds),"branch_nodes":sum(d>2 for d in degrees.values()),"open_nodes":sum(d==1 for d in degrees.values()),"crossing_keys":keys,"ordered_points":[list(points[n]) for n in ordered] if closed and len(ordered)==len(nodes) else [list(points[n]) for n in keys],"segment_lines":[[list(points[segments[si][0]]),list(points[segments[si][1]])] for si in inds]})
        for si in inds: segment_cluster[si]=cid
        for n in nodes:
            if n[0]=='e': edge_node_map[(n[1],n[2])]=cid
    return clusters,edge_node_map

bm=bmesh.new(); bm.from_mesh(src.data); bm.transform(src.matrix_world); bm.normal_update()
bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=0.0001)
bm.verts.ensure_lookup_table(); bm.edges.ensure_lookup_table(); bm.faces.ensure_lookup_table(); bm.verts.index_update(); bm.edges.index_update(); bm.faces.index_update()
vertex_by_index={v.index:v for v in bm.verts}
components=face_components(bm)
all_clusters=[]; component_audit=[]; center_links=[]; root_curves=[]; iso_curves=[]; irregular_boundary=[]; nonmanifold_edges=sum(1 for e in bm.edges if len(e.link_faces)>2)
for ci,faces in enumerate(components):
    loops,irregular=component_loops(ci,faces); irregular_boundary.extend(irregular)
    if not loops:
        component_audit.append({"component":ci,"faces":len(faces),"boundary_loops":0,"status":"no_boundary_root"}); continue
    root=max(loops,key=lambda x:(x["area_m2"],x["perimeter_m"]))
    root_vs=root["ordered_vertices"]
    dist,prev,adj,edges=dijkstra(faces,root_vs)
    max_distance=max(dist.values(),default=0)
    root_pts=[v.co.copy() for v in root_vs]
    root_cluster={"id":0,"global_id":len(all_clusters),"component":ci,"distance_m":0.0,"center_m":root["center_m"],"equivalent_radius_m":math.sqrt(max(0,root["area_m2"])/math.pi),"max_radius_m":0.0,"area_m2":root["area_m2"],"perimeter_m":root["perimeter_m"],"closed_ring":True,"node_count":len(root_pts),"segment_count":len(root_pts),"branch_nodes":0,"open_nodes":0,"crossing_keys":[],"ordered_points":[list(p) for p in root_pts],"root_boundary":True,"kept":math.sqrt(max(0,root["area_m2"])/math.pi)>=MIN_RADIUS}
    comp_clusters=[root_cluster]; comp_levels={0.0:[root_cluster]}; root_curves.append(root_pts)
    root_vertex_set={v.index for v in root_vs}
    prev_level_map={}
    # Root contour map: vertices on its boundary share the root identity.
    for e in edges:
        a,b=e.verts
        if (a.index in root_vertex_set and dist[b]>1e-9) or (b.index in root_vertex_set and dist[a]>1e-9):
            lo,hi=sorted((a.index,b.index)); prev_level_map[(lo,hi)]=root_cluster["global_id"]
    levels=[]; level_maps={0.0:prev_level_map}
    level=INTERVAL
    while level<max_distance+1e-8:
        clusters,edge_map=contour_at_level(ci,faces,dist,level)
        # Clusters map to global IDs; retain local maps for predecessor tracing.
        for cl in clusters:
            cl["global_id"]=len(all_clusters)+len(comp_clusters)
            cl["kept"]=cl["equivalent_radius_m"]>=MIN_RADIUS
            comp_clusters.append(cl)
        local_map={edge:clusters[cid]["global_id"] for edge,cid in edge_map.items() if cid is not None and cid<len(clusters)}
        level_maps[level]=local_map
        for c in clusters:
            c["_edge_map"]=None
            iso_curves.append([Vector(p) for p in c["ordered_points"]])
        comp_levels[level]=clusters; levels.append(level)
        level+=INTERVAL
    # Connect next-level contours through actual Dijkstra predecessor paths.
    for li,high in enumerate(levels):
        low=0.0 if li==0 else levels[li-1]
        low_map=level_maps[low]
        low_ids={c["global_id"] if "global_id" in c else 0 for c in comp_levels[low]}
        for child in comp_levels[high]:
            parents=set()
            for key in child["crossing_keys"]:
                if key[0]=='e':
                    a=vertex_by_index.get(key[1]); b=vertex_by_index.get(key[2])
                    if a is None or b is None: continue
                    cur=a if dist[a]>=high else b if dist[b]>=high else None
                elif key[0]=='v': cur=vertex_by_index.get(key[1])
                else: continue
                if cur is None: continue
                steps=0
                while cur in prev and dist[cur]>low+1e-8 and steps<100000:
                    p=prev[cur]
                    if dist[cur]>low+1e-8 and dist[p]<=low+1e-8:
                        edge=(min(cur.index,p.index),max(cur.index,p.index))
                        if edge in low_map: parents.add(low_map[edge])
                        elif low==0 and (cur.index in root_vertex_set or p.index in root_vertex_set): parents.add(root_cluster["global_id"])
                        break
                    cur=p; steps+=1
            for parent_id in parents:
                par=next((c for c in comp_clusters if c.get("global_id",0)==parent_id),root_cluster if parent_id==0 else None)
                if par:
                    center_links.append({"component":ci,"from_cluster":parent_id,"to_cluster":child["global_id"],"from_center_m":par["center_m"],"to_center_m":child["center_m"],"distance_m":math.dist(par["center_m"],child["center_m"]),"parent_count_for_child":len(parents)})
    all_clusters.extend(comp_clusters)
    root_info={k:v for k,v in root.items() if k!="ordered_vertices"}
    component_audit.append({"component":ci,"faces":len(faces),"vertices":len(dist),"boundary_loops":len(loops),"root_loop":root_info,"max_geodesic_distance_m":max_distance,"contour_levels":len(levels),"clusters":sum(len(v) for v in comp_levels.values()),"kept_clusters_radius_ge_0_07m":sum(1 for c in comp_clusters if c["kept"]),"ambiguous_contours":sum(1 for c in comp_clusters if c["branch_nodes"] or c["open_nodes"] or not c["closed_ring"]),"unreachable_vertices":sum(1 for d in dist.values() if not math.isfinite(d))})

closed_count=sum(1 for c in all_clusters if c["closed_ring"])
ambiguous_count=sum(1 for c in all_clusters if c["branch_nodes"] or c["open_nodes"] or not c["closed_ring"])
kept_count=sum(1 for c in all_clusters if c["kept"])
parent_children=defaultdict(set)
for link in center_links: parent_children[(link["component"],link["from_cluster"])].add(link["to_cluster"])
cluster_by_gid={c["global_id"]:c for c in all_clusters}
kept_links=[x for x in center_links if cluster_by_gid.get(x["from_cluster"],{}).get("kept") and cluster_by_gid.get(x["to_cluster"],{}).get("kept")]
kept_parent_children=defaultdict(set)
for link in kept_links: kept_parent_children[(link["component"],link["from_cluster"])].add(link["to_cluster"])
report={"schema":"mighty-oak-branch-contours-v1","status":"feasibility_review","source_url":"https://sketchfab.com/3d-models/mighty-oak-trees-4f6ab5594a8a415aba3f958682b9ced5","license":"CC BY 4.0","required_credit":"Mighty oak trees by Jagobo, CC BY 4.0","source_fbx_sha256":source_hash,"source_object":"SM_MightyOak_UpperStructure","units":"metres; accepted object matrix baked into analysis coordinates","weld_tolerance_m":0.0001,"dijkstra_edge_length_geodesic":True,"contour_interval_m":INTERVAL,"tiny_branch_filter_equivalent_radius_below_m":MIN_RADIUS,"source_vertices":len(bm.verts),"source_faces":len(bm.faces),"edge_connected_components":len(components),"boundary_loops_total":sum(x["boundary_loops"] for x in component_audit),"nonmanifold_edges_incidence_gt_2":nonmanifold_edges,"irregular_boundary_components":irregular_boundary,"contour_clusters_total":len(all_clusters),"closed_contour_clusters":closed_count,"ambiguous_or_open_contours":ambiguous_count,"clusters_kept_by_radius":kept_count,"centerline_link_count":len(center_links),"links_with_child_multiple_parents":sum(1 for x in center_links if x["parent_count_for_child"]>1),"parent_clusters_with_multiple_children":sum(1 for v in parent_children.values() if len(v)>1),"kept_centerline_link_count":len(kept_links),"kept_parent_clusters_with_multiple_children":sum(1 for v in kept_parent_children.values() if len(v)>1),"components":component_audit,"contours":[{k:v for k,v in c.items() if k not in {"crossing_keys","_edge_map","segment_lines"}} for c in all_clusters],"ambiguous_contour_segments":[{"global_id":c["global_id"],"component":c["component"],"distance_m":c["distance_m"],"center_m":c["center_m"],"equivalent_radius_m":c["equivalent_radius_m"],"branch_nodes":c["branch_nodes"],"open_nodes":c["open_nodes"],"segment_lines":c["segment_lines"]} for c in all_clusters if c.get("branch_nodes") or c.get("open_nodes") or not c.get("closed_ring")],"centerline_links":center_links,"note":"Audit-only: no collision hulls are generated. Crossings are from source mesh triangle interpolation; branch nodes/open contours flag ambiguity. Nonmanifold mesh edges are retained in Dijkstra adjacency and reported, not repaired."}
with open(os.path.join(CORRECTED_OUT,"branch-contour-audit.json"),"w",encoding="utf-8") as f: json.dump(report,f,indent=2)

# Review source wood plus actual geodesic contour polylines and centerline links.
if fol: fol.hide_render=True
scene.render.engine='BLENDER_EEVEE'; scene.render.image_settings.file_format='PNG'; scene.render.image_settings.color_mode='RGBA'; scene.render.resolution_percentage=100; scene.render.resolution_x=1200; scene.render.resolution_y=1200; scene.view_settings.view_transform='Standard'; scene.view_settings.look='Medium High Contrast'; scene.world=bpy.data.worlds.new('Branch Contour Review World'); scene.world.use_nodes=True; scene.world.node_tree.nodes.get('Background').inputs['Color'].default_value=(.16,.18,.20,1); scene.world.node_tree.nodes.get('Background').inputs['Strength'].default_value=.7
for ob in list(scene.objects):
    if ob.type in {'CAMERA','LIGHT'}: bpy.data.objects.remove(ob,do_unlink=True)
def color_material(name,color):
    m=bpy.data.materials.new(name); m.diffuse_color=(*color,1); m.use_nodes=True; m.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(*color,1); return m
ring_mat=color_material('Geodesic rings kept',(.02,.78,.95)); connector_mat=color_material('Geodesic centerline',(.95,.27,.04)); thin_mat=color_material('Filtered tiny twig rings',(.65,.65,.65))
def curve_obj(name,lines,material,radius,closed=False):
    data=bpy.data.curves.new(name,'CURVE'); data.dimensions='3D'; data.resolution_u=1; data.bevel_depth=radius; data.bevel_resolution=1
    for points in lines:
        if len(points)<2: continue
        sp=data.splines.new('POLY'); sp.points.add(len(points)-1)
        for p,co in zip(sp.points,points): p.co=(co[0],co[1],co[2],1)
        sp.use_cyclic_u=closed
    ob=bpy.data.objects.new(name,data); scene.collection.objects.link(ob); ob.data.materials.append(material); return ob
kept_lines=[]; thin_lines=[]
for c in all_clusters:
    if c.get('root_boundary',False):
        pts=[Vector(p) for p in c['ordered_points']]
        if c['kept'] and len(pts)>=3: kept_lines.append((pts,True))
        elif len(pts)>=3: thin_lines.append((pts,True))
        continue
    pts=[Vector(p) for p in c['ordered_points']]
    if c['closed_ring'] and len(pts)>=3:
        (kept_lines if c['kept'] else thin_lines).append((pts,True))
    elif c.get('segment_lines'):
        dst=kept_lines if c['kept'] else thin_lines
        dst.extend(([Vector(p) for p in seg],False) for seg in c['segment_lines'])
for name,items,mat in [('KeptRings',kept_lines,ring_mat),('FilteredTwigRings',thin_lines,thin_mat)]:
    data=bpy.data.curves.new(name,'CURVE'); data.dimensions='3D'; data.resolution_u=1; data.bevel_depth=.005; data.bevel_resolution=1
    for points,cyclic in items:
        spl=data.splines.new('POLY'); spl.points.add(len(points)-1)
        for p,co in zip(spl.points,points): p.co=(co[0],co[1],co[2],1)
        spl.use_cyclic_u=cyclic
    ob=bpy.data.objects.new(name,data); scene.collection.objects.link(ob); ob.data.materials.append(mat)
source_bvh=BVHTree.FromBMesh(bm,epsilon=1e-7)
def overlay_center(point):
    p=Vector(point); hit=source_bvh.find_nearest(p)
    if not hit: return p
    outward=hit[0]-p
    if outward.length<1e-5: outward=hit[1]
    if outward.length<1e-5: return p
    # Keep the computed center in JSON; render proxy is moved just outside the
    # source surface so opaque wood does not hide the centerline overlay.
    return hit[0]+outward.normalized()*0.02
center_lines=[[overlay_center(x['from_center_m']),overlay_center(x['to_center_m'])] for x in kept_links if x['parent_count_for_child']<=1]
curve_obj('GeodesicCenterlineLinks',center_lines,connector_mat,.009)
root_lines=[]
for component in component_audit:
    # Root boundary rings are already existing source edges; use ordered-loop centers only in JSON.
    pass

def area_light(name,loc,power,size):
    ld=bpy.data.lights.new(name,'AREA'); ld.energy=power; ld.shape='DISK'; ld.size=size; ob=bpy.data.objects.new(name,ld); scene.collection.objects.link(ob); ob.location=loc; ob.rotation_euler=(Vector((0,0,5))-ob.location).to_track_quat('-Z','Y').to_euler()
def render(name,loc,scale=17):
    cd=bpy.data.cameras.new('Review Camera'); cd.type='ORTHO'; cd.ortho_scale=scale; cam=bpy.data.objects.new('Review Camera',cd); scene.collection.objects.link(cam); cam.location=loc; cam.rotation_euler=(Vector((0,0,5))-cam.location).to_track_quat('-Z','Y').to_euler(); scene.camera=cam; scene.render.filepath=os.path.join(OUT,name); bpy.ops.render.render(write_still=True); bpy.data.objects.remove(cam,do_unlink=True); bpy.data.cameras.remove(cd)
area_light('Key',(6,-10,11),1800,6); area_light('Fill',(-8,-5,5),900,7); area_light('Rim',(0,8,7),1100,5)
render('BranchContours_Front.png',(0,-22,5)); render('BranchContours_Side.png',(22,0,5)); render('BranchContours_ThreeQuarter.png',(17,-20,12),18)
src.hide_render=True
for ob in scene.objects:
    if ob.name=='FilteredTwigRings': ob.hide_render=True
render('BranchContours_MajorNetwork_ThreeQuarter.png',(17,-20,12),18)
print('BRANCH_CONTOUR_AUDIT',json.dumps({"components":len(components),"boundary_loops":report['boundary_loops_total'],"contours":len(all_clusters),"closed":closed_count,"ambiguous":ambiguous_count,"kept_radius":kept_count,"centerline_links":len(center_links),"links_with_multi_parent":report['links_with_child_multiple_parents']}))
