"""Silverbrook wall tower from docs/Game-Design/concept-art/Silverbrook/Wall Tower.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_walltower.py
Tower in the town wall, 8 x 8 x 12 m (`wall.tower` in Greenlands/silverbrook-town-layout.json).
Chamfered-square (octagonal) plan on a battered plinth, slightly tapering shaft with quoined
corners and round-headed loops, a corbelled string course at wall-walk height, and an
overhanging crenellated head on stone and oak corbels with tall corner merlons.
The wall runs along X through the tower. Each side carries a wall joint: the half wall-turret,
curtain, wall-walk and rear parapet of ../WallSection, built with that package's own
dimensions and UV phase, so a WallSection segment butts flush at x=+-4 (its end section is
asserted vertex-for-vertex against SilverbrookWallSection.blend). The walk runs on to a plank
door in each side face.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,UP
import bpy
from mathutils import Matrix
scene=kit.reset(2101)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Wall Tower.png'
WALLSEC=HERE.parent/'WallSection/SilverbrookWallSection.blend'
LAYOUT=json.loads((HERE.parents[1]/'Greenlands/silverbrook-town-layout.json').read_text())['wall']
asset=bpy.data.collections.new('SilverbrookWallTower');scene.collection.children.link(asset)

M=dict(  # the first three are WallSection's materials with identical arguments, so the masonry matches across the joint
    stone=kit.scan('SB_WallSection_Masonry','rough_block_wall',dark=(.70,.74,.86),aged=.4),
    paving=kit.scan('SB_WallSection_Paving','medieval_blocks_03',dark=(.55,.58,.62),damp=False,aged=.25),
    void=kit.solid('SB_WallSection_LoopVoid',(.012,.012,.014),.95),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    banner=kit.scan('SB_WallTower_Banner','denim_fabric_03',dark=(.12,.14,.30),damp=False,aged=.75),
    gilt=kit.solid('SB_WallTower_Gilt',(.80,.55,.18),.35,1.0))
kit.SCALE['SB_WallSection_Masonry']=20/6    # WallSection's block size and UV repeat
S=kit.SCALE['SB_WallSection_Masonry']
STONES=kit.patches('rough_block_wall',(.06,.035),dark=.7,frac=.01,std=.25)
BANDS=kit.bands('weathered_brown_planks')
def rect():return kit.rng.choice(STONES)
def ruv(r):u,v,du,dv=r;return [(u,v),(u+du,v),(u+du,v+dv),(u,v+dv)]
def w0():return (kit.rng.uniform(.05,.3),0)
G=dict(stone=Geo('SM_SBWallTower_Masonry',asset),dressed=Geo('SM_SBWallTower_Dressed',asset),walk=Geo('SM_SBWallTower_Walk',asset),detail=Geo('SM_SBWallTower_Details',asset))

# ---- WallSection dimensions (copied from ../WallSection/build_wallsection.py; the joint is asserted against it) ----
L,T,H=LAYOUT['segment_length'],LAYOUT['thickness'],LAYOUT['height']
SLOT=tuple(LAYOUT['tower']);J=SLOT[0]/2       # 8 x 8 x 12; wall segments butt at x=+-J
E=L/2;YF,YB=-T/2,T/2
CF,SK,WALK,PAVE=-.70,.30,5.30,.10
PF,PB,RB,COP=-.95,-.45,1.15,.10
SW,SF,BW,BH=.70,-1.30,1.20,2.30
HW,HF,HT=.85,-1.44,7.60
STR=(2.25,2.40);LOOP=(3.25,4.25)
WIN=(-E,E)
def B(geo,x0,x1,y0,y1,z0,z1,mat,bevel=.012,rect=None,w=(0,0)):
    a,b=max(x0,WIN[0]),min(x1,WIN[1])
    if b-a<.004:return
    if a!=x0 or b!=x1:bevel=0
    geo.box(((a+b)/2,(y0+y1)/2,(z0+z1)/2),(b-a,y1-y0,z1-z0),mat,bevel,rect=rect,w=w)
def frustum(geo,bot,top,z0,z1,mat):
    cl=lambda r:(max(r[0],WIN[0]),min(r[1],WIN[1]),r[2],r[3])
    (a0,a1,b0,b1),(c0,c1,d0,d1)=cl(bot),cl(top)
    v=[Vector(p) for p in [(a0,b0,z0),(a1,b0,z0),(a1,b1,z0),(a0,b1,z0),(c0,d0,z1),(c1,d0,z1),(c1,d1,z1),(c0,d1,z1)]]
    for f in [[3,2,1,0],[4,5,6,7]]+[[i,(i+1)%4,(i+1)%4+4,i+4] for i in range(4)]:geo.poly([v[i] for i in f],mat)
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
    for a,b in ((x-.36,x-.19),(x+.19,x+.36)):B(G['dressed'],a,b,yf-.05,yf+.06,z0-.12,z1+.12,M['stone'],.01,rect(),w0())
    for za,zb in ((z0-.22,z0-.06),(z1+.06,z1+.22)):B(G['dressed'],x-.4,x+.4,yf-.06,yf+.06,za,zb,M['stone'],.01,rect(),w0())
def course(geo,x0,x1,y0,y1,z0,z1,step=.62):
    n=max(1,round((x1-x0)/step));d=(x1-x0)/n
    for i in range(n):B(geo,x0+i*d+.004,x0+(i+1)*d-.004,y0,y1,z0,z1,M['stone'],.012,rect(),w0())
    B(geo,x0,x1,y0+.03,y1-.03,z0+.02,z1-.02,M['stone'],0)

def joint(s):
    """WallSection's end of segment (curtain, walk, rear parapet, end half-turret) for the last 1.1 m before
    x=s*E, built in segment coordinates (so the world-projected masonry keeps the segment's UV phase) and then
    moved to x=s*J. Its inner end is buried in the tower body. Returns the vertex ranges it added."""
    global WIN;c=s*E;WIN=(E-1.1,E) if s>0 else (-E,-E+1.1);start={k:len(g.v) for k,g in G.items()}
    B(G['stone'],-E,E,CF+SK,YB,0,WALK,M['stone'],0)
    holed(G['stone'],-E,E,CF,SK,0,WALK,[],M['stone'])
    B(G['stone'],-E,E,RB,YB,WALK,WALK+.85,M['stone'],0)
    course(G['dressed'],-E,E,RB-.04,YB,WALK+.85,WALK+.95)
    B(G['walk'],-E,E,PB,RB,WALK,WALK+PAVE,M['paving'],0)
    # the turret code of WallSection, verbatim
    frustum(G['stone'],(c-BW,c+BW,YF,CF+.1),(c-SW,c+SW,SF,CF+.1),0,BH,M['stone'])
    holed(G['stone'],c-SW,c+SW,SF,.25,BH,WALK,cross(c,*LOOP),M['stone']);surround(c,SF,*LOOP)
    B(G['stone'],c-SW,c+SW,SF+.25,CF+.1,BH,WALK,M['stone'],0)
    B(G['dressed'],c-SW-.08,c+SW+.08,SF-.08,CF+.05,*STR,M['stone'],.012,rect(),w0())
    for x in (c-.42,c+.42):B(G['dressed'],x-.13,x+.13,HF+.02,SF+.05,4.80,WALK,M['stone'],.01,rect(),w0())
    for t in (-1,1):B(G['dressed'],c+t*SW-.13 if t<0 else c+SW-.05,c-SW+.05 if t<0 else c+SW+.13,-1.12,-.88,4.80,WALK,M['stone'],.01,rect(),w0())
    course(G['dressed'],c-HW-.05,c+HW+.05,YF,PB,WALK,WALK+.15,.6)
    holed(G['stone'],c-HW,c+HW,HF,.25,WALK+.15,HT,cross(c,6.55,7.25,bar=False),M['stone']);surround(c,HF,6.55,7.25)
    B(G['stone'],c-HW,c+HW,HF+.25,PB,WALK+.15,HT,M['stone'],0)
    for x0,x1 in ((c-HW-.03,c-HW+.45),(c+HW-.45,c+HW+.03)):
        for y0,y1 in ((HF-.03,HF+.40),(PB-.40,PB+.03)):
            B(G['stone'],x0,x1,y0,y1,HT,H-COP,M['stone'],0)
            B(G['dressed'],x0-.02,x1+.02,y0-.02 if y0<-1 else y0,y1+.02 if y1>-.5 else y1,H-COP,H,M['stone'],.01,rect(),w0())
    shift=Vector((s*(E-J),0,0))
    for k,g in G.items():g.v[start[k]:]=[p-shift for p in g.v[start[k]:]]
    return {k:(start[k],len(g.v)) for k,g in G.items()}
JOINTS={s:joint(s) for s in (1,-1)}

# ---- tower layout (metres; concept read at ~84 px/m, 8 m across the plinth, 12 m to the corner merlons) ----
TAN=math.tan(math.pi/8)
H0,F0=3.42,1.70                # shaft at its foot: half-width across flats, half-width of each face
Z0,ZT,TAPER=2.30,9.40,.24      # shaft foot, head base; the shaft draws in 0.24 m per side
ZM=(5.28,5.46)                 # string course, at the wall-walk
ZC=8.55                        # foot of the corbel table under the head
DH=-TAPER+.60                  # head 0.6 m proud of the shaft top
SILL,MER,CMER=10.80,11.45,11.90  # crenel sill, merlon top, corner merlon top (caps add 0.1)
FRONT,REAR,SIDES,DIAG=7,3,(1,5),(0,2,4,6)
def ds(z):return -TAPER*(z-Z0)/(ZT-Z0)
def octo(d,z=0):
    """Plan octagon offset d from the shaft foot, CCW from the front-right corner. Face k runs from vertex k to k+1:
    7 front (-Y), 1 and 5 sides (+-X), 3 rear, the even ones the chamfers."""
    h,f=H0+d,F0+d*TAN;return [Vector((x,y,z)) for x,y in ((f,-h),(h,-f),(h,f),(f,h),(-f,h),(-h,f),(-h,-f),(-f,-h))]
def nrm(k):e=octo(0)[(k+1)%8]-octo(0)[k];return Vector((e.y,-e.x,0)).normalized()
def closed(geo,polys,mat,uvs=None,w=(0,0)):
    vol=0
    for i,q in enumerate(polys):
        geo.poly(q,mat,uvs[i] if uvs else None,w);vol+=sum(q[0].dot(q[j].cross(q[j+1])) for j in range(1,len(q)-1))
    assert vol>0,'inside-out solid'
def loft(geo,d0,z0,d1,z1,mat):
    """Closed octagonal frustum; side UVs run along each face so chamfer stones keep their size."""
    a,b=octo(d0,z0),octo(d1,z1);s=kit.SCALE[mat.name];polys=[];uvs=[]
    for k in range(8):
        j=(k+1)%8;q=[a[k],a[j],b[j],b[k]];t=(a[j]-a[k]).normalized();polys.append(q);uvs.append([(p.dot(t)/s,p.z/s) for p in q])
    closed(geo,polys+[b,a[::-1]],mat,uvs+[None,None])
def ring(geo,do,di,z0,z1,mat):
    o0,o1,i0,i1=octo(do,z0),octo(do,z1),octo(di,z0),octo(di,z1);s=kit.SCALE[mat.name];polys=[];uvs=[]
    for k in range(8):
        j=(k+1)%8;t=(o0[j]-o0[k]).normalized()
        for q in ([o0[k],o0[j],o1[j],o1[k]],[i0[j],i0[k],i1[k],i1[j]]):polys.append(q);uvs.append([(p.dot(t)/s,p.z/s) for p in q])
        polys+=[[o1[k],o1[j],i1[j],i1[k]],[o0[j],o0[k],i0[k],i0[j]]];uvs+=[None,None]
    closed(geo,polys,mat,uvs)
def blocks(z0,z1,do,di,faces=range(8),step=.6):
    """Dressed course between offsets do (out) and di (in), mitred at the corners, one scanned stone per block,
    8 mm joints over a recessed backing."""
    zm=(z0+z1)/2;O,I,Ob=octo(do,zm),octo(di,zm),octo(do-.03,zm)
    for k in faces:
        j=(k+1)%8;n=max(1,round((O[j]-O[k]).length/step));g=.004/(O[j]-O[k]).length
        for m in range(n):
            t0,t1=m/n+g,(m+1)/n-g
            G['dressed'].prism([O[k].lerp(O[j],t0),O[k].lerp(O[j],t1),I[k].lerp(I[j],t1),I[k].lerp(I[j],t0)],UP,z1-z0,M['stone'],uv=ruv(rect()),w=w0())
        G['dressed'].prism([Ob[k],Ob[j],I[j],I[k]],UP,z1-z0-.04,M['stone'])
def fframe(k,z0,dfun=ds):
    """Frame on face k at height z0, lying in the (battered) face: P(a along face, d outward, z up the face)."""
    a0,a1,b0=octo(dfun(z0),z0)[k],octo(dfun(z0),z0)[(k+1)%8],octo(dfun(z0+1),z0+1)[k]
    r=(a1-a0).normalized();n=r.cross(b0-a0).normalized();u=n.cross(r);O=(a0+a1)/2
    return (lambda a,d,z:O+r*a+n*d+u*z),Matrix((r,n,u)).transposed(),n
def arc(n):return [math.pi*i/n for i in range(n+1)]

# ---- plinth, shaft, string course ---------------------------------------------------------------------------------
loft(G['stone'],.58,0,.06,2.12,M['stone'])                     # battered plinth: 8 m across at the ground
blocks(2.08,2.28,ds(2.28)+.13,ds(2.28)-.30)                     # plinth cap
loft(G['stone'],ds(2.25),2.25,ds(ZT+.05),ZT+.05,M['stone'])     # shaft
blocks(*ZM,ds(5.37)+.10,ds(5.37)-.30,[0,2,3,4,6,7])             # string course (the wall joints take the sides)
for k in (FRONT,REAR,*DIAG):                                    # small corbels under it
    P,R,_=fframe(k,4.95)
    for a in ((-1.0,0,1.0) if k in (FRONT,REAR) else (0,)):G['dressed'].box(P(a,.03,.165),(.2,.26,.33),M['stone'],.01,R,rect=rect(),w=w0())
# quoins on the four corners either side of the front and rear faces
for k in (0,7,3,4):
    flat_first=(k-1)%8 in (FRONT,REAR)
    for z0,z1 in ((2.28,ZM[0]),(ZM[1],ZC-.03)):
        n=round((z1-z0)/.42);hc=(z1-z0)/n
        for m in range(n):
            zc=z0+(m+.5)*hc;O,I=octo(ds(zc)+.03,zc),octo(ds(zc)-.2,zc)
            ta,tb=(O[k]-O[k-1]).normalized(),(O[(k+1)%8]-O[k]).normalized()
            flat,diag=((.32,.28),(.2,.46))[m%2];la,lb=(flat,diag) if flat_first else (diag,flat)
            for q in ([O[k]-ta*la,O[k],I[k],I[k]-ta*la],[O[k],O[k]+tb*lb,I[k]+tb*lb,I[k]]):G['dressed'].prism(q,UP,hc-.012,M['stone'],uv=ruv(rect()),w=w0())

# ---- loops: round-headed slits in dressed surrounds (dark void plate, jambs 0.1 m proud) -------------------------
def slit(k,a,z0,z1,w=.16):
    P,R,n=fframe(k,z0);h=z1-z0;rad=w/2
    G['dressed'].prism([P(a-rad,.012,0),P(a+rad,.012,0)]+[P(a+rad*math.cos(t),.012,h-rad+rad*math.sin(t)) for t in arc(8)],n,.008,M['void'])
    for side in (-1,1):G['dressed'].box(P(a+side*(rad+.075),0,h/2),(.15,.2,h),M['stone'],.01,R,rect=rect(),w=w0())
    G['dressed'].box(P(a,0,h+.11),(w+.36,.22,.22),M['stone'],.01,R,rect=rect(),w=w0())
    G['dressed'].box(P(a,.01,-.08),(w+.44,.24,.16),M['stone'],.01,R,rect=rect(),w=w0())
for k in (FRONT,REAR):
    slit(k,0,2.65,3.95);slit(k,0,6.95,8.35)
    for a in (-.95,.95):slit(k,a,6.3,7.6)
for k in DIAG:slit(k,0,2.65,3.85)

# ---- doors where the wall-walk meets the side faces -------------------------------------------------------------
def door(k,a):
    P,R,n=fframe(k,WALK+PAVE);w,sp=1.0,1.5;rad=w/2
    D=Geo(f'SM_SBWallTower_Door{len(kit.DOORS)+1}',asset);G[D.name]=D;kit.DOORS.append(dict(geo=D,r=R.col[0].normalized(),o=Vector((n.x,n.y,0)).normalized()))
    D.prism([P(a-rad,.02,-.01),P(a+rad,.02,-.01)]+[P(a+rad*math.cos(t),.02,sp+rad*math.sin(t)) for t in arc(16)],n,.06,M['door'])
    for z in (.35,sp-.1):D.box(P(a-rad*.15,.057,z),(w*.8,.012,.05),M['iron'],.004,R)
    for i in range(10):t=math.tau*i/10;D.box(P(a+rad*.55+.06*math.cos(t),.06,1.0+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
    D.box(P(a+rad*.55,.055,1.06),(.07,.01,.07),M['iron'],.004,R)
    z=-.02;i=0
    while z<sp-.05:
        hgt=min(.31,sp-z);qw=.2 if i%2 else .28
        for side in (-1,1):G['dressed'].box(P(a+side*(rad-.012+qw/2),.01,z+hgt/2),(qw,.22,hgt-.012),M['stone'],.012,R,rect=rect(),w=w0())
        z+=hgt;i+=1
    for i in range(9):
        t=math.pi*(i+.5)/9;rm=rad-.012+.14;radial=R@Vector((math.cos(t),0,math.sin(t)));tang=R@Vector((-math.sin(t),0,math.cos(t)))
        G['dressed'].box(P(a+rm*math.cos(t),.01,sp+rm*math.sin(t)),(math.pi*rm/9-.014,.22,.28),M['stone'],.012,Matrix((tang,n,radial)).transposed(),rect=rect(),w=w0())
DOOR_Y=(PB+RB)/2               # centred on the wall-walk
door(1,DOOR_Y);door(5,-DOOR_Y)

# ---- corbel table and head --------------------------------------------------------------------------------------
for k in range(8):                                              # two-stage stone corbels on every corner
    p=octo(ds(ZC),ZC)[k];P,R=kit.frame(p,(nrm(k-1)+nrm(k)))
    G['dressed'].box(P(0,.12,.2),(.34,.48,.4),M['stone'],.012,R,rect=rect(),w=w0())
    G['dressed'].box(P(0,.22,.63),(.38,.68,.45),M['stone'],.012,R,rect=rect(),w=w0())
for k in range(8):                                              # oak brackets between them, iron-strapped
    P,R,_=fframe(k,ZC)
    for a in ((-1.0,0,1.0) if k not in DIAG else (0,)):
        b=kit.rng.choice(BANDS)
        G['detail'].box(P(a,.21,.68),(.22,.62,.38),M['timber'],.012,R,grain=True,band=b)
        G['detail'].box(P(a,.09,.25),(.2,.38,.5),M['timber'],.012,R,grain=True,band=b)
        G['detail'].box(P(a,.285,.25),(.12,.012,.36),M['iron'],.004,R)
blocks(ZT,9.75,DH+.06,ds(ZT)-.30,step=.7)                          # overhanging head course
loft(G['stone'],ds(ZT)-.25,9.42,ds(ZT)-.25,9.73,M['stone'])    # its hidden core
ring(G['stone'],DH,DH-.5,9.75,SILL,M['stone'])                 # parapet
blocks(SILL,SILL+COP,DH+.04,DH-.54)                            # crenel sills
loft(G['walk'],DH-.45,9.75,DH-.45,9.85,M['paving'])            # roof flags, into the parapet foot
for k in range(8):                                              # merlons: three per flat face, a tall one on each chamfer
    P,R,_=fframe(k,0,lambda z:DH)
    for a,w,top in ([(0,1.5,CMER)] if k in DIAG else [(-1.08,.62,MER),(0,.62,MER),(1.08,.62,MER)]):
        G['stone'].box(P(a,-.25,(SILL+COP+top)/2),(w,.5,top-SILL-COP),M['stone'],0,R)
        G['dressed'].box(P(a,-.25,top+COP/2),(w+.06,.56,COP),M['stone'],.012,R,rect=rect(),w=w0())

# ---- stag banners on iron arms either side of the front (copied from the Watchtower's banner) ---------------------
def stag(origin,right,up,normal,s):
    o=Vector(origin)+Vector(normal)*.008;R_,U,N=Vector(right),Vector(up),Vector(normal);geo=G['detail']
    def p(a,z):return o+R_*(a*s)+U*(z*s)
    geo.prism([p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,M['gilt'])
    for sx in (-1,1):
        geo.prism([p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
def trim(pts,normal,w=.04):
    n=Vector(normal)*.008
    for a,b in zip(pts,pts[1:]+pts[:1]):G['detail'].beam(Vector(a)+n,Vector(b)+n,.02,w,M['gilt'],grain=False)
AY,AZ,BX=-1.9,8.35,3.6
for s in (-1,1):
    o=octo(ds(AZ));xf=o[0].x-o[0].y+AY              # chamfer face: x - y = h + f
    G['detail'].beam((s*(xf-.06),AY,AZ),(s*3.97,AY,AZ),.05,.05,M['iron'],grain=False)
    G['detail'].beam((s*(xf-.04),AY,AZ-.6),(s*3.45,AY,AZ-.03),.035,.035,M['iron'],grain=False)
    xc=s*BX;outline=[(xc-.32,8.15),(xc+.32,8.15),(xc+.32,6.5),(xc,6.15),(xc-.32,6.5)]
    G['detail'].prism([(x,AY,z) for x,z in outline],(0,1,0),.02,M['banner'])
    G['detail'].beam((xc-.36,AY,8.18),(xc+.36,AY,8.18),.03,.03,M['iron'],grain=False)
    for dx in (-.27,.27):G['detail'].beam((xc+dx,AY,8.17),(xc+dx,AY,AZ-.02),.02,.02,M['iron'],grain=False)
    for n in (-1,1):
        trim([(x,AY+n*.01,z) for x,z in outline],(0,n,0))
        stag((xc,AY+n*.01,7.4),(-n,0,0),(0,0,1),(0,n,0),.55)

# ---- checks -------------------------------------------------------------------------------------------------------
objs=[g.object() for g in G.values()]
pts=[v for g in G.values() for v in g.v]
lo=[min(p[k] for p in pts) for k in range(3)];hi=[max(p[k] for p in pts) for k in range(3)]
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)) and all(abs(l+h)<1e-3 for l,h in zip(lo[:2],hi[:2])) and abs(lo[2])<1e-4,f'slot {SLOT}: {lo} {hi}'
def end_set(points,x):return sorted({(round(p.y,3),round(p.z,3)) for p in points if abs(p.x-x)<1e-4})
with bpy.data.libraries.load(str(WALLSEC)) as (src,dst):dst.meshes=[n for n in src.meshes if n.startswith('SM_SBWallSection')]
wall_end=end_set([v.co for me in dst.meshes for v in me.vertices],E)
for s,rng_ in JOINTS.items():   # the joint's outer face is exactly WallSection's end section, so segments butt flush
    mine=end_set([p for k,(a,b) in rng_.items() for p in G[k].v[a:b]],s*J);assert mine==wall_end and len(mine)>20,f'joint {s} differs from WallSection end'
walk=[p for a,b in [JOINTS[s]['walk'] for s in (1,-1)] for p in G['walk'].v[a:b] if abs(p.z-WALK-PAVE)<1e-4]
assert max(p.x for p in walk)==J and min(p.x for p in walk)==-J and min(abs(p.x) for p in walk)<octo(ds(WALK))[1].x-.3,'wall-walk does not reach the tower doors'
OUT=HERE/'SilverbrookWallTower'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
PARTS=kit.split_doors(str(OUT),'SM_SilverbrookWallTower')
# Door leaves: own meshes hinged on the -r jamb, opening 90 deg inward; check the swing ends inside the building.
for d,row in zip(kit.DOORS,PARTS):
    ob_=bpy.data.objects[d['geo'].name];pv=Vector(row['pivot_m']);rz=Matrix.Rotation(math.radians(row['travel']),3,'Z');o_h=Vector((d['o'].x,d['o'].y,0)).normalized()
    vs=[pv+rz@(ob_.matrix_world@v.co-pv) for v in ob_.data.vertices];c_=sum(vs,Vector())/len(vs)
    assert (c_-pv).dot(o_h)<-.3 and max((v-pv).dot(o_h) for v in vs)<.02,f"{row['name']} does not swing inward"
    print('DOOR_SWING',row['name'],row['pivot_m'],row['travel'],'open centre inward %.2f m'%-(c_-pv).dot(o_h))
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
# ---- preview: a WallSection segment butted on each side ------------------------------------------------------------
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
for me in dst.meshes:
    for i,m in enumerate(me.materials):me.materials[i]=kit.MATS[m.name.rsplit('.',1)[0]]
    for s in (-1,1):n=bpy.data.objects.new(me.name+('_L' if s<0 else '_R'),me);n.location.x=s*(J+E);pre.objects.link(n)
bpy.data.orphans_purge(do_recursive=True)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-60,-60,0),(60,-60,0),(60,60,0),(-60,60,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,target,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=(Vector(target)-Vector(loc)).to_track_quat('-Z','Y').to_euler();pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-13,-24,6),(0,0,6),lens=30);cam('Front',(0,-40,6.2),(0,0,6.2),13.5);cam('Side',(-40,0,6.2),(0,0,6.2),13.5);cam('Rear',(0,40,6.2),(0,0,6.2),13.5)
cam('Walk',(12,.35,7.3),(3.3,.35,6.6),lens=24)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'WallTower-reference.png')
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs);assert tris<=200000,tris
manifest=dict(asset='SilverbrookWallTower',asset_id='BLD-GreenlandsWallTower-01',reference=dict(file='WallTower-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_walltower.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(h-l,3) for l,h in zip(lo,hi)],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookWallTower',parts=PARTS,slot_m=list(SLOT),
    wall_joint=dict(axis='X',joint_planes_x=[-J,J],end_section='identical to SilverbrookWallSection end section (asserted)',walk_top_z=WALK+PAVE,walk_y=[PB,RB],door_threshold_z=WALK+PAVE,masonry_uv_repeat_m=round(S,4)),
    outputs=['SilverbrookWallTower.blend','SilverbrookWallTower.glb','SilverbrookWallTower.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('WALLTOWER_BUILT',manifest['bounds_m'],tris)
