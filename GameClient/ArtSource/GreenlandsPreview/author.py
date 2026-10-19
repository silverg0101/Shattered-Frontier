"""Deterministic, metre-scale scenery; Blender 5.2 background authoring."""
import bpy, math, random, json
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parent
random.seed(27)
bpy.ops.wm.read_factory_settings(use_empty=True)
def mat(name, color):
    m=bpy.data.materials.new(name); m.diffuse_color=(*color,1); return m
mats=[mat('Bark',(.19,.105,.046)),mat('Leaves',(.15,.29,.055)),mat('Rock',(.31,.34,.29)),mat('Grass',(.28,.39,.065)),mat('Petal',(.85,.65,.17)),mat('Ground',(.25,.34,.075))]
def mesh(name,verts,faces,mi=0):
    m=bpy.data.meshes.new(name);m.from_pydata(verts,[],faces);m.update()
    o=bpy.data.objects.new(name,m);bpy.context.collection.objects.link(o);o.data.materials.append(mats[mi]);return o
def branch(a,b,r1,r2):
    d=Vector(b)-Vector(a);bpy.ops.mesh.primitive_cone_add(vertices=9,radius1=r1,radius2=r2,depth=d.length,location=(Vector(a)+Vector(b))/2)
    o=bpy.context.object;o.rotation_euler=d.to_track_quat('Z','Y').to_euler();o.data.materials.append(mats[0]);return o
def ball(loc,scale,mi,sub=2):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=sub,radius=1,location=loc)
    o=bpy.context.object;o.scale=scale;o.data.materials.append(mats[mi])
    for v in o.data.vertices:v.co*=random.uniform(.85,1.15)
    return o
def export(name,objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:o.select_set(True)
    bpy.context.view_layer.objects.active=objs[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
    bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',add_leaf_bones=False)
    o.hide_set(True);return o
def height(x,y):
    # Level 18m clearing, gentle hills beyond; all dimensions metres.
    r=math.hypot(x,y);f=max(0,min(1,(r-10)/15))
    return f*(1.1+1.2*math.sin(x*.105)+.65*math.sin(y*.18)+.45*math.cos((x+y)*.14))
vs=[];fs=[];n=100
for j in range(n+1):
    y=-50+j
    for i in range(n+1):
        x=-50+i;vs.append((x,y,height(x,y)))
for j in range(n):
    for i in range(n):
        a=j*(n+1)+i;fs.extend([(a,a+1,a+n+2),(a,a+n+2,a+n+1)])
o=mesh('Terrain',vs,fs,5)
uv=o.data.uv_layers.new(name='UVMap');col=o.data.color_attributes.new(name='Color',type='BYTE_COLOR',domain='CORNER')
for p in o.data.polygons:
    p.use_smooth=True
    for li in p.loop_indices:
        x,y,z=vs[o.data.loops[li].vertex_index];uv.data[li].uv=(x/3,y/3)
        path=abs(y-(2*math.sin(x*.12)-3))
        clear=max(0,min(1,(9-math.hypot(x,y))/3))*.8
        dirt=max(clear,max(0,min(1,(2.4-path)/1.4)))
        col.data[li].color=(dirt,0,0,1)
export('SM_MeadowTerrain',[o])
for k in range(3):
    parts=[branch((0,0,0),(.12,0,5.8),.38,.17)]
    for i in range(13):
        ang=i*2.399+k;rad=random.uniform(1.2,2.8);z=random.uniform(4.5,7.3)
        end=(math.cos(ang)*rad,math.sin(ang)*rad,z)
        parts.append(branch((.05,0,2.2+i*.2),end,.13,.025))
        parts.append(ball(end,(random.uniform(1.25,1.9),random.uniform(1.2,1.8),random.uniform(1.05,1.55)),1,2))
    export('SM_Oak_%02d'%k,parts)
for k in range(3):export('SM_Boulder_%02d'%k,[ball((0,0,.45),(1.3+k*.25,.95,1+k*.15),2,2)])
verts=[];faces=[]
for i in range(11000):
    x=random.uniform(-46,46);y=random.uniform(-46,46)
    if math.hypot(x,y)<8 or abs(y-(2*math.sin(x*.12)-3))<2.2:continue
    z=height(x,y)-.015
    for j in range(3):
        a=random.random()*math.tau;w=random.uniform(.035,.075);h=random.uniform(.16,.45);dx=math.cos(a)*w;dy=math.sin(a)*w;q=len(verts)
        verts.extend([(x-dx,y-dy,z),(x+dx,y+dy,z),(x+dx*2,y+dy*2,z+h)])
        faces.extend([(q,q+1,q+2),(q+2,q+1,q)])
export('SM_MeadowGrass',[mesh('Grass',verts,faces,3)])
parts=[]
for i in range(45):
    x=random.uniform(-1.5,1.5);y=random.uniform(-1.5,1.5);z=random.uniform(.22,.45)
    parts.append(ball((x,y,z),(.065,.065,.035),4,1))
export('SM_Wildflowers',parts)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'GreenlandsPreview.blend'))
(OUT/'layout.json').write_text(json.dumps({'seed':27,'terrain_metres':100,'clearing_diameter_metres':18,'source':'Authored scenery; waystone reused from sibling GreenlandsWaystone package.'},indent=2))
