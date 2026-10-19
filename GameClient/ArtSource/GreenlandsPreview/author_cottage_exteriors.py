"""Continuous plaster infill, external timber framing and laid fieldstone plinth."""
import bpy,bmesh,math,random,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'PreparedArrival'
bpy.ops.wm.read_factory_settings(use_empty=True)
def box(loc,size,bevel=.006):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.scale=size;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        modifier=o.modifiers.new('Worn edges','BEVEL');modifier.width=bevel;modifier.segments=2;bpy.ops.object.modifier_apply(modifier=modifier.name)
    return o
def export(name,parts):
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts:o.select_set(True)
    bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
    bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');uv=o.data.uv_layers.active or o.data.uv_layers.new()
    for face in o.data.polygons:
        axis=max(range(3),key=lambda k:abs(face.normal[k]));a,b=[k for k in range(3) if k!=axis]
        for li in face.loop_indices:
            p=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(p[a]*.5,p[b]*.5)
    for v in o.data.vertices:v.co.y=-v.co.y
    bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
    bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
parts=[]
def wall(horizontal_min,horizontal_max,side,offset,holes):
    us=sorted(set([horizontal_min,horizontal_max]+[max(horizontal_min,min(horizontal_max,h[k])) for h in holes for k in [0,2]]))
    zs=sorted(set([.3,4]+[max(.3,min(4,h[k])) for h in holes for k in [1,3]]))
    for za,zb in zip(zs,zs[1:]):
        run=None
        for index,(ua,ub) in enumerate(zip(us,us[1:])):
            filled=not any(a<(ua+ub)/2<c and b<(za+zb)/2<d for a,b,c,d in holes)
            if filled and run is None:run=ua
            end=ub if filled and index==len(us)-2 else ua
            if run is not None and (not filled or index==len(us)-2):
                if side=='y':parts.append(box(((run+end)/2,offset,(za+zb)/2),(end-run,.055,zb-za),0))
                else:parts.append(box((offset,(run+end)/2,(za+zb)/2),(.055,end-run,zb-za),0))
                run=None
front_holes=[(-1.05,0,1.05,2.08)]+[(x-.49,2.46,x+.49,3.54) for x in [-3,-1,1,3]]+[(x-.49,.59,x+.49,1.67) for x in [-2.5,2.5]]
wall(-4.2,4.2,'y',-3.18,front_holes);wall(-4.2,4.2,'y',3.18,[])
for x in [-4.18,4.18]:wall(-3.2,3.2,'x',x,[(-1.99,2.46,-1.01,3.54)])
export('SM_CottagePlasterExterior',parts)
parts=[]
for y in [-3.24,3.24]:
    for x in [-4.12,-2,0,2,4.12]:
        bottom=2.1 if y<0 and x in [-2,0,2] else .36;parts.append(box((x,y,(bottom+4.02)/2),(.14,.16,4.02-bottom)))
    for z in [2.08,3.98]:parts.append(box((0,y,z),(8.45,.18,.15)))
    if y<0:
        for x in [-2.63,2.63]:parts.append(box((x,y,.38),(3.15,.18,.14)))
        for x in [-1.08,1.08]:parts.append(box((x,y,1.22),(.12,.18,1.74)))
    else:parts.append(box((0,y,.38),(8.45,.18,.14)))
for x in [-4.24,4.24]:
    for z in [.38,2.08,3.98]:parts.append(box((x,0,z),(.18,6.5,.15)))
    for y in [-3.12,0,3.12]:parts.append(box((x,y,2.19),(.16,.14,3.66)))
# Short diagonal corner braces stay below the upper windows.
for y in [-3.25,3.25]:
    for x in [-4,4]:
        a=Vector((x,y,2.18));b=Vector((x-math.copysign(.45,x),y,2.58));o=box((a+b)/2,(.095,.13,(b-a).length));o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler();parts.append(o)
export('SM_CottageExteriorFraming',parts)
rng=random.Random(8143);parts=[]
for side in range(4):
    for course in range(2):
        length=8.4 if side<2 else 6.4;cursor=-length/2
        while cursor<length/2-.03:
            width=min(rng.uniform(.31,.49),length/2-cursor);centre=cursor+width/2;cursor+=width
            if side==0 and abs(centre)<1.05+width/2:continue
            z=.08+course*.17+rng.uniform(-.008,.008)
            if side<2:parts.append(box((centre,(-1 if side==0 else 1)*3.25,z),(max(.03,width-.012),.2,.155),.018))
            else:parts.append(box(((-1 if side==2 else 1)*4.25,centre,z),(.2,max(.03,width-.012),.155),.018))
export('SM_CottageFieldstonePlinth',parts)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'CottageExteriors.blend'))
(OUT/'cottage-exteriors.json').write_text(json.dumps(dict(units='metres',dimensions=[8.5,6.5,4.05],entrance_width_metres=2.1,source='Original facade geometry; existing CC0 plaster/wood/rock PBR scans',collision='Existing supplied solid walls remain the collision shell; new exterior finish is cosmetic'),indent=2))
