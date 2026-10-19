"""Round-trip Blender validation for the versioned FBX collision packet."""
import bpy, bmesh, json, math, os
from mathutils import Vector
ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),".."))
PACKAGE=os.path.abspath(os.path.join(ROOT,"..","CollisionCandidate-v1"))
FBX=os.path.join(PACKAGE,"SM_MightyOak_FallCollision.fbx")
bpy.context.preferences.filepaths.use_scripts_auto_execute=False
for obj in list(bpy.context.scene.objects): bpy.data.objects.remove(obj,do_unlink=True)
bpy.ops.import_scene.fbx(filepath=FBX,use_custom_normals=True,ignore_leaf_bones=True,automatic_bone_orientation=False,global_scale=1.0)
objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
base=[o for o in objects if o.name.startswith("SM_MightyOak_FallCollision") and not o.name.startswith("UCX_")]
hulls=[o for o in objects if o.name.startswith("UCX_SM_MightyOak_FallCollision_")]
expected={f"UCX_SM_MightyOak_FallCollision_{i:02d}" for i in range(52)}
names={o.name for o in hulls}
if len(base)!=1: raise RuntimeError(f"expected one base mesh, found {[o.name for o in base]}")
if names!=expected: raise RuntimeError(f"UCX naming mismatch: missing={sorted(expected-names)}, extra={sorted(names-expected)}")
if len(objects)!=53: raise RuntimeError(f"expected 1 base + 52 UCX meshes, got {len(objects)}")
def world_vertices(obj): return [obj.matrix_world@v.co for v in obj.data.vertices]
def bounds(points): return {"min":[min(p[k] for p in points) for k in range(3)],"max":[max(p[k] for p in points) for k in range(3)],"dimensions":[max(p[k] for p in points)-min(p[k] for p in points) for k in range(3)]}
bpts=world_vertices(base[0]); hull_stats=[]
for ob in hulls:
    bm=bmesh.new(); bm.from_mesh(ob.data); bm.transform(ob.matrix_world); bm.normal_update()
    nonmanifold=sum(1 for e in bm.edges if len(e.link_faces)!=2)
    volume=abs(bm.calc_volume(signed=True))
    hull_stats.append({"name":ob.name,"vertices":len(ob.data.vertices),"faces":len(ob.data.polygons),"nonmanifold_edges":nonmanifold,"volume_m3":volume,"object_location":list(ob.location),"object_scale":list(ob.scale)})
    bm.free()
bounds_actual=bounds(bpts)
expected_dims=[13.6335268,12.2571111,8.9643155]
if any(abs(bounds_actual["dimensions"][i]-expected_dims[i])>.02 for i in range(3)): raise RuntimeError(f"FBX base bounds changed: {bounds_actual}")
if any(x["nonmanifold_edges"] for x in hull_stats): raise RuntimeError("FBX contains open UCX hulls")
if any(x["vertices"]>64 for x in hull_stats): raise RuntimeError("FBX contains hull over 64 vertices")
result={"status":"blender_roundtrip_pass","fbx":"SM_MightyOak_FallCollision.fbx","mesh_object_count":len(objects),"mesh_names":[o.name for o in objects],"base_mesh":base[0].name,"base_bounds_m":bounds_actual,"ucx_count":len(hulls),"ucx_hulls":hull_stats,"uniform_import_scale":1.0}
with open(os.path.join(PACKAGE,"validation.json"),"w",encoding="utf-8") as f: json.dump(result,f,indent=2)
print(json.dumps({k:v for k,v in result.items() if k!="ucx_hulls"},indent=2))
