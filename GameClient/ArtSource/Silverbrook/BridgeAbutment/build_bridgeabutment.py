"""Silverbrook Bridge Abutment: coursed stone front wall and splayed wingwalls retaining a raised grass bank.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_bridgeabutment.py
Local frame: X runs along the river bank (across the bridge width), -Y faces the river, Z up.
Origin: centre of the top edge on the bank side (y=0, z=0 = bank / deck-end ground level).
The front wall's top (coping) is at z=0; wingwalls splay back from its ends and step down with the earth.
Built for 2-4 m banks: stone runs 4 m below the top edge (terrain hides the excess), earth fill is capped just
under z=0 so it never fights the road surface.
"""
import sys,math,json
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector
import bpy
from mathutils import Matrix
scene=kit.reset(3302)
asset=bpy.data.collections.new('SilverbrookBridgeAbutment');scene.collection.children.link(asset)
M=dict(
    dressed=kit.scan('SB_RiverAbutment_Dressed','medieval_blocks_03',dark=(.62,.74,1.0),aged=.85),
    earth=kit.scan('SB_RiverAbutment_Grass','leafy_grass',dark=(.55,.9,.4),damp=False,aged=.8))
# True-scale single-block sampling: a block L long and ~.35 m high takes a (L/2, .17) UV patch of the 2 m scan, 1:1.
LENS=(.45,.55,.65)
BLOCKS={L:kit.patches('medieval_blocks_03',(L/2,.17),dark=.6,frac=.04,std=.3) for L in LENS}
PATCH=kit.patches('medieval_blocks_03',(.3,.07),dark=.6,frac=.04,std=.3)   # coping slabs
def rect():return kit.rng.choice(PATCH)
G=dict(wall=Geo('SM_SBBridgeAbutment_Stone',asset),earth=Geo('SM_SBBridgeAbutment_Earth',asset))
W,E=G['wall'],G['earth']
# ---- layout (metres) ----------------------------------------------------------------------------
SLOT=(17,5.4,4.2)
H=4.0                     # stone depth below the top edge
FY,FT=-5.1,.7             # front face y; front wall thickness
FX=3.45                   # front wall half-length
COPE=.14                  # coping thickness
SEG,TOPS=6,[-COPE-.03-.45*i for i in range(6)]   # wingwall segments and their wall tops (coping sits on top)
S0=(FX,FY+FT/2);E0=(8.1,-.35);WT=.6          # wing centre line start/end, wing thickness
dv=(E0[0]-S0[0],E0[1]-S0[1]);LW=math.hypot(*dv);D=(dv[0]/LW,dv[1]/LW);N_IN=(-D[1],D[0])   # N_IN points to the retained earth
ALPHA=math.atan2(D[1],D[0])
# ---- course heights, shared by every wall ----------------------------------------------------------
z=-H;BOUND=[z]
while z<-COPE-.3:z+=kit.rng.uniform(.30,.40);BOUND.append(z)
BOUND=[b for b in BOUND if b<-COPE-.25]+[-COPE]
def courses(top):
    """Course (z0,z1) pairs from the base up to `top`; the last course is trimmed to land exactly on it."""
    bs=[b for b in BOUND if b<top-.2]+[top];return list(zip(bs,bs[1:]))
def blocks(geo,pt,direction,rot,length,thick,z0,z1,bond):
    """Running-bond blocks along `direction` from pt (a point on the wall's centre line), `length` long."""
    u=-bond*kit.rng.uniform(.25,.5)
    while u<length-.02:
        L0=kit.rng.choice(LENS);ln=L0;a,b=max(u,0),min(u+ln,length);u+=ln
        if b-a<.2:continue
        c=(a+b)/2;x=pt[0]+direction[0]*c;y=pt[1]+direction[1]*c;off=kit.rng.uniform(-.012,.012)
        geo.box((x,y+off if direction[1]==0 else y,(z0+z1)/2),(b-a-.014,thick,z1-z0-.014),M['dressed'],.012,rot,rect=kit.rng.choice(BLOCKS[L0]))
# ---- front wall --------------------------------------------------------------------------------------
yf=FY+FT/2
for k,(z0,z1) in enumerate(courses(-COPE)):
    blocks(W,(-FX,yf),(1,0),Matrix.Rotation(0,3,'Z'),2*FX,FT,z0,z1,k%2)
u=-FX
while u<FX-.05:
    ln=min(1.05,FX-u)+.02
    W.box((u+ln/2,yf,-COPE/2),(ln,FT+.16,COPE),M['dressed'],.012,rect=rect());u+=ln-.02
# corner piers hide the joint between front wall and wingwalls
for s in (-1,1):
    W.box((s*FX,yf,(-H-COPE)/2),(.95,.95,H-COPE),M['dressed'],.015)
# ---- wingwalls: stepped tops, coping per segment -------------------------------------------------------
LS=LW/SEG
for s in (-1,1):
    d=(s*D[0],D[1]);st=(s*S0[0],S0[1]);rot=Matrix.Rotation(math.atan2(d[1],d[0]),3,'Z')
    for i,top in enumerate(TOPS):
        p=(st[0]+d[0]*i*LS,st[1]+d[1]*i*LS)
        for k,(z0,z1) in enumerate(courses(top)):blocks(W,p,d,rot,LS,WT,z0,z1,(k+i)%2)
        c=(st[0]+d[0]*(i+.5)*LS,st[1]+d[1]*(i+.5)*LS)
        W.box((c[0],c[1],top+COPE/2),(LS+.03,WT+.16,COPE),M['dressed'],.012,rot,rect=rect())
# ---- earth fill behind the front wall, between the wings: grass ramp that falls away with the coping ----
def ft(x):
    x=abs(x)
    if x<=FX:return -.05
    pts=[(FX,-.05)]+[(FX+(S0[0]*0+D[0]*i*LS),TOPS[i]+.05) for i in range(1,SEG)]+[(E0[0],TOPS[-1]-.1)]
    for (xa,za),(xb,zb) in zip(pts,pts[1:]):
        if x<=xb:return za+(zb-za)*(x-xa)/(xb-xa)
    return pts[-1][1]
def y_in(x):
    """Inner face of the wing at |x| (the fill's front edge), or the front wall's back face."""
    x=abs(x)
    if x<=FX:return FY+FT
    t=(x-S0[0])/D[0];return max(FY+FT,S0[1]+D[1]*t+N_IN[1]*WT/2-N_IN[0]*WT/2*D[1]/D[0]*0)
XT=E0[0]-.2
xs=[-XT+2*XT*i/32 for i in range(33)]
for xa,xb in zip(xs,xs[1:]):
    pts=[(xa,y_in(xa),ft(xa)),(xb,y_in(xb),ft(xb)),(xb,0,ft(xb)),(xa,0,ft(xa))]
    # two triangles, up-facing (the quad is not planar where the front edge steps)
    for tri in ((0,1,2),(0,2,3)):
        q=[Vector(pts[i]) for i in tri]
        if (q[1]-q[0]).cross(q[2]-q[0]).z<0:q.reverse()
        E.poly(q,M['earth'])
for xa,xb in zip(xs,xs[1:]):    # buried back face at the bank edge (closes the solid)
    q=[Vector(v) for v in ((xa,0,ft(xa)),(xb,0,ft(xb)),(xb,0,-H),(xa,0,-H))]
    E.poly(q if (q[1]-q[0]).cross(q[2]-q[0]).y>0 else q[::-1],M['earth'])
# ---- checks and export --------------------------------------------------------------------------------
objs=[g.object() for g in G.values()]
WATER=-2.6   # preview waterline: 2.6 m below the top edge (2-4 m banks)
def rewet(o):
    """The kit damps stone up to 0.65 m above z=0; measure the wet band from the preview waterline (z=-2.6)."""
    me=o.data;col=me.color_attributes['Weathering']
    for poly in me.polygons:
        m=me.materials[poly.material_index]
        if m.name not in kit.DAMP:continue
        for li in poly.loop_indices:
            p=me.vertices[me.loops[li].vertex_index].co;damp=max(0,1-(p.z-WATER)/.65)*(.30+.15*math.sin(p.x*7.3+p.y*5.1))
            col.data[li].color=(min(.8,max(damp,kit.AGED.get(m.name,0))),col.data[li].color[1],0,1)
for o in objs:rewet(o)
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
assert abs(lo[0]+hi[0])<1e-6,'must be centred on X'
assert hi[1]<=1e-6 and hi[2]<=1e-6,f'nothing may rise above the top edge or past the bank edge: {hi}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs);assert tris<=40_000,f'budget {tris}'
OUT=HERE/'SilverbrookBridgeAbutment'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
# ---- preview scene (not exported): bank crest at y>=-0.3 falling to the river, water at z=-1.5 --------------
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
def preview(name,verts,faces,mat):
    me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);ob=bpy.data.objects.new(name,me);pre.objects.link(ob);me.materials.append(mat)
prof=[(-60,-3.7),(-14,-3.7),(-6.5,-3.7),(-.3,0),(60,0)]   # (y, z)
vs=[(x,y,z) for y,z in prof for x in (-40,40)]
preview('Ground',vs,[(2*i,2*i+1,2*i+3,2*i+2) for i in range(len(prof)-1)],kit.solid('Preview_Ground',(.11,.12,.07),.95))
preview('Water',[(-40,-40,WATER),(40,-40,WATER),(40,-5.5,WATER),(-40,-5.5,WATER)],[(0,1,2,3)],kit.solid('Preview_Water',(.03,.05,.06),.05))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,target,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=(Vector(target)-Vector(loc)).to_track_quat('-Z','Y').to_euler();c.lens=lens;c.clip_end=500;pre.objects.link(o)
cam('Beauty',(-12,-16,3.2),(0,-2.5,-1.8),32);cam('Front',(0,-24,-.5),(0,-3,-1.5),32);cam('Side',(-24,-3,-.5),(0,-3,-1.5),32);cam('Rear',(0,12,3.5),(0,-3,-1.5),32)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1600;scene.render.resolution_y=800;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
manifest=dict(asset='SilverbrookBridgeAbutment',reference=None,note='designed to fit Silverbrook crossings; no owner concept',context_screenshot='GameClient/ArtSource/Greenlands/Silverbrook-Crossing-InGame.png',
    blender=bpy.app.version_string,generator='build_bridgeabutment.py',method='procedural original geometry',
    units='metres, Z up, X along the bank (across the bridge), -Y faces the river, origin at the centre of the top edge on the bank side (y=0, z=0)',
    bounds_m=[round(h-l,3) for l,h in zip(lo,hi)],z_range_m=[round(lo[2],3),round(hi[2],3)],y_range_m=[round(lo[1],3),round(hi[1],3)],
    pairing=dict(bridge='SM_SilverbrookRiverBridge',front_face_y_m=FY,stone_top_z_m=0.0,earth_top_z_m=-.05,
                 origin_at_bridge_x_m=[14.0,-14.0],note='bridge-local X=+14 for the +X bank (this mesh yawed so its local +Y points to bridge +X, i.e. -90 deg about Z); X=-14 for the -X bank (+90 deg about Z). Front face then lies at bridge |x|=8.9, 0.1 m riverward of the arch springers at |x|=9. Both banks share z=0.'),
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookBridgeAbutment',slot_m=list(SLOT),
    asset_id='BLD-SilverbrookBridgeAbutment-01',outputs=['SilverbrookBridgeAbutment.blend','SilverbrookBridgeAbutment.glb','SilverbrookBridgeAbutment.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('ABUTMENT_BUILT',manifest['bounds_m'],manifest['z_range_m'],tris)
