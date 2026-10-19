"""Build a disposable convex-hull feasibility prototype from accepted source contours."""
import bpy, bmesh, json, math, os, hashlib, heapq, argparse, sys
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),".."))
ASSET=os.path.abspath(os.path.join(ROOT,"..","..","MightyOak-v1","exports","MightyOak_v1.blend"))
SOURCE_FBX=os.path.abspath(os.path.join(ROOT,"..","..","MightyOak-v1","exports","SM_MightyOak_UpperStructure.fbx"))
AUDIT=os.path.join(ROOT,"CorrectedRootLinks","branch-contour-audit.json")
MIN_RADIUS=0.06
if "--" not in sys.argv: raise SystemExit("Pass --output <fresh absolute directory> after Blender's --")
arg_parser=argparse.ArgumentParser(description="Build the source-derived oak collision candidate into a new output directory.")
arg_parser.add_argument("--output",required=True,help="fresh, absolute, non-existent output directory")
cli=arg_parser.parse_args(sys.argv[sys.argv.index("--")+1:])
if not os.path.isabs(cli.output): raise SystemExit("--output must be an absolute path")
OUTPUT_ROOT=os.path.normpath(cli.output)
if os.path.lexists(OUTPUT_ROOT): raise SystemExit(f"Refusing existing output directory: {OUTPUT_ROOT}")
if not os.path.isdir(os.path.dirname(OUTPUT_ROOT)): raise SystemExit(f"Output parent directory must already exist: {os.path.dirname(OUTPUT_ROOT)}")
os.mkdir(OUTPUT_ROOT)
OUT=os.path.join(OUTPUT_ROOT,f"LocalHullPrototype-r{int(MIN_RADIUS*100):02d}"); os.makedirs(os.path.join(OUT,"previews"),exist_ok=False)
PACKAGE=os.path.join(OUTPUT_ROOT,"CollisionCandidate-v1")
bpy.context.preferences.filepaths.use_scripts_auto_execute=False
bpy.ops.wm.open_mainfile(filepath=ASSET)
scene=bpy.context.scene
src=bpy.data.objects.get("SM_MightyOak_UpperStructure")
fol=bpy.data.objects.get("SM_MightyOak_Foliage")
if src is None: raise RuntimeError("accepted source mesh not found")
report=json.load(open(AUDIT,encoding="utf-8"))
source_hash=hashlib.sha256(open(SOURCE_FBX,"rb").read()).hexdigest()
if source_hash != report["source_fbx_sha256"]: raise RuntimeError("source hash changed")
clusters={c["global_id"]:c for c in report["contours"]}
ambiguous={c["global_id"]:c for c in report.get("ambiguous_contour_segments",[])}
links=[]
for link in report["centerline_links"]:
    a=clusters.get(link["from_cluster"]); b=clusters.get(link["to_cluster"])
    if not a or not b: continue
    clean_a=a.get("closed_ring") and not a.get("branch_nodes") and not a.get("open_nodes") and a.get("equivalent_radius_m",0)>=MIN_RADIUS
    clean_b=b.get("closed_ring") and not b.get("branch_nodes") and not b.get("open_nodes") and b.get("equivalent_radius_m",0)>=MIN_RADIUS
    if clean_a and clean_b:
        links.append((link,a,b,"full",None))
    elif clean_a and b["global_id"] in ambiguous and b.get("equivalent_radius_m",0)>=MIN_RADIUS:
        links.append((link,a,b,"local",ambiguous[b["global_id"]]))
    elif clean_b and a["global_id"] in ambiguous and a.get("equivalent_radius_m",0)>=MIN_RADIUS:
        links.append((link,a,b,"local",ambiguous[a["global_id"]]))
if len(links)>128: raise RuntimeError("prototype exceeds 128 hull bound")

# Build BVH in the same baked metre space as the contour audit.
sbm=bmesh.new(); sbm.from_mesh(src.data); sbm.transform(src.matrix_world); sbm.normal_update()
source_bvh=BVHTree.FromBMesh(sbm,epsilon=0.0)
hull_objects=[]; hull_bvhs=[]; metrics=[]
def local_arc_points(contour,near_center,radius_m=0.18):
    key=lambda p:tuple(round(float(v),6) for v in p)
    adj={}
    for line in contour["segment_lines"]:
        aa,bb=map(key,line); adj.setdefault(aa,{})[bb]=math.dist(aa,bb); adj.setdefault(bb,{})[aa]=math.dist(aa,bb)
    start=min(adj,key=lambda p:math.dist(p,near_center))
    dist={start:0.0}; heap=[(0.0,start)]
    while heap:
        d,node=heapq.heappop(heap)
        if d!=dist[node]: continue
        for nxt,w in adj[node].items():
            nd=d+w
            if nd<dist.get(nxt,float('inf')):
                dist[nxt]=nd; heapq.heappush(heap,(nd,nxt))
    selected=[Vector(p) for p,d in dist.items() if d<=radius_m]
    if len(selected)<4: selected=[Vector(p) for p in sorted(dist,key=dist.get)[:4]]
    return selected

if len(links)>64: raise RuntimeError(f"local fork prototype exceeds preferred 64-hull budget ({len(links)})")
for hi,(link,a,b,mode,fork) in enumerate(links):
    if mode=="full": pts=[Vector(p) for p in a["ordered_points"]+b["ordered_points"]]
    else:
        clean=a if a["global_id"] not in ambiguous else b
        arc=local_arc_points(fork,clean["center_m"])
        pts=[Vector(p) for p in clean["ordered_points"]]+arc
    hm=bmesh.new(); hv=[hm.verts.new(p) for p in pts]; hm.verts.ensure_lookup_table()
    bmesh.ops.convex_hull(hm,input=hv, use_existing_faces=False)
    loose=[v for v in hm.verts if not v.link_faces]
    if loose: bmesh.ops.delete(hm,geom=loose,context='VERTS')
    hm.normal_update()
    if len(hm.faces)<4 or not hm.is_valid: raise RuntimeError(f"invalid hull {hi}")
    # Local hull is represented as an independent closed convex mesh.
    coords=[v.co.copy() for v in hm.verts]
    mesh=bpy.data.meshes.new(f"LocalHull_{hi:03d}_Mesh")
    mesh.from_pydata([tuple(v) for v in coords],[],[[v.index for v in f.verts] for f in hm.faces]); mesh.update()
    ob=bpy.data.objects.new(f"LocalHull_{hi:03d}",mesh); scene.collection.objects.link(ob); hull_objects.append(ob)
    hb=bmesh.new(); hb.from_mesh(mesh); hull_bvhs.append(BVHTree.FromBMesh(hb,epsilon=0.0)); hb.free()
    # Sample hull face centers/edge midpoints against the actual wood surface to detect bridges.
    samples=[]
    for f in hm.faces:
        vs=list(f.verts); samples.append(sum((v.co for v in vs),Vector())/len(vs))
        for i,v in enumerate(vs): samples.append((v.co+vs[(i+1)%len(vs)].co)*.5)
    ds=[]
    for p in samples:
        hit=source_bvh.find_nearest(p)
        if hit and hit[0] is not None: ds.append(hit[3])
    vol=abs(hm.calc_volume(signed=True)); nonmanifold=sum(1 for e in hm.edges if len(e.link_faces)!=2)
    metrics.append({"hull":hi,"mode":mode,"fork_cluster":fork["global_id"] if fork else None,"component":link["component"],"from_cluster":link["from_cluster"],"to_cluster":link["to_cluster"],"link_distance_m":link["distance_m"],"vertices":len(hm.verts),"faces":len(hm.faces),"nonmanifold_edges":nonmanifold,"volume_m3":vol,"source_surface_sample_count":len(ds),"surface_samples_over_0_10m":sum(d>.10 for d in ds),"surface_samples_over_0_25m":sum(d>.25 for d in ds),"max_hull_surface_sample_distance_m":max(ds,default=0.0),"mean_hull_surface_sample_distance_m":sum(ds)/max(1,len(ds))})
    hm.free()

# Preview overlay: translucent cyan hulls over the unchanged wood; no export is produced.
if fol: fol.hide_render=True
mat=bpy.data.materials.new("Prototype hull overlay"); mat.diffuse_color=(.02,.8,1,.35); mat.use_nodes=True
bs=mat.node_tree.nodes.get("Principled BSDF"); bs.inputs["Base Color"].default_value=(.02,.8,1,.35); bs.inputs["Alpha"].default_value=.35
try: mat.surface_render_method='DITHERED'
except: pass
for ob in hull_objects: ob.data.materials.append(mat)
scene.render.engine='BLENDER_EEVEE'; scene.render.image_settings.file_format='PNG'; scene.render.image_settings.color_mode='RGBA'; scene.render.resolution_percentage=100; scene.render.resolution_x=1400; scene.render.resolution_y=1400
scene.view_settings.view_transform='Standard'; scene.view_settings.look='Medium High Contrast'
scene.render.film_transparent=False
for ob in list(scene.objects):
    if ob.type in {'CAMERA','LIGHT'}: bpy.data.objects.remove(ob,do_unlink=True)
world=bpy.data.worlds.new('Local hull prototype world'); world.use_nodes=True; world.node_tree.nodes.get('Background').inputs['Color'].default_value=(.13,.15,.17,1); world.node_tree.nodes.get('Background').inputs['Strength'].default_value=.8; scene.world=world
light_data=bpy.data.lights.new('Review key','AREA'); light_data.energy=2200; light_data.size=9; light=bpy.data.objects.new('Review key',light_data); scene.collection.objects.link(light); light.location=(5,-8,13)
bpy.context.view_layer.update()
corners=[Vector(c) for c in src.bound_box]; corners=[src.matrix_world@c for c in corners]
center=sum(corners,Vector())/8; span=max((max(p[i] for p in corners)-min(p[i] for p in corners) for i in range(3)))
views=[('Front',Vector((0,-1,0))),('Side',Vector((1,0,0))),('ThreeQuarter',Vector((1,-1,.16)).normalized())]
for name,direction in views:
    cam_data=bpy.data.cameras.new('ReviewCamera_'+name); cam_data.type='ORTHO'; cam_data.ortho_scale=span*1.24
    cam=bpy.data.objects.new('ReviewCamera_'+name,cam_data); scene.collection.objects.link(cam); cam.location=center+direction*span*2.1
    cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler(); scene.camera=cam
    light.location=center+Vector((-.4,-.6,1.1))*span
    scene.render.filepath=os.path.join(OUT,'previews',f'LocalHulls_{name}.png'); bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam,do_unlink=True)

total_samples=sum(m["source_surface_sample_count"] for m in metrics)
source_points=[v.co.copy() for v in sbm.verts]
z0=min(p.z for p in source_points)
covered={"0_10m":0,"0_25m":0,"0_50m":0}; bands={}
for p in source_points:
    dmin=float('inf')
    for hb in hull_bvhs:
        hit=hb.find_nearest(p)
        if hit and hit[0] is not None: dmin=min(dmin,hit[3])
    covered["0_10m"]+=dmin<=.10
    covered["0_25m"]+=dmin<=.25
    covered["0_50m"]+=dmin<=.50
    band=str(int((p.z-z0)//1.0))
    item=bands.setdefault(band,{"z_from_ground_m":int(band),"source_vertices":0,"within_0_25m_of_hulls":0})
    item["source_vertices"]+=1; item["within_0_25m_of_hulls"]+=dmin<=.25
for item in bands.values(): item["coverage_fraction_0_25m"]=item["within_0_25m_of_hulls"]/max(1,item["source_vertices"])
report_out={"schema":"mighty-oak-local-hull-prototype-v1","status":"feasibility_only_not_accepted","source_url":report["source_url"],"license":report["license"],"required_credit":report["required_credit"],"source_fbx_sha256":source_hash,"method":"Clean adjacent contours receive direct local hulls. If exactly one end is an ambiguous fork contour, the clean ring is hulled to a 0.18m contour-geodesic arc nearest its center; each linked child receives its own piece. This resolves local bands without convexifying the full fork. Roots use corrected global IDs. No source mesh is modified.","minimum_contour_radius_m":MIN_RADIUS,"ambiguous_arc_radius_m":0.18,"hull_count":len(metrics),"piece_budget_ceiling":64,"vertices_total":sum(m["vertices"] for m in metrics),"faces_total":sum(m["faces"] for m in metrics),"volume_total_m3":sum(m["volume_m3"] for m in metrics),"hull_surface_samples_total":total_samples,"hull_surface_samples_over_0_10m":sum(m["surface_samples_over_0_10m"] for m in metrics),"hull_surface_samples_over_0_25m":sum(m["surface_samples_over_0_25m"] for m in metrics),"max_hull_surface_sample_distance_m":max((m["max_hull_surface_sample_distance_m"] for m in metrics),default=0),"source_vertex_count":len(source_points),"source_vertices_within_0_10m_of_hulls":covered["0_10m"],"source_vertices_within_0_25m_of_hulls":covered["0_25m"],"source_vertices_within_0_50m_of_hulls":covered["0_50m"],"source_vertex_coverage_by_1m_height_band":bands,"hulls":metrics,"preview_files":[f"previews/LocalHulls_{x}.png" for x,_ in views],"limitation":"Hull face/edge samples farther than the source surface expose bridge spans. Source-vertex proximity exposes uncovered wood surface. Neither metric is a physics or runtime test."}
with open(os.path.join(OUT,"prototype-metrics.json"),"w",encoding="utf-8") as f: json.dump(report_out,f,indent=2)

# Package the reviewed source-derived pieces as a versioned editable collision candidate.
clean_ids={c["global_id"] for c in report["contours"] if c.get("closed_ring") and not c.get("branch_nodes") and not c.get("open_nodes") and c.get("equivalent_radius_m",0)>=MIN_RADIUS}
represented={gid for m in metrics for gid in (m["from_cluster"],m["to_cluster"]) if gid is not None}
represented|={m["fork_cluster"] for m in metrics if m["fork_cluster"] is not None}
fork_ids={c["global_id"] for c in ambiguous.values() if c.get("equivalent_radius_m",0)>=MIN_RADIUS}
if not clean_ids.issubset(represented): raise RuntimeError(f"uncovered clean contour clusters: {sorted(clean_ids-represented)}")
if not fork_ids.issubset(represented): raise RuntimeError(f"uncovered ambiguous fork clusters: {sorted(fork_ids-represented)}")
if any(m["nonmanifold_edges"] for m in metrics): raise RuntimeError("candidate has open/nonmanifold convex hull")
if any(m["vertices"]>64 for m in metrics): raise RuntimeError("candidate hull exceeds 64-vertex limit")
if report_out["hull_surface_samples_over_0_25m"]: raise RuntimeError("candidate includes a hull bridge sample >0.25 m from source wood")
os.makedirs(PACKAGE,exist_ok=False)
base_data=src.data.copy(); base_bm=bmesh.new(); base_bm.from_mesh(base_data); base_bm.transform(src.matrix_world); base_bm.normal_update(); base_bm.to_mesh(base_data); base_bm.free(); base_data.materials.clear()
base=bpy.data.objects.new("SM_MightyOak_FallCollision",base_data); base.matrix_world=Matrix.Identity(4)
candidate_scene=bpy.data.scenes.new("MightyOak Collision Candidate v1")
candidate_collection=bpy.data.collections.new("MightyOak Collision Candidate v1")
candidate_scene.collection.children.link(candidate_collection); candidate_collection.objects.link(base)
for m,ob in zip(metrics,hull_objects):
    ob.name=f"UCX_SM_MightyOak_FallCollision_{m['hull']:02d}"
    ob.data.name=ob.name+"_Mesh"; ob.data.materials.clear(); candidate_collection.objects.link(ob)
    if any(abs(v)>1e-8 for v in ob.location) or any(abs(v-1)>1e-8 for v in ob.scale): raise RuntimeError(f"non-identity hull transform: {ob.name}")
candidate_scene.unit_settings.system='METRIC'; candidate_scene.unit_settings.scale_length=1.0
scene.name="Prototype Source Review"
bpy.context.window.scene=candidate_scene
bpy.ops.object.select_all(action='DESELECT')
export_objects=[base]+hull_objects
for ob in export_objects: ob.select_set(True)
bpy.context.view_layer.objects.active=base
fbx_path=os.path.join(PACKAGE,"SM_MightyOak_FallCollision.fbx")
bpy.ops.export_scene.fbx(filepath=fbx_path,use_selection=True,object_types={'MESH'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',bake_space_transform=False,add_leaf_bones=False,mesh_smooth_type='OFF',path_mode='AUTO',embed_textures=False,axis_forward='-Z',axis_up='Y')
blend_path=os.path.join(PACKAGE,"MightyOak_FallCollision_v1.blend")
bpy.ops.wm.save_as_mainfile(filepath=blend_path)
fbx_hash=hashlib.sha256(open(fbx_path,'rb').read()).hexdigest()
bounds_min=[min(p[k] for p in source_points) for k in range(3)]; bounds_max=[max(p[k] for p in source_points) for k in range(3)]
manifest={"schema":"mighty-oak-felling-collision-candidate-v1","status":"generated_candidate_not_imported; motion_and_quality_review_pending","source":{"url":report["source_url"],"license":report["license"],"required_credit":report["required_credit"],"accepted_upper_fbx_sha256":source_hash},"outputs":{"fbx":"SM_MightyOak_FallCollision.fbx","fbx_sha256":fbx_hash,"editable_blend":"MightyOak_FallCollision_v1.blend","base_mesh":"SM_MightyOak_FallCollision","collision_object_pattern":"UCX_SM_MightyOak_FallCollision_00 through _51"},"coordinate_system":{"source_blender_axes":"X right, Y depth, Z up","fbx_forward":"-Z","fbx_up":"Y","units":"meters in Blender; FBX unit scale baked, import scale 1.0","source_bounds_m":{"min":bounds_min,"max":bounds_max,"dimensions":[bounds_max[k]-bounds_min[k] for k in range(3)]},"base_and_hull_object_transforms":"identity; mesh coordinates baked to source world origin"},"construction":{"minimum_clean_contour_radius_m":MIN_RADIUS,"fork_local_arc_radius_m":0.18,"clean_closed_contours_represented":len(clean_ids),"clean_contours_total":len(clean_ids),"ambiguous_fork_contours_represented":len(fork_ids),"ambiguous_fork_contours_total":len(fork_ids),"hull_count":len(metrics),"hull_vertices_total":sum(m["vertices"] for m in metrics),"hull_faces_total":sum(m["faces"] for m in metrics),"maximum_vertices_per_hull":max(m["vertices"] for m in metrics),"nonmanifold_hull_edges_total":sum(m["nonmanifold_edges"] for m in metrics),"hull_surface_sample_count":total_samples,"samples_over_0_10m_from_source":report_out["hull_surface_samples_over_0_10m"],"samples_over_0_25m_from_source":report_out["hull_surface_samples_over_0_25m"],"maximum_hull_surface_sample_distance_m":report_out["max_hull_surface_sample_distance_m"],"source_vertices_below_0_06m_contour_threshold_not_represented_as_collision":"fine twigs and slender high branches remain outside the collision target"},"runtime_note":"52 UCX pieces fit within the V2 64-piece candidate ceiling. A newly generated output is not imported or runtime-adopted; fall motion and collision quality remain to be reviewed.","hulls":metrics}
with open(os.path.join(PACKAGE,"manifest.json"),"w",encoding="utf-8") as f: json.dump(manifest,f,indent=2)
with open(os.path.join(PACKAGE,"README.md"),"w",encoding="utf-8") as f:
    f.write("# Mighty Oak felling collision candidate v1\n\n")
    f.write("**Status:** Newly generated output; not imported or runtime-adopted. Fall motion and collision quality remain to be reviewed. Source: **Mighty oak trees by Jagobo, CC BY 4.0**. The accepted source asset is unchanged.\n\n")
    f.write(f"The FBX contains base mesh `SM_MightyOak_FallCollision` plus {len(metrics)} closed convex objects named `UCX_SM_MightyOak_FallCollision_00`–`_{len(metrics)-1:02d}`. All 54 clean contour bands at or above 0.06 m equivalent radius and all four ambiguous fork bands at or above that radius are represented. Fork hulls use a 0.18 m local contour-geodesic arc, split per linked neighbor. Thin limbs below 0.06 m remain excluded.\n\n")
    f.write(f"Hull validation: 0 nonmanifold hull edges, maximum {max(m['vertices'] for m in metrics)} vertices per hull, {total_samples} hull surface samples, {report_out['hull_surface_samples_over_0_25m']} samples over 0.25 m from source wood, maximum distance {report_out['max_hull_surface_sample_distance_m']:.3f} m. Three-view overlays are in `../LocalHullPrototype-r06/previews/`.\n\n")
    f.write("Import units are meters with FBX unit scale baked and uniform import scale 1.0. Blender scene axes are X/Y/Z with Z up; FBX export uses forward -Z and up Y. Source origin and identity transforms are shared by the base mesh and every UCX object.\n\n")
    f.write("The 52 pieces fit within the V2 64-piece candidate ceiling. This freshly generated output has not been imported into Unreal. Review contact behavior, rooted fall motion, and collision quality before runtime use.\n\n")
    f.write("Reproduce from the repository root into a new absolute path: `blender --background --factory-startup --threads 4 --python GameClient/ArtSource/HarvestOak/MightyOak-Felling-v1/BranchContours/tools/Build-LocalHullPrototype.py -- --output C:/path/to/new-output-directory`. The output directory must not exist; the builder refuses existing paths.\n")
sbm.free()
print(json.dumps({k:v for k,v in report_out.items() if k!="hulls"},indent=2))
