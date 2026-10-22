"""Per house: wall-base to plinth-top gap, minimum visible plinth height, burial and proud edge, all in centimetres."""
import json, math, sys
from pathlib import Path
import unreal as u

actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
by={a.get_actor_label():a for a in actors}
terrain=next(a for a in actors if isinstance(a,u.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name()=='SM_Silverbrook_Terrain')
def ground(x,y):
    hit=terrain.static_mesh_component.line_trace_component(u.Vector(x,y,100000),u.Vector(x,y,-100000),True,False)
    assert hit,(x,y)
    return hit[0].z
rows=[]
for house in actors:
    if not isinstance(house,u.StaticMeshActor) or not str(house.get_folder_path()).startswith('Districts/'):continue
    mesh=house.static_mesh_component.static_mesh
    if not mesh or not mesh.get_name().startswith('SM_Silverbrook') or '_Door' in mesh.get_name():continue
    plinth=by.get(house.get_actor_label()+'_Plinth')
    if not plinth:
        rows.append(dict(house=house.get_actor_label(),error='missing plinth'));continue
    b=mesh.get_bounding_box();wall=house.get_actor_location().z+b.min.z*house.get_actor_scale3d().z
    p=plinth.get_actor_location();s=plinth.get_actor_scale3d();top=p.z+50*s.z;bottom=p.z-50*s.z
    yaw=math.radians(plinth.get_actor_rotation().yaw);co,si=math.cos(yaw),math.sin(yaw)
    levels=[ground(p.x+co*x-si*y,p.y+si*x+co*y) for x in (-50*s.x,50*s.x) for y in (-50*s.y,50*s.y)]
    levels.append(ground(p.x,p.y))
    dm=u.DynamicMesh();dm,_=u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(mesh,dm,u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD())
    pts=u.GeometryScript_List.convert_vector_list_to_array(u.GeometryScript_MeshQueries.get_all_vertex_positions(dm,False)[1])
    low=min(q.z for q in pts);near=[q for q in pts if q.z<=low+25];hs=house.get_actor_scale3d()
    # Local axes of plinth and building share the actor rotation, so proud edges compare in mesh space.
    sys.path.insert(0,str(Path(__file__).resolve().parents[4]/'GameClient/ArtSource/Silverbrook'));import silverbrook_props as sp
    X=sorted(q.x for q in near);Y=sorted(q.y for q in near)  # wall base = outermost vertex plane holding >=8% of the base band
    bx0,bx1,by0,by1=sp._edge(X,True)*hs.x,sp._edge(X,False)*hs.x,sp._edge(Y,True)*hs.y,sp._edge(Y,False)*hs.y
    hyaw=math.radians(house.get_actor_rotation().yaw);hc,hsn=math.cos(hyaw),math.sin(hyaw)
    dx,dy=p.x-house.get_actor_location().x,p.y-house.get_actor_location().y;lx,ly=hc*dx+hsn*dy,-hsn*dx+hc*dy
    sides=[(lx-50*s.x)-bx0,bx1-(lx+50*s.x),(ly-50*s.y)-by0,by1-(ly+50*s.y)]  # + = plinth stands proud on that side
    proud=[-v for v in sides]
    rows.append(dict(house=house.get_actor_label(),gap_cm=round(wall-top,3),min_proud_cm=round(min(proud),2),max_proud_cm=round(max(proud),2),
                     min_visible_cm=round(top-max(levels),2),min_buried_cm=round(min(levels)-bottom,2)))
bad=[x for x in rows if 'error' in x or abs(x['gap_cm'])>.1 or x['min_visible_cm']<50 or x['min_buried_cm']<100 or x['min_proud_cm']<-0.5 or x['max_proud_cm']>8]
report=dict(count=len(rows),failures=bad,min_visible_cm=min(x['min_visible_cm'] for x in rows if 'min_visible_cm' in x),
            min_proud_cm=min(x['min_proud_cm'] for x in rows if 'min_proud_cm' in x),max_proud_cm=max(x['max_proud_cm'] for x in rows if 'max_proud_cm' in x),max_abs_gap_cm=max(abs(x['gap_cm']) for x in rows if 'gap_cm' in x),houses=rows)
Path(__file__).with_name('foundation-check.json').write_text(json.dumps(report,indent=1),encoding='utf-8')
print('FOUNDATION_CHECK',report['count'],'failures',len(bad),'min_visible_cm',report['min_visible_cm'],'max_abs_gap_cm',report['max_abs_gap_cm'],'proud_cm',report['min_proud_cm'],report['max_proud_cm'])
assert not bad,bad[:5]
