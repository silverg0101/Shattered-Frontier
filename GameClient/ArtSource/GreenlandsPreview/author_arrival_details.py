"""Continuous worn-road ribbon, footpaths and metre-scale settlement details."""
import bpy,math,sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from region_layout import height,ROUTE
OUT=ROOT/'PreparedArrival';bpy.ops.wm.read_factory_settings(use_empty=True)
def mesh(name,v,faces):
    m=bpy.data.meshes.new(name);m.from_pydata([(x,-y,z) for x,y,z in v],[],[tuple(reversed(f)) for f in faces]);m.update();o=bpy.data.objects.new(name,m);bpy.context.collection.objects.link(o);return o
def export(name,objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:o.hide_set(False);o.select_set(True)
    bpy.context.view_layer.objects.active=objs[0]
    if len(objs)>1:bpy.ops.object.join()
    o=bpy.context.object;o.name=name;bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE');o.hide_set(True)
def spline(points,closed=False):
    out=[]
    for i in range(len(points)-1):
        p0=points[i-1] if i else points[0];p1=points[i];p2=points[i+1];p3=points[min(i+2,len(points)-1)]
        count=max(2,int(math.dist(p1,p2)))
        for j in range(count):
            t=j/count;out.append(tuple(.5*((2*p1[k])+(-p0[k]+p2[k])*t+(2*p0[k]-5*p1[k]+4*p2[k]-p3[k])*t*t+(-p0[k]+3*p1[k]-3*p2[k]+p3[k])*t*t*t) for k in range(2)))
    out.append(points[-1]);return out
paths=[(ROUTE,1.0),([(-101,-47),(-96,-44),(-90,-40),(-80,-37),(-77,-51)],.45),([(-105,-23),(-99,-28),(-90,-40),(-78,-30),(-76,-24)],.45)]
v=[];faces=[];weights=[];uvs=[]
for points,width in paths:
    curve=spline(points);start=len(v);distance=0
    for i,(x,y) in enumerate(curve):
        if i:distance+=math.dist(curve[i-1],(x,y))
        a=curve[max(0,i-1)];b=curve[min(len(curve)-1,i+1)];dx=b[0]-a[0];dy=b[1]-a[1];l=math.hypot(dx,dy) or 1
        for off,w in [(-3,0),(-2.3,.4),(-1.8,1),(1.8,1),(2.3,.4),(3,0)]:
            ox=x-dy/l*off*width;oy=y+dx/l*off*width;v.append((ox,oy,height(ox,oy)+.055));weights.append(w);uvs.append((distance/3,off*width/3))
        if i:
            q=start+(i-1)*6
            for k in range(5):faces.extend([(q+k,q+k+1,q+k+7),(q+k,q+k+7,q+k+6)])
# Worn market/arrival ground blends into the incoming and outgoing ribbons.
for cx,cy,r in [(-90,-40,13),(-185,-85,6)]:
    a=len(v);v.append((cx,cy,height(cx,cy)+.06));weights.append(1);uvs.append((cx/3,cy/3))
    for i in range(65):
        an=i*math.tau/64;rr=r*(1+.06*math.sin(i*1.9));x=cx+math.cos(an)*rr;y=cy+math.sin(an)*rr
        v.append((x,y,height(x,y)+.06));weights.append(0);uvs.append((x/3,y/3))
        if i:faces.append((a,a+i,a+i+1))
o=mesh('Continuous roads',v,faces);uv=o.data.uv_layers.new();col=o.data.color_attributes.new(name='Color',type='BYTE_COLOR',domain='CORNER')
for p in o.data.polygons:
    for li in p.loop_indices:
        vi=o.data.loops[li].vertex_index;uv.data[li].uv=uvs[vi];col.data[li].color=(weights[vi],weights[vi],weights[vi],1)
export('SM_GreenlandsWornRoads',[o])
def box(loc,scale):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.scale=scale
    bevel=o.modifiers.new('Worn edges','BEVEL');bevel.width=.015;bevel.segments=1;bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=bevel.name);return o
parts=[box((x,0,.65),(.14,.18,1.3)) for x in [-2,2]]
parts +=[box((0,0,z),(4,.10,.13)) for z in [.45,.98]]
export('SM_RoughFieldFence',parts)
parts=[box((0,0,1.1),(.14,.16,2.2)),box((.24,0,1.9),(1.4,.16,.37))];export('SM_SettlementSign',parts)
parts=[box((0,0,0),(1.2,.1,1.25))]
for x in [-.65,.65]:parts.append(box((x,-.07,0),(.1,.13,1.4)))
for z in [-.67,.67]:parts.append(box((0,-.07,z),(1.4,.13,.1)))
parts.append(box((0,-.08,0),(.055,.13,1.3)));export('SM_ClosedShutters',parts)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'ArrivalRoadsAndDetails.blend'))
(OUT/'road-layout.json').write_text(json.dumps(dict(road_width_metres=3.6,soft_shoulders_metres=1.2,footpath_width_metres=1.62,incoming_and_outgoing_route=ROUTE,geometry_units='metres',surface='visual ribbon over continuous collision terrain; no loading boundaries'),indent=2))
