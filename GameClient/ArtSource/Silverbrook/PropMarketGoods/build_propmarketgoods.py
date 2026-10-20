"""Silverbrook street prop: market goods cluster, barrels, crates (some open with apples and cabbages) and tied hessian sacks (designed to match Silverbrook; no owner concept).

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_propmarketgoods.py
Barrels and crates are Poly Haven CC0 models (via kit.model); sacks, ties and produce are procedural.
"""
import sys,math,json
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector
import bpy
from mathutils import Matrix
NAME='PropMarketGoods';scene=kit.reset(5103);rng=kit.rng
asset=bpy.data.collections.new('SilverbrookPropMarketGoods');scene.collection.children.link(asset)
M=dict(sack=kit.scan('SB_PropMarketGoods_Hessian','hessian_380',dark=(.42,.32,.2),damp=False,aged=.3,normal=1.6),
    tie=kit.scan('SB_PropMarketGoods_Rope','hessian_380',dark=(.30,.2,.1),damp=False,aged=.7),
    tomato=kit.solid('SB_PropMarketGoods_Tomato',(.5,.04,.02),.4),
    jug=kit.solid('SB_PropMarketGoods_Stoneware',(.22,.13,.07),.45),
    glass=kit.solid('SB_PropMarketGoods_BottleGlass',(.05,.12,.12),.15),
    apple=kit.solid('SB_PropMarketGoods_Apple',(.30,.03,.02),.35),
    cabbage=kit.solid('SB_PropMarketGoods_Cabbage',(.10,.22,.05),.6),
    turnip=kit.solid('SB_PropMarketGoods_Turnip',(.55,.42,.5),.5))
G=dict(sacks=Geo('SM_SBMarketGoods_Sacks',asset),produce=Geo('SM_SBMarketGoods_Produce',asset),jugs=Geo('SM_SBMarketGoods_Jugs',asset));SMOOTH=['sacks','produce','jugs'];EXTRA=[]
EXPECT=[(2.2,2.8),(1.2,2.0),(1.1,1.5)];BUDGET=40000
PLACEMENT=dict(footprint_m=None,front='-Y',ground_contact=True,near=['market','square','granary','townhouse','inn','road'],clearance_m=1.0)
def put(mid,x,y,z,yaw,aged=.2,keep=(0,),off=(0,0)):
    objs=kit.model(asset,mid,Matrix.Translation((x,y,z))@Matrix.Rotation(math.radians(yaw),4,'Z')@Matrix.Translation((-off[0],-off[1],0)),aged)
    for i,o in enumerate(objs):
        if i in keep:o.data.transform(o.matrix_world);o.matrix_world=Matrix();EXTRA.append(o)
        else:bpy.data.objects.remove(o,do_unlink=True)
HS=.8   # hessian metres per texture tile on the sacks (scan is .274; magnified so the weave reads from 2-3 m)
def lathe(g,mat,prof,base,rot,sx=1,sy=1,seg=14,noise=.05,sub=1):
    """prof: [(radius,z)] ascending; closed with fans at both ends; rot: 3x3 applied about the base point."""
    ph=[rng.uniform(0,6.3) for _ in range(3)];rr=lambda a,z:1+noise*math.sin(3*a+ph[0]+z*9)+noise*.6*math.sin(5*a+ph[1])+noise*.45*math.sin(7*a+ph[2]+z*14)-noise*.8*max(0,1.0-z*3)*abs(math.sin(2*a+ph[0]))
    if sub>1:prof=[p for i in range(len(prof)-1) for p in ((prof[i][0],prof[i][1]),((prof[i][0]+prof[i+1][0])/2,(prof[i][1]+prof[i+1][1])/2))]+[prof[-1]]
    rings=[[Vector(base)+rot@Vector((r*rr(math.tau*j/seg,z)*math.cos(math.tau*j/seg)*sx,r*rr(math.tau*j/seg,z)*math.sin(math.tau*j/seg)*sy,z)) for j in range(seg)] for r,z in prof]
    cb=Vector(base)+rot@Vector((0,0,prof[0][1]));ct=Vector(base)+rot@Vector((0,0,prof[-1][1]));uv=lambda a,b:[(a,b)]
    circ=lambda i:math.tau*max(prof[i][0],.01)
    for j in range(seg):
        k=(j+1)%seg
        g.poly([cb,rings[0][k],rings[0][j]],mat,[(0,0),(.5,0),(.5,.5)] if False else [(.3,.3),(.4,.3),(.3,.4)],smooth=False)
        g.poly([ct,rings[-1][j],rings[-1][k]],mat,[(.3,.3),(.4,.3),(.3,.4)],smooth=False)
        v=0
        for i in range(len(prof)-1):
            dv=math.hypot(prof[i+1][0]-prof[i][0],prof[i+1][1]-prof[i][1]);a,b=j/seg,(j+1)/seg;cu=circ(i)/HS;cu2=circ(i+1)/HS
            g.poly([rings[i][j],rings[i][k],rings[i+1][k],rings[i+1][j]],mat,[(a*cu,v/HS),(b*cu,v/HS),(b*cu2,(v+dv)/HS),(a*cu2,(v+dv)/HS)],smooth=True,w=(min(.7,max(0,.1+.16*math.sin(5*a*math.tau+ph[1]+i)+.12*math.sin(9*a*math.tau+ph[2])+(.18 if i<2 else 0))),0));v+=dv
def sack(x,y,H=.62,Rr=.21,yaw=0,tilt=(0,0),z=0):
    rot=Matrix.Rotation(math.radians(yaw),3,'Z')@Matrix.Rotation(math.radians(tilt[1]),3,'Y')@Matrix.Rotation(math.radians(tilt[0]),3,'X')
    f=[(.0,.0),(.7,.0),(.96,.06),(1,.25),(.95,.45),(.82,.62),(.55,.76),(.28,.84),(.17,.88),(.2,.95),(.34,1.0),(.3,1.06),(.1,1.1),(.0,1.1)]
    prof=[(Rr*a,H*b) for a,b in f];prof[0]=(.001,0);prof[-1]=(.001,prof[-1][1])
    lathe(G['sacks'],M['sack'],prof,(x,y,z),rot,sx=1.08,sy=.94,seg=24,noise=.11,sub=1)
    nr=Rr*.16;lathe(G['sacks'],M['tie'],[(nr+.016,H*.9),(nr+.016,H*.95)][:2]+[(nr+.016,H*.95+.001)],(x,y,z),rot,seg=8,noise=0)
def ball(c,r,mat,sy=1.0):
    cx,cy,cz=c;prof=[(r*math.sin(a),-r*sy*math.cos(a)) for a in [0,.5,1.05,1.57,2.1,2.64,3.14159]];prof[0]=(0.0001,prof[0][1]);prof[-1]=(0.0001,prof[-1][1])
    G['produce'].vessel((cx,cy,cz),prof,mat,segments=8)
def fill(cx,cy,yaw,hx,hy,z,r,mats,sy=.9,jit=.02):
    c,s_=math.cos(math.radians(yaw)),math.sin(math.radians(yaw));nx=max(1,int(2*hx/(2*r)));ny=max(1,int(2*hy/(2*r)))
    for i in range(nx):
        for j in range(ny):
            x=-hx+(i+.5)*2*hx/nx+rng.uniform(-jit,jit);y=-hy+(j+.5)*2*hy/ny+rng.uniform(-jit,jit)
            ball((cx+c*x-s_*y,cy+s_*x+c*y,z+rng.uniform(-.01,.012)),r*rng.uniform(.92,1.05),mats[(i+j)%len(mats)],sy)
def jug(x,y,h,r,mat):
    G['jugs'].vessel((x,y,0),[(.001,0),(r*.7,0),(r,h*.18),(r*1.05,h*.45),(r*.9,h*.7),(r*.45,h*.82),(r*.3,h*.95),(r*.38,h),(.001,h)],mat,segments=10)
# ---- layout (metres); crates are the models' open bodies (lids and latches dropped) ------------
put('wine_barrel_01',-.8,.4,0,0,keep=(0,1,2,3));put('wooden_barrels_01',-.1,.55,0,20,keep=(1,),off=(-.94,1.56))
put('wooden_crate_01',-.15,-.45,0,4);put('wooden_crate_02',.6,.25,0,0)
fill(-.15,-.45,4,.35,.15,.27,.055,[M['apple'],M['tomato']],sy=1)
fill(.6,.25,0,.2,.45,.34,.1,[M['cabbage']],sy=.85)
sack(-.1,.55,H=.38,Rr=.19,yaw=30,tilt=(8,0),z=.92)  # on the barrel top
sack(-1.02,-.3,H=.6,Rr=.2,yaw=40,tilt=(4,3));sack(-.72,-.62,H=.48,Rr=.19,yaw=-30)
sack(1.1,-.15,H=.62,Rr=.21,yaw=10,tilt=(-3,5));sack(1.12,.4,H=.5,Rr=.19,yaw=60)
jug(.4,-.6,.2,.06,M['jug']);jug(.56,-.62,.16,.05,M['glass'])
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
