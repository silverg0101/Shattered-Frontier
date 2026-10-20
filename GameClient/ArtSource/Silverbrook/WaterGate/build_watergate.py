"""Silverbrook town-wall water gate: a WallSection-compatible 20 m segment that carries the wall across the river.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_watergate.py
Same envelope, turrets, parapet, wall-walk, materials and end faces as ../WallSection (the curtain and turret code is
copied from build_wallsection.py). The segment origin is on the channel bed; the water surface sits 2 m above it.
Two elliptical water arches (spring 1.8 m, crown 4.0 m above the bed) run through the curtain at x=+-6.667 with splayed
reveals on both faces, dressed archivolts, and a forged-iron portcullis grate (bars 14 cm apart) filling each one.
designed to match Silverbrook Wall section; no owner concept.
"""
import sys,math,json,hashlib
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector
import bpy
from mathutils import Matrix
scene=kit.reset(2002)
LAYOUT=json.loads((HERE.parents[1]/'Greenlands/silverbrook-town-layout.json').read_text())['wall']
asset=bpy.data.collections.new('SilverbrookWaterGate');scene.collection.children.link(asset)

M=dict(
    stone=kit.scan('SB_WallSection_Masonry','rough_block_wall',dark=(.70,.74,.86),aged=.4),
    paving=kit.scan('SB_WallSection_Paving','medieval_blocks_03',dark=(.55,.58,.62),damp=False,aged=.25),
    void=kit.solid('SB_WallSection_LoopVoid',(.012,.012,.014),.95),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9))
kit.SCALE['SB_WallSection_Masonry']=20/6
STONES=kit.patches('rough_block_wall',(.06,.035),dark=.7,frac=.01,std=.25)
G=dict(stone=Geo('SM_SBWaterGate_Masonry',asset),dressed=Geo('SM_SBWaterGate_Dressed',asset),walk=Geo('SM_SBWaterGate_Walk',asset),grate=Geo('SM_SBWaterGate_Grate',asset))
# ---- layout (identical to WallSection) ------------------------------------------
L,T,H=LAYOUT['segment_length'],LAYOUT['thickness'],LAYOUT['height']
SLOT=(L,T,H);E=L/2;YF,YB=-T/2,T/2
CF=-.70;SK=.30;WALK=5.30;PAVE=.10
PF,PB=-.95,-.45
SILL,MER,COP=6.45,7.20,.10
RB=1.15
TOW=[-E,-E/3,E/3,E]
SW,SF=.70,-1.30
BW,BH=1.20,2.30
HW,HF,HT=.85,-1.44,7.60
STR=(2.25,2.40)
LOOP=(3.25,4.25)
BED,WATER=0.0,2.0                 # channel bed (piece origin) and assumed water surface above it
# ---- water arches -----------------------------------------------------------------
CX=E/3*2                          # opening centres +-6.667 (middle of the outer bays)
A0,AF=1.60,1.85                   # half-width in the barrel / at the two faces (splayed reveals)
ZS,CROWN=1.80,4.00;BB=CROWN-ZS    # spring (below the waterline), crown, ellipse semi-height
SPL=.80;YBS=YB-.05                # splay depth each side; slab back (5 cm recess leaves room for the rear archivolt)
XP=2.30                           # half-width of the arch block (pier edge)
GY=.40;PITCH=.20;BAR=.06          # grate plane, bar pitch, bar section -> clear gap 14 cm
w0=lambda:(kit.rng.uniform(.05,.3),0)

def B(geo,x0,x1,y0,y1,z0,z1,mat,bevel=.012,rect=None,w=(0,0)):
    a,b=max(x0,-E),min(x1,E)
    if b-a<.004:return
    if a!=x0 or b!=x1:bevel=0
    geo.box(((a+b)/2,(y0+y1)/2,(z0+z1)/2),(b-a,y1-y0,z1-z0),mat,bevel,rect=rect,w=w)

def frustum(geo,bot,top,z0,z1,mat):
    cl=lambda r:(max(r[0],-E),min(r[1],E),r[2],r[3])
    (a0,a1,b0,b1),(c0,c1,d0,d1)=cl(bot),cl(top)
    v=[Vector(p) for p in [(a0,b0,z0),(a1,b0,z0),(a1,b1,z0),(a0,b1,z0),(c0,d0,z1),(c1,d0,z1),(c1,d1,z1),(c0,d1,z1)]]
    faces=[[3,2,1,0],[4,5,6,7]]+[[i,(i+1)%4,(i+1)%4+4,i+4] for i in range(4)]
    vol=0
    for f in faces:
        geo.poly([v[i] for i in f],mat);vol+=sum(v[f[0]].dot(v[f[j]].cross(v[f[j+1]])) for j in range(1,len(f)-1))
    assert vol>0,'inside-out batter'

def holed(geo,x0,x1,yf,depth,z0,z1,holes,mat):
    xs=sorted({x0,x1,*[max(x0,min(x1,v)) for h in holes for v in h[:2]]})
    for a,b in zip(xs,xs[1:]):
        m=(a+b)/2;spans=sorted((h[2],h[3]) for h in holes if h[0]<m<h[1]);z=z0
        for s0,s1 in spans+[(z1,z1)]:
            if s0-z>.004:B(geo,a,b,yf,yf+depth,z,s0,mat,0)
            z=max(z,s1)
    for h in holes:B(G['dressed'],h[0]-.01,h[1]+.01,yf+depth-.02,yf+depth+.02,h[2]-.01,h[3]+.01,M['void'],0)

def cross(x,z0,z1,bar=True):
    out=[(x-.05,x+.05,z0,z1)]
    if bar:zb=z0+(z1-z0)*.38;out+=[(x-.16,x-.05,zb,zb+.08),(x+.05,x+.16,zb,zb+.08)]
    return out

def surround(x,yf,z0,z1):
    for a,b in ((x-.36,x-.19),(x+.19,x+.36)):B(G['dressed'],a,b,yf-.05,yf+.06,z0-.12,z1+.12,M['stone'],.01,kit.rng.choice(STONES),w0())
    for za,zb in ((z0-.22,z0-.06),(z1+.06,z1+.22)):B(G['dressed'],x-.4,x+.4,yf-.06,yf+.06,za,zb,M['stone'],.01,kit.rng.choice(STONES),w0())

def course(geo,x0,x1,y0,y1,z0,z1,step=.62):
    n=max(1,round((x1-x0)/step));d=(x1-x0)/n
    for i in range(n):B(geo,x0+i*d+.004,x0+(i+1)*d-.004,y0,y1,z0,z1,M['stone'],.012,kit.rng.choice(STONES),w0())
    B(geo,x0,x1,y0+.03,y1-.03,z0+.02,z1-.02,M['stone'],0)

def quad(geo,pts,want,mat):
    """Polygon wound so its normal faces `want`."""
    p=[Vector(q) for q in pts];n=sum((a.cross(p[(i+1)%len(p)]) for i,a in enumerate(p)),Vector())
    if n.dot(Vector(want))<0:p.reverse()
    geo.poly(p,mat)

def arch_block(geo,cx):
    """Masonry block [cx-XP,cx+XP] x [CF,YBS] x [0,WALK] with an elliptical water arch lofted through it, splayed at both faces."""
    N=16;secs=[(CF,AF),(CF+SPL,A0),(YBS-SPL,A0),(YBS,AF)];X0,X1=cx-XP,cx+XP
    ar=lambda a,t:(cx+a*math.cos(t),ZS+BB*math.sin(t))
    for si,(y,a) in enumerate(secs):
        if si not in (0,len(secs)-1):continue
        nrm=(0,-1,0) if si==0 else (0,1,0)
        pts=[ar(a,math.pi*i/N) for i in range(N+1)]
        for (x0,z0),(x1,z1) in zip(pts,pts[1:]):quad(geo,[(x0,y,z0),(x1,y,z1),(x1,y,WALK),(x0,y,WALK)],nrm,M['stone'])
        quad(geo,[(cx+a,y,0),(X1,y,0),(X1,y,WALK),(cx+a,y,WALK),(cx+a,y,ZS)],nrm,M['stone'])
        quad(geo,[(X0,y,0),(cx-a,y,0),(cx-a,y,ZS),(cx-a,y,WALK),(X0,y,WALK)],nrm,M['stone'])
    for (ya,aa),(yb,ab) in zip(secs,secs[1:]):
        pa=[ar(aa,math.pi*i/N) for i in range(N+1)];pb=[ar(ab,math.pi*i/N) for i in range(N+1)]
        for i in range(N):   # soffit, facing into the opening
            q=[(pa[i][0],ya,pa[i][1]),(pa[i+1][0],ya,pa[i+1][1]),(pb[i+1][0],yb,pb[i+1][1]),(pb[i][0],yb,pb[i][1])]
            mx=sum(p[0] for p in q)/4;mz=sum(p[2] for p in q)/4;quad(geo,q,(cx-mx,0,ZS-mz),M['stone'])
        for s in (-1,1):     # vertical reveals below the spring
            quad(geo,[(cx+s*aa,ya,0),(cx+s*aa,ya,ZS),(cx+s*ab,yb,ZS),(cx+s*ab,yb,0)],(-s,0,0),M['stone'])
            e0,e1=cx+s*aa,cx+s*XP;f0,f1=(cx+s*ab,cx+s*XP)
            quad(geo,[(e0,ya,0),(e1,ya,0),(f1,yb,0),(f0,yb,0)],(0,0,-1),M['stone'])   # pier bottom
        quad(geo,[(X0,ya,WALK),(X1,ya,WALK),(X1,yb,WALK),(X0,yb,WALK)],(0,0,1),M['stone'])

def archivolt(cx,yc,th,n=11,depth=.30):
    """Dressed voussoir ring around the face opening (half-width AF), plus quoin courses down the jambs."""
    P=lambda t:Vector((cx+AF*math.cos(t),0,ZS+BB*math.sin(t)))
    N_=lambda t:Vector((BB*math.cos(t),0,AF*math.sin(t))).normalized()
    for k in range(n):
        t0,t1=math.pi*k/n,math.pi*(k+1)/n;tm=(t0+t1)/2
        c0=P(t0)+N_(t0)*depth/2;c1=P(t1)+N_(t1)*depth/2;tang=(c0-c1).normalized();up=Vector((-tang.z,0,tang.x))
        if up.z<0:up=-up
        ctr=P(tm)+N_(tm)*depth/2;ctr.y=yc
        G['dressed'].box(ctr,((c1-c0).length-.014,th,depth),M['stone'],.012,Matrix((tang,Vector((0,1,0)),up)).transposed(),rect=kit.rng.choice(STONES),w=w0())
    hgt=ZS/4
    for i in range(4):
        qw=.30 if i%2 else .44
        for s in (-1,1):G['dressed'].box((cx+s*(AF+qw/2),yc,hgt*(i+.5)),(qw,th,hgt-.012),M['stone'],.012,rect=kit.rng.choice(STONES),w=w0())

# ---- curtain core: solid ends and middle bay, water arches between ------------------
XA=CX-XP                                  # inner edge of the arch blocks (|x|)
for x0,x1,hl in ((-XA,XA,True),(XA+2*XP,E,False),(-E,-XA-2*XP,False)):
    B(G['stone'],x0,x1,CF+SK,YB,0,WALK,M['stone'],0)
    holes=[]
    if hl:
        for x in (-1.6,0,1.6):holes+=cross(x,*LOOP);surround(x,CF,*LOOP)
    holed(G['stone'],x0,x1,CF,SK,0,WALK,holes,M['stone'])
for s in (-1,1):
    arch_block(G['stone'],s*CX)
    archivolt(s*CX,CF+.005,.11);archivolt(s*CX,YB-.03,.06)
bays=[(a+SW,b-SW) for a,b in zip(TOW,TOW[1:])]
course(G['dressed'],*bays[1],CF-.08,CF+.06,*STR)
# corbel table under the parapet
for a,b in bays:
    for k in range(4):
        x=a+(b-a)*(k+.5)/4
        B(G['dressed'],x-.15,x+.15,PF-.03,CF+.05,4.85,WALK,M['stone'],.01,kit.rng.choice(STONES),w0())
        B(G['dressed'],x-.12,x+.12,PF+.12,CF+.05,4.45,4.85,M['stone'],.01,kit.rng.choice(STONES),w0())
# ---- parapet with merlons (unchanged) ------------------------------------------------
for t0,t1 in zip(TOW,TOW[1:]):
    a,b=t0+HW,t1-HW
    course(G['dressed'],a,b,PF-.05,PB,WALK,WALK+.15)
    B(G['stone'],a,b,PF,PB,WALK+.15,SILL,M['stone'],0)
    course(G['dressed'],a,b,PF-.04,PB+.04,SILL,SILL+COP,.55)
    n=5;g=.5;m=(b-a-(n-1)*g)/n
    for k in range(n):
        x=a+k*(m+g)
        B(G['stone'],x,x+m,PF,PB,SILL+COP,MER,M['stone'],0)
        course(G['dressed'],x-.04,x+m+.04,PF-.05,PB+.05,MER,MER+COP,m+.1)
B(G['stone'],-E,E,RB,YB,WALK,WALK+.85,M['stone'],0)
course(G['dressed'],-E,E,RB-.04,YB,WALK+.85,WALK+.95)
B(G['walk'],-E,E,PB,RB,WALK,WALK+PAVE,M['paving'],0)
# ---- turrets (unchanged) -------------------------------------------------------------
for c in TOW:
    frustum(G['stone'],(c-BW,c+BW,YF,CF+.1),(c-SW,c+SW,SF,CF+.1),0,BH,M['stone'])
    holed(G['stone'],c-SW,c+SW,SF,.25,BH,WALK,cross(c,*LOOP),M['stone']);surround(c,SF,*LOOP)
    B(G['stone'],c-SW,c+SW,SF+.25,CF+.1,BH,WALK,M['stone'],0)
    B(G['dressed'],c-SW-.08,c+SW+.08,SF-.08,CF+.05,*STR,M['stone'],.012,kit.rng.choice(STONES),w0())
    for x in (c-.42,c+.42):B(G['dressed'],x-.13,x+.13,HF+.02,SF+.05,4.80,WALK,M['stone'],.01,kit.rng.choice(STONES),w0())
    for s in (-1,1):B(G['dressed'],c+s*SW-.13 if s<0 else c+SW-.05,c-SW+.05 if s<0 else c+SW+.13,-1.12,-.88,4.80,WALK,M['stone'],.01,kit.rng.choice(STONES),w0())
    course(G['dressed'],c-HW-.05,c+HW+.05,YF,PB,WALK,WALK+.15,.6)
    holed(G['stone'],c-HW,c+HW,HF,.25,WALK+.15,HT,cross(c,6.55,7.25,bar=False),M['stone']);surround(c,HF,6.55,7.25)
    B(G['stone'],c-HW,c+HW,HF+.25,PB,WALK+.15,HT,M['stone'],0)
    for x0,x1 in ((c-HW-.03,c-HW+.45),(c+HW-.45,c+HW+.03)):
        for y0,y1 in ((HF-.03,HF+.40),(PB-.40,PB+.03)):
            B(G['stone'],x0,x1,y0,y1,HT,H-COP,M['stone'],0)
            B(G['dressed'],x0-.02,x1+.02,y0-.02 if y0<-1 else y0,y1+.02 if y1>-.5 else y1,H-COP,H,M['stone'],.01,kit.rng.choice(STONES),w0())
# ---- portcullis grates: vertical bars to the soffit, flat rails, all embedded in the masonry -----------
def soffit(dx):return ZS+BB*math.sqrt(max(0,1-(dx/A0)**2))
for s in (-1,1):
    cx=s*CX;gg=G['grate'];K=int(A0/PITCH-.5)       # bars at k*PITCH, k=-7..7
    for k in range(-K,K+1):
        top=soffit(k*PITCH)+.10
        gg.box((cx+k*PITCH,GY,top/2),(BAR,BAR,top),M['iron'],0)
    for z in (.15,.85,1.55,2.25,2.95,3.55):
        half=(A0 if z<=ZS else A0*math.sqrt(max(0,1-((z-ZS)/BB)**2)))+.10
        gg.box((cx,GY-.04,z),(2*half,.05,.08),M['iron'],0)
# ---- checks, export, preview scene, manifest ----------------------------------------
objs=[g.object() for g in G.values()]
pts=[o.matrix_world@v.co for o in objs for v in o.data.vertices]
lo=[min(p[k] for p in pts) for k in range(3)];hi=[max(p[k] for p in pts) for k in range(3)]
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)) and abs(lo[0]+E)<1e-4 and abs(hi[0]-E)<1e-4 and lo[2]>-1e-4,f'slot {SLOT}: {lo} {hi}'
end=lambda s:sorted({(round(p.y,4),round(p.z,4)) for p in pts if abs(p.x-s*E)<1e-4})
assert end(-1)==end(1) and len(end(1))>20,'end faces differ'
walk_tops=[p for p in G['walk'].v if abs(p.z-WALK-PAVE)<1e-4];assert min(p.x for p in walk_tops)==-E and max(p.x for p in walk_tops)==E,'walk not continuous'
assert PITCH-BAR<=.25 and A0-int(A0/PITCH-.5)*PITCH-BAR/2<=.25
assert BED+WATER<ZS+0.3 and 3.5<=CROWN<=4.5,'arch heights'
OUT=HERE/'SilverbrookWaterGate'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
for o in objs:
    for dx in (-L,L):n=o.copy();n.location.x=dx;pre.objects.link(n)
def plane(name,z,mat,s=60):
    me=bpy.data.meshes.new(name);me.from_pydata([(-s,-s,z),(s,-s,z),(s,s,z),(-s,s,z)],[],[(0,1,2,3)]);o=bpy.data.objects.new(name,me);pre.objects.link(o);o.data.materials.append(mat)
plane('Ground',BED,kit.solid('Preview_Ground',(.11,.12,.07),.95))
wm=bpy.data.materials.new('Preview_Water');wm.use_nodes=True;wp=wm.node_tree.nodes['Principled BSDF']
wp.inputs['Base Color'].default_value=(.03,.09,.10,1);wp.inputs['Roughness'].default_value=.04;wp.inputs['Alpha'].default_value=.6
wm.surface_render_method='BLENDED'
plane('Water',BED+WATER,wm)
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-9,-16,4.5),(84,0,-30),lens=26);cam('Front',(0,-40,4),(90,0,0),21);cam('Side',(-40,0,4),(90,0,-90),9);cam('Rear',(0,40,4),(90,0,180),21);cam('Walk',(-13,6,10),(62,0,-118),lens=28)
cam('Through',(CX-1.2,-7,2.45),(88,0,8),lens=24)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs);assert tris<=60000,tris
manifest=dict(asset='SilverbrookWaterGate',reference=None,note='designed to match Silverbrook Wall section; no owner concept',
    blender=bpy.app.version_string,generator='build_watergate.py',method='procedural original geometry matching ../WallSection',
    units='metres, Z up, front -Y (outside of the wall), origin ground (channel bed) centre',bounds_m=[round(h-l,3) for l,h in zip(lo,hi)],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookWaterGate',slot_m=list(SLOT),
    tiling=dict(axis='X',segment_length_m=L,end_faces='identical sections at x=+-%g, same as WallSection'%E,walk_top_z=WALK+PAVE,walk_y=[PB,RB],masonry_uv_repeat_m=round(20/6,4)),
    water=dict(bed_z=BED,assumed_water_surface_z=BED+WATER,openings=[dict(centre_x=s*CX,width_barrel_m=2*A0,width_face_m=2*AF,spring_z=ZS,crown_z=CROWN) for s in (-1,1)],
        grate=dict(mesh='SM_SBWaterGate_Grate',plane_y=GY,bar_pitch_m=PITCH,bar_section_m=BAR,max_clear_gap_m=round(PITCH-BAR,3),collision='complex as simple (solid bars in the main export)')),
    outputs=['SilverbrookWaterGate.blend','SilverbrookWaterGate.glb','SilverbrookWaterGate.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('WATERGATE_BUILT',manifest['bounds_m'],tris)
