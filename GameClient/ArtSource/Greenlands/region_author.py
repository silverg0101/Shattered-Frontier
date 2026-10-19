"""600m Greenlands region, walkable settlement scenery and full-scale paths."""
import bpy, sys, math, random, json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from region_layout import *
bpy.ops.wm.read_factory_settings(use_empty=True);random.seed(71)
def mesh(name,v,f):
    # UE's FBX import reflects Blender Y. Bake its inverse for authored world coordinates.
    m=bpy.data.meshes.new(name);m.from_pydata([(x,-y,z) for x,y,z in v],[],[tuple(reversed(face)) for face in f]);m.update();o=bpy.data.objects.new(name,m);bpy.context.collection.objects.link(o);return o
def export(name,objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:o.hide_set(False);o.select_set(True)
    bpy.context.view_layer.objects.active=objs[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
    bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(name+'.fbx')),use_selection=True,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
    o.hide_set(True)
v=[];f=[];n=300
for j in range(n+1):
    for i in range(n+1):
        x=-300+i*2;y=-300+j*2;v.append((x,y,height(x,y)))
for j in range(n):
    for i in range(n):
        a=j*(n+1)+i;f.extend([(a,a+1,a+n+2),(a,a+n+2,a+n+1)])
o=mesh('Region',v,f);uv=o.data.uv_layers.new();col=o.data.color_attributes.new(name='Color',type='BYTE_COLOR',domain='CORNER')
for p in o.data.polygons:
    p.use_smooth=True
    for li in p.loop_indices:
        x,y,z=v[o.data.loops[li].vertex_index];uv.data[li].uv=(x/3,y/3);col.data[li].color=(soil(x,y),0,0,1)
export('SM_GreenlandsRegion',[o])
v=[];f=[]
for i in range(105000):
    x=random.uniform(-230,230);y=random.uniform(-215,215)
    if soil(x,y)>.15:continue
    z=height(x,y)
    for k in range(2):
        a=random.random()*math.tau;w=random.uniform(.025,.05);h=random.uniform(.12,.3);dx=math.cos(a)*w;dy=math.sin(a)*w;q=len(v)
        v.extend([(x-dx,y-dy,z),(x+dx,y+dy,z),(x+dx*1.7,y+dy*1.7,z+h)]);f.append((q,q+1,q+2))
export('SM_RegionGrass',[mesh('Grass',v,f)])
materials={}
for name,c in [('Bark',(.19,.1,.04)),('Plaster',(.65,.53,.35)),('Roof',(.13,.16,.14)),('Stone',(.3,.3,.25)),('Canvas',(.55,.37,.13))]:
    m=bpy.data.materials.new(name);m.diffuse_color=(*c,1);materials[name]=m
def box(name,loc,scale,material,rot=0):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=name;o.scale=scale;o.rotation_euler[1]=rot;o.data.materials.append(materials[material]);return o
parts=[]
parts.append(box('Stone footing',(0,0,.1),(8.3,6.3,.2),'Stone'))
parts.append(box('Timber floor',(0,0,.23),(7.8,5.8,.08),'Bark'))
# Rear and side walls; front is split around an open 1.4 x 2.3m doorway.
parts.extend([box('Rear',(0,2.9,1.7),(8,.22,2.8),'Plaster'),box('West',(-3.9,0,1.7),(.22,5.8,2.8),'Plaster'),box('East',(3.9,0,1.7),(.22,5.8,2.8),'Plaster')])
for x in [-2.35,2.35]:parts.append(box('Front panel',(x,-2.9,1.7),(3.3,.22,2.8),'Plaster'))
parts.append(box('Door lintel',(0,-2.9,2.85),(1.4,.24,.5),'Bark'))
for x in [-3.9,-.8,.8,3.9]:
    for y in [-3,3]:parts.append(box('Oak frame',(x,y,1.7),(.22,.25,3),'Bark'))
for y in [-3,3]:parts.append(box('Crossbeam',(0,y,3.05),(8.3,.25,.25),'Bark'))
# Roof ridge runs along Y, each pitch is 2.5m rise over a 4.5m run.
angle=math.atan2(2.5,4.5);length=math.hypot(2.5,4.5)
for x in [-2.25,2.25]:parts.append(box('Slate roof',(x,0,4.3),(length,7.2,.18),'Roof',angle if x>0 else -angle))
for y in [-2.91,2.91]:
    g=mesh('Gable',[(-4,y,3.1),(4,y,3.1),(0,y,5.55)],[(0,1,2),(2,1,0)]);g.data.materials.append(materials['Plaster']);parts.append(g)
parts.append(box('Ridge cap',(0,0,5.58),(.22,7.2,.18),'Bark'))
parts.append(box('Chimney',(2,1.4,4.7),(.75,.75,2.5),'Stone'))
for x in [-.7,.7]:parts.append(box('Door jamb',(x,-3.05,1.4),(.15,.32,2.3),'Bark'))
export('SM_FootholdCottage',parts)
parts=[]
for x in [-1.5,1.5]:
    for y in [-1,1]:parts.append(box('Post',(x,y,1.25),(.14,.14,2.5),'Bark'))
parts.append(box('Market counter',(0,-.7,.95),(3,.7,.12),'Bark'));parts.append(box('Canvas awning',(0,0,2.5),(3.5,2.7,.07),'Canvas',.10))
export('SM_MarketStall',parts)
parts=[]
for i in range(9):
    bpy.ops.mesh.primitive_cylinder_add(vertices=10,radius=.22,depth=3,location=((i%3)*.42-.42,0,(i//3)*.4+.22),rotation=(math.pi/2,0,0));o=bpy.context.object;o.data.materials.append(materials['Bark']);parts.append(o)
export('SM_LogStack',parts)
parts=[box('Post',(0,0,1.1),(.15,.15,2.2),'Bark'),box('Direction board',(.32,0,1.95),(1.1,.15,.28),'Bark')];export('SM_RouteMarker',parts)
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'GreenlandsRegion.blend'))
(ROOT/'region-layout.json').write_text(json.dumps(dict(units='metres; Unreal import uses 100cm/m',area_size_metres=[600,600],route_metres=round(ROUTE_METRES,1),path_width_metres=4,build_plots_metres=[20,20],areas=AREAS,route=ROUTE,design='Vertical Slice Specification / Region layout and encounter envelope; dimensions locally authored, not a canonical full-biome size'),indent=2))
