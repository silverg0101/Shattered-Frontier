"""Silverbrook street prop: medieval wooden wheelbarrow (designed to match Silverbrook; no owner concept).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propwheelbarrow.py
Oak rails with a plank tray, two legs and a spoked wooden wheel with an iron tyre at the front, after the owner's Street Props concept sheet.
"""
import sys,math,json
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector
import bpy
from mathutils import Matrix
NAME='PropWheelbarrow';scene=kit.reset(5101)
asset=bpy.data.collections.new('SilverbrookPropWheelbarrow');scene.collection.children.link(asset)
M=dict(timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    boards=kit.scan('SB_PropWheelbarrow_Boards','old_planks_02',aged=.3),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9))
BANDS=kit.bands('weathered_brown_planks');band=lambda:kit.rng.choice(BANDS)
G=dict(wood=Geo('SM_SBWheelbarrow_Frame',asset),boards=Geo('SM_SBWheelbarrow_Tray',asset),wheel=Geo('SM_SBWheelbarrow_Wheel',asset),iron=Geo('SM_SBWheelbarrow_Iron',asset));EXTRA=[]
EXPECT=[(.45,.75),(1.4,1.6),(.55,.8)];BUDGET=40000
PLACEMENT=dict(footprint_m=None,front='-Y',ground_contact=True,near=['farmhouse','stables','granary','market','cottage','blacksmith'],clearance_m=1.2)
X=Vector((1,0,0))
def plank(g,a,b,wdir,wid,thk,mat,**kw):
    a,b=Vector(a),Vector(b);d=(b-a).normalized();w=(Vector(wdir)-d*Vector(wdir).dot(d)).normalized();n=w.cross(d)
    g.box((a+b)/2,(thk,wid,(b-a).length),mat,.004,Matrix((n,w,d)).transposed(),grain=True,band=band(),**kw)
# ---- frame ---------------------------------------------------------------------
K=.307;th=math.atan(K);D=Vector((0,math.cos(th),math.sin(th)));N=Vector((0,-math.sin(th),math.cos(th)))
zr=lambda y:.25+(y+.55)*K;xr=lambda y:.12+(y+.62)/1.34*.12
for sg in (-1,1):G['wood'].beam((sg*xr(-.62),-.62,zr(-.62)),(sg*xr(.72),.72,zr(.72)),.05,.065,M['timber'],band=band())
P=lambda s,o=0:Vector((0,-.28,zr(-.28)))+D*s+N*(.0325+o)
L=.62
for i in range(3):
    x=-.133+i*.133;plank(G['boards'],P(0,.01)+X*x,P(L,.01)+X*x,X,.13,.02,M['boards'])
phi=math.radians(22)
for sg in (-1,1):
    w=N*math.cos(phi)+X*sg*math.sin(phi);e0=P(0,.02)+X*sg*.2
    for h in (.065,):plank(G['boards'],e0+w*h,e0+w*h+D*L,w,.12,.022,M['boards'])
for h in (.06,):  # front board leans forward, back board is lower and leans back
    w=N*math.cos(phi)-D*math.sin(phi);e=P(0,.02);plank(G['boards'],e+w*h-X*.25,e+w*h+X*.25,w,.105,.022,M['boards'])
w=N*math.cos(phi)+D*math.sin(phi);e=P(L,.02);plank(G['boards'],e+w*.04-X*.24,e+w*.04+X*.24,w,.08,.022,M['boards'])
for sg in (-1,1):  # iron straps round the tray rim ends and a bearer under the tray
    G['iron'].box(P(.5,-.0375)+X*sg*.17,(.04,.02,.01),M['iron'],.002,Matrix((X,N,D)).transposed())
for s in (.12,.5):G['wood'].beam(P(s,-.04)-X*.22,P(s,-.04)+X*.22,.03,.03,M['timber'],band=band())
for sg in (-1,1):
    G['wood'].beam((sg*xr(.4),.4,zr(.4)-.01),(sg*.25,.54,0),.05,.055,M['timber'],band=band())
G['wood'].beam((-.25,.49,.2),(.25,.49,.2),.032,.04,M['timber'],band=band())
# ---- wheel: hub, 8 spokes, 12 felloes, iron tyre -------------------------------------
C=Vector((0,-.55,.25));rad=lambda t:Vector((0,math.cos(t),math.sin(t)));tan=lambda t:Vector((0,-math.sin(t),math.cos(t)))
G['wheel'].prism([C+rad(math.tau*k/12)*.045 for k in range(12)],X,.12,M['timber'],swap=True)
for k in range(8):
    t=math.tau*k/8;r_,t_=rad(t),tan(t);G['wheel'].box(C+r_*.12,(.03,.034,.17),M['timber'],.003,Matrix((X,-t_,r_)).transposed(),grain=True,band=band())
for k in range(12):
    t=math.tau*(k+.5)/12;rm=.215;G['wheel'].box(C+rad(t)*rm,(.06,.04,2*rm*math.sin(math.pi/12)+.004),M['timber'],.004,Matrix((X,rad(t),tan(t))).transposed(),grain=True,band=band())
for k in range(24):
    t=math.tau*k/24;rm=.244;G['iron'].box(C+rad(t)*rm,(.052,.012,2*rm*math.sin(math.pi/24)+.004),M['iron'],.001,Matrix((X,rad(t),tan(t))).transposed())
G['iron'].beam(C-X*.2,C+X*.2,.022,.022,M['iron'],grain=False)
for sg in (-1,1):G['iron'].box(C+X*sg*.075,(.012,.1,.1),M['iron'],.003)
# ---- export, preview scene, manifest ---------------------------------------------
import math as _m
from mathutils import Matrix
objs=[g.object() for g in G.values()]+EXTRA
import bmesh,hashlib
for k,o in zip(list(G),objs):
    if k in globals().get('SMOOTH',[]):
        me=o.data;bm=bmesh.new();bm.from_mesh(me);bmesh.ops.remove_doubles(bm,verts=bm.verts,dist=1e-5)
        for e in bm.edges:
            if len(e.link_faces)==2 and e.calc_face_angle(0)>.8:e.smooth=False
        bm.to_mesh(me);bm.free()
        for p in me.polygons:p.use_smooth=True
for o in objs:o.data.transform(Matrix.Scale(globals().get('SCALE',1),4))
def bounds():
    pts=[o.matrix_world@Vector(c) for o in objs for c in o.bound_box];return [min(p[k] for p in pts) for k in range(3)],[max(p[k] for p in pts) for k in range(3)]
lo,hi=bounds()
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,-lo[2])))
lo,hi=bounds();size=[h-l for l,h in zip(lo,hi)]
SLOT=tuple(_m.ceil(round(s*10,4))/10 for s in size)
assert all(s<=t+1e-6 for s,t in zip(size,SLOT)),f'exceeds slot {SLOT}: {size}'
assert all(a<=s<=b for s,(a,b) in zip(size,EXPECT)),f'size {size} outside plausible real-world range {EXPECT}'
OUT=HERE/f'Silverbrook{NAME}'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(_m.radians(52),0,_m.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[_m.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
R_=max(size)*1.0;cz=size[2]/2
cam('Beauty',(-R_*1.15,-R_*1.5,cz+R_*.65),(70,0,-37),lens=50);cam('Front',(0,-20,cz),(90,0,0),R_*1.25);cam('Side',(-20,0,cz),(90,0,-90),R_*1.25);cam('Rear',(0,20,cz),(90,0,180),R_*1.25)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
PLACEMENT['footprint_m']=list(SLOT[:2]);tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs);assert tris<=BUDGET,f'{tris} triangles'
manifest=dict(asset=f'Silverbrook{NAME}',reference=dict(file=f'{NAME}-reference.png',sha256=hashlib.sha256((HERE/f'{NAME}-reference.png').read_bytes()).hexdigest(),source='docs/Game-Design/concept-art/Silverbrook/Street Props.png (cropped)'),
    blender=bpy.app.version_string,generator=f'build_{NAME.lower()}.py',method='procedural original geometry',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in size],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans and models)',unreal_mesh=f'SM_Silverbrook{NAME}',parts=[],slot_m=list(SLOT),
    placement=PLACEMENT,outputs=[f'Silverbrook{NAME}.blend',f'Silverbrook{NAME}.glb',f'Silverbrook{NAME}.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('BUILT',NAME,manifest['bounds_m'],tris)
