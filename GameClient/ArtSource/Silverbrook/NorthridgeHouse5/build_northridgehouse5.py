"""Silverbrook Northridge House 5 from docs/Game-Design/concept-art/Silverbrook/Northridge House-5.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_northridgehouse5.py
Original procedural geometry interpreting the elevation (14 m wide, ~16 m to the spire
finial, compressed to the 12 x 10 x 10 m slot): pale ashlar Gothic house with two steep
half-timbered front gables, a projecting gabled stone portal with pinnacles, a slate-roofed
bay window, a crenellated corner tower on corbels with a slate spire, two small dormers,
a gable-end stack, banners with a gilded stag, lanterns and iron downpipes.
Town asset BLD-GreenlandsNorthridgeHouse-01, variant 5.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,UP,tile_plane,deck,frame,lantern
import bpy,numpy as np
scene=kit.reset(4155)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Northridge House-5.png'
NAME='SilverbrookNorthridgeHouse5'
asset=bpy.data.collections.new(NAME);scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_NorthridgeHouse5_Slate','grey_roof_tiles_02',dark=(.10,.12,.24),lichen='lichen_rock',damp=False,aged=.55),
    stone=kit.scan('SB_NorthridgeHouse5_Ashlar','sandstone_blocks_05',dark=(.45,.40,.33)),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_NorthridgeHouse5_Banner','denim_fabric_03',dark=(.16,.19,.34),normal=.6,damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_NorthridgeHouse5_Gilt',(.80,.55,.18),.35,1.0))
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('sandstone_blocks_05',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.0)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
def stone():return kit.rng.choice(STONES)
G=dict(stone=Geo('SM_SBNorthridgeHouse5_Masonry',asset),timber=Geo('SM_SBNorthridgeHouse5_Timber',asset),roof=Geo('SM_SBNorthridgeHouse5_Roof',asset),detail=Geo('SM_SBNorthridgeHouse5_Details',asset))
S,D,T=G['stone'],G['detail'],G['timber']
# ---- measured layout (metres; concept ~90 px/m across, heights compressed ~0.75) ------
SLOT=(12,10,10)                      # town-layout slot w x d x h (concept README 'Fits')
XL,XR,FY,RY,TH=-5.3,5.5,-2.4,3.0,.45 # main walls: left/right/front/rear outer faces, thickness
EAVE,RIDGE=5.2,8.0                   # main eave and ridge (front gables share the ridge)
CY,EY,XE,XV=.3,3.0,5.8,-5.6          # roof centre y, eave half-depth, hip eave x, left verge x
KM=(RIDGE-EAVE)/EY;XRH=XE-EY         # main pitch, x where the ridge meets the right hip
GXS,GH=(-3.55,-.05),1.75             # front gables: centres, roof half width
KG=(RIDGE-EAVE)/GH;GY=FY-.25         # gable pitch, gable verge y
WT=EAVE+KM*(EY-(CY-FY))-.06          # wall top under the main eaves (roof at the outer face, less deck)
TX,TYC,TW=3.5,-2.25,1.35             # tower centre x, y and half width (2.7 m square)
TS,PT=7.25,7.9                       # tower shaft top, parapet top (merlons to 8.25)
PX,PW,PT2=GXS[1],1.35,3.6            # portal centre, half width, top of the porch block
DSP,DWD=2.35,1.5                     # door spring and width (sill = step top 0.3)
STK=(-4.85,.65)                      # gable-end stack
def main_z(x,y):return EAVE+KM*min(EY-abs(y-CY),XE-x)
def gable_z(x,xc):return RIDGE-abs(x-xc)*KG
def roof_y(z):return CY-EY+(z-EAVE)/KM          # main front slope: y at height z
in_gable=[(lambda xc:lambda p:abs(p.x-xc)<GH+.04 and p.y<CY and p.z<gable_z(p.x,xc)-.005)(xc) for xc in GXS]
in_tower=lambda p:abs(p.x-TX)<TW+.03 and abs(p.y-TYC)<TW+.03 and p.z<TS+.1
in_stack=lambda p:abs(p.x-STK[0])<.44 and abs(p.y-STK[1])<.39
under_main=lambda p:p.x>XV and CY-EY-.4<p.y<CY+EY+.4 and p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.2 else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
BIG=dict(size=(.34,.48,.022),exposure=.23)
def plane(O,e,u,L,lo,hi,covered,**kw):
    u=Vector(u).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.3,.7),**kw)
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n,cover=(),w=.26):
    a,b=Vector(a),Vector(b)
    for i in range(n):
        p,q=a+(b-a)*i/n,a+(b-a)*(i+1.04)/n
        if not any(c((p+q)/2) for c in cover):cap(p,q,w)
def hip(cx,cy,ex,ey,eave,ridge,cover=(),tiles={},capw=.26):
    """Hipped (or, with ex == ey, pyramidal) slate roof; ridge along the longer axis."""
    es,rise,rl,alongx=min(ex,ey),ridge-eave,abs(ex-ey),ex>=ey;L=math.hypot(es,rise)
    for out in [Vector((0,-1,0)),Vector((0,1,0)),Vector((1,0,0)),Vector((-1,0,0))]:
        side=out.x==0;half=ex if side else ey;r=rl if side==alongx else 0
        plane(Vector((cx,cy,eave))+out*(ey if side else ex),UP.cross(out),-out*es+UP*rise,L,lerp(-half,-r,L),lerp(half,r,L),cover,**tiles)
    ends=[Vector((cx+s*rl,cy,ridge+.05)) if alongx else Vector((cx,cy+s*rl,ridge+.05)) for s in (-1,1)]
    n=max(2,round(L/.45))
    if rl>.05:capped(ends[0],ends[1],max(1,round(2*rl/.45)),cover,capw)
    for sx in (-1,1):
        for sy in (-1,1):capped((cx+sx*ex,cy+sy*ey,eave+.05),ends[(sx if alongx else sy)>0],n,cover,capw)
def pyramid(g,c,half,z0,apex_,mat):
    cs=[Vector((c[0]+x*half,c[1]+y*half,z0)) for x,y in ((-1,-1),(1,-1),(1,1),(-1,1))];top=Vector((c[0],c[1],apex_))
    for i in range(4):g.poly([cs[i],cs[(i+1)%4],top],mat)
    g.poly(cs[::-1],mat)
def finial(x,y,z0,z1,mat=None):
    """Forged-iron finial: spike with two knops."""
    mat=mat or M['iron'];D.beam((x,y,z0),(x,y,z1),.04,.04,mat,grain=False)
    for z,r in ((z0+(z1-z0)*.35,.07),(z0+(z1-z0)*.7,.045)):D.vessel((x,y,z-r),[(0,0),(r*.7,r*.3),(r,r),(r*.7,r*1.7),(0,2*r)],mat,10)
def slab(g,pts,th,mat):
    """Thin plate through pts (a small roof); kit.prism orients by the Newell normal."""
    pts=[Vector(p) for p in pts];n=(pts[1]-pts[0]).cross(pts[2]-pts[0]).normalized();g.prism(pts,n if n.z>0 else -n,th,mat)
# ---- pointed (two-centred) arches -----------------------------------------------------
def arcpts(w,spring,off=0,n=9):
    """Right-hand arc of a pointed arch (radius .75 w), springing to apex, as (offset from axis, z)."""
    r,h=.75*w,w/2;c=h-r;R=r+off;t1=math.acos(-c/R)
    return [(c+R*math.cos(t),spring+R*math.sin(t)) for t in np.linspace(0,t1,n)]
def apex(w,spring,off=0):return arcpts(w,spring,off,2)[-1][1]
def seg(g,A,B,o,radial,depth,mat):
    t=B-A;L=t.length;t.normalize();n=Vector(o).normalized();rot=Matrix((n.cross(t),n,t)).transposed()
    g.box((A+B)/2,(radial,depth,L+.012),mat,.01,rot,rect=stone())
def ring(P,R,o,a,w,spring,off,radial,dc,depth,jamb=None,n=9):
    pts=arcpts(w,spring,off,n)
    for s in (1,-1):
        for (x0,z0),(x1,z1) in zip(pts,pts[1:]):seg(S,P(a+s*x0,dc,z0),P(a+s*x1,dc,z1),o,radial,depth,M['stone'])
        if jamb is not None:S.box(P(a+s*(w/2+off),dc,(jamb+spring)/2),(radial,depth,spring-jamb),M['stone'],.012,R,rect=stone())
def facade(c,o,lo,hi,openings=(),th=TH,top=lambda a:WT,breaks=()):
    """Ashlar wall from a=lo..hi with pointed-head openings (a, width, bottom, spring).
    Its top follows top(a) (linear between `breaks`): the roof surface at the OUTER face less
    the deck, so no wall top stands above the slates."""
    P,R=frame(c,o);holes=[(a,w,b,apex(w,s)) for a,w,b,s in openings]
    cuts=sorted({lo,hi,*[x for x in breaks if lo<x<hi],*[a+d*w/2 for a,w,_,_ in holes for d in (-1,1)]})
    for a0,a1 in zip(cuts,cuts[1:]):
        m=(a0+a1)/2;z=0;spans=[]
        for a,w,b,t in sorted(holes,key=lambda h:h[2]):
            if abs(a-m)<w/2:spans.append((z,b,b));z=t
        spans.append((z,top(a0),top(a1)))
        for z0,q0,q1 in spans:
            if min(q0,q1)-z0>.005:S.prism([P(a0,-th/2,z0),P(a1,-th/2,z0),P(a1,-th/2,q1),P(a0,-th/2,q0)],o,th,M['stone'])
    for a,w,b,s in openings:
        t=apex(w,s)
        for sd in (1,-1):S.prism([P(a+sd*w/2,-th/2,t)]+[P(a+sd*x,-th/2,z) for x,z in arcpts(w,s)],o,th,M['stone'])
    return P,R
def lancet(c,o,a,w,bottom,spring,th=TH,lights=1,decor=False,sill=True,hood=True,tracery=False):
    """Leaded Gothic window: lit glass, stone mullion (and two sub-arches with an oculus when
    tracery), lead cames, a moulded hood and sill. decor=True lays it on a solid face."""
    P,R=frame(c,o);dg=.03 if decor else -th/2;pts=arcpts(w,spring)
    outline=[P(a-w/2,dg,bottom),P(a+w/2,dg,bottom)]+[P(a+x,dg,z) for x,z in pts]+[P(a-x,dg,z) for x,z in pts[::-1][1:]]
    D.prism(outline,o,.02,M['glass'])
    def zarc(x):r,h=.75*w,w/2;return spring+math.sqrt(max(0,r*r-(abs(x)+r-h)**2))
    for i in range(1,lights):
        x=-w/2+i*w/lights;zt=spring+.37*w if tracery else zarc(x)
        S.box(P(a+x,dg,(bottom+zt)/2),(.09,.15,zt-bottom),M['stone'],.012,R,rect=stone())
    if tracery:
        for s in (-1,1):
            q=arcpts(w/2,spring,0,7)
            for (x0,z0),(x1,z1) in zip(q,q[1:]):
                for m in (1,-1):seg(S,P(a+s*w/4+m*x0,dg,z0),P(a+s*w/4+m*x1,dg,z1),o,.07,.13,M['stone'])
        oc=spring+.53*w;rr=.16*w;k=14
        for j in range(k):
            t0,t1=math.tau*j/k,math.tau*(j+1)/k;seg(S,P(a+rr*math.cos(t0),dg,oc+rr*math.sin(t0)),P(a+rr*math.cos(t1),dg,oc+rr*math.sin(t1)),o,.06,.13,M['stone'])
        for j in range(4):   # quatrefoil cusps
            t=math.pi/4+j*math.pi/2;D.beam(P(a,dg+.01,oc),P(a+rr*.85*math.cos(t),dg+.01,oc+rr*.85*math.sin(t)),.03,.03,M['lead'],grain=False)
    z=bottom+.28
    while z<spring-.08:D.beam(P(a-w/2,dg+.02,z),P(a+w/2,dg+.02,z),.014,.012,M['lead'],grain=False);z+=.28
    for x in np.arange(-w/2+.16,w/2-.08,.16):
        if spring-bottom>.1 and (lights<2 or min(abs(x-(-w/2+i*w/lights)) for i in range(1,lights))>.08):D.beam(P(a+x,dg+.02,bottom),P(a+x,dg+.02,min(spring,zarc(x)-.03)),.014,.012,M['lead'],grain=False)
    if hood:ring(P,R,o,a,w,spring,.08,.14,.03 if not decor else .06,.1 if not decor else .08,jamb=bottom)
    if sill:S.box(P(a,.05 if not decor else .08,bottom-.06),(w+.32,.2 if not decor else .14,.12),M['stone'],.012,R,rect=stone())
def planter(c,o,a,z,w):
    """Timber window box on two iron brackets; z = top of the box."""
    P,R=frame(c,o);T.box(P(a,.11,z-.09),(w,.24,.18),M['timber'],.006,R,grain=True,band=band())
    D.box(P(a,.11,z-.015),(w-.05,.19,.02),M['soil'],0,R)
    for s in (-1,1):D.beam(P(a+s*w*.35,-.01,z-.42),P(a+s*w*.35,.2,z-.18),.03,.03,M['iron'],grain=False)
def stag(origin,right,up,normal,s):
    """Gilded stag's head appliqué (face, ears, branched antlers)."""
    o=Vector(origin)+Vector(normal)*.008;R_,U,N=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+R_*(a*s)+U*(z*s)
    D.prism([p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,M['gilt'])
    for sx in (-1,1):
        D.prism([p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            D.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
def banner(c,o,a,ztop,zbot,w,d=.12):
    """Navy cloth banner with a pointed foot, gilt edging, a stag, on a rod and iron brackets."""
    P,R=frame(c,o);foot=min(.4,w*.55);pts=[(-w/2,ztop),(w/2,ztop),(w/2,zbot+foot),(0,zbot),(-w/2,zbot+foot)]
    D.prism([P(a+x,d,z) for x,z in pts],o,.02,M['banner'])
    for (x0,z0),(x1,z1) in zip(pts,pts[1:]+pts[:1]):D.beam(P(a+x0,d+.012,z0),P(a+x1,d+.012,z1),.03,.012,M['gilt'],grain=False)
    D.beam(P(a-w/2-.08,d,ztop+.04),P(a+w/2+.08,d,ztop+.04),.035,.035,M['iron'],grain=False)
    for s in (-1,1):D.box(P(a+s*(w/2+.03),d/2,ztop+.04),(.035,d+.03,.035),M['iron'],.004,R)
    o_=Vector(o).normalized();stag(P(a,d+.01,ztop-(ztop-zbot)*.38),UP.cross(o_),UP,o_,w*.95)
def dormer(x,yf,base,half,pitch,apex_z,ov=.2):
    """Small gabled stone dormer on the main front slope: face, two slate slabs, cheeks."""
    zt=apex_z-half*pitch;zs=lambda u:apex_z+.06-abs(u-x)*pitch
    S.prism([(x-half,yf+.15,base),(x+half,yf+.15,base),(x+half,yf+.15,zt),(x,yf+.15,apex_z),(x-half,yf+.15,zt)],(0,1,0),.3,M['stone'])
    y0,y1=yf-.18,roof_y(apex_z+.25)
    for s in (-1,1):
        e=x+s*(half+ov);G['roof'].prism([(e,y0,zs(e)),(x,y0,zs(x)),(x,y1,zs(x)),(e,y1,zs(e))],(s*pitch,0,1),.08,M['slate'])
        S.prism([(x+s*(half-.08),yf+.3,base),(x+s*(half-.08),yf+.3,zt),(x+s*(half-.08),roof_y(zt+.1),zt)],(1,0,0),.16,M['stone'])
        T.beam((e+s*.02,yf-.21,zs(e)-.05),(x-s*.05,yf-.21,apex_z+.11+.05*pitch),.05,.16,M['timber'],band=band())
    lancet((x,yf,base),(0,-1,0),0,half*.75,.12,.12+half*1.1,decor=True,sill=False,hood=False)
    finial(x,yf-.1,apex_z+.08,apex_z+.5)
    return lambda p:abs(p.x-x)<half+ov+.02 and yf-.25<p.y<y1+.05 and p.z<zs(p.x)-.005   # slates below the face stay
# ---- main walls ------------------------------------------------------------------------
GT=RIDGE-.08                                     # gable face apex (under the ridge deck)
def gtop(a):return max(WT,GT-KG*abs(a))
gb=(GT-WT)/KG
FRONT=(0,-1,0)
# front: two gables, the right bay (behind the tower), full width from the left wall to the right wall
g1,g2=GXS
facade((g1,FY,0),FRONT,XL+TH-g1,GH,[(-1.0,.36,3.65,4.75),(0,.46,3.65,6.0),(1.0,.36,3.65,4.75)],top=gtop,breaks=(-gb,0,gb))
facade((g2,FY,0),FRONT,-GH,GH,[(0,DWD,.3,DSP),(-1.2,.32,4.2,4.95),(0,1.0,4.75,5.5),(1.2,.32,4.2,4.95)],top=gtop,breaks=(-gb,0,gb))
facade((0,FY,0),FRONT,g2+GH,XR-TH,[])
# corner closer: where gable 1's roof rises above the main slope over the left wall's front end
x0=g1-(RIDGE-.08-WT)/KG;S.prism([(x0,FY+TH/2,WT),(XL+TH,FY+TH/2,WT),(XL+TH,FY+TH/2,gable_z(XL+TH,g1)-.08)],(0,1,0),TH,M['stone'])
RA=(-3.0,0,3.6)                      # rear: a = -x
facade((0,RY,0),(0,1,0),-(XR-TH),-(XL+TH),[(a,.5,1.0,2.2) for a in RA]+[(a,.44,3.5,4.6) for a in RA])
for a in RA:lancet((0,RY,0),(0,1,0),a,.5,1.0,2.2);lancet((0,RY,0),(0,1,0),a,.44,3.5,4.6)
# left gable end (full depth; top follows the main slopes) and right hip end
facade((XL,CY,0),(-1,0,0),-(RY-CY),-(FY-CY),[(0,.5,1.0,2.2),(0,.44,3.5,4.6),(0,.34,5.65,6.25)],top=lambda a:EAVE+KM*(EY-abs(a))-.06,breaks=(0,))
for b,s in ((1.0,2.2),(3.5,4.6),(5.65,6.25)):lancet((XL,CY,0),(-1,0,0),0,{1.0:.5,3.5:.44,5.65:.34}[b],b,s)
facade((XR,CY,0),(1,0,0),FY-CY,RY-CY,[(1.2,.5,1.0,2.2),(1.2,.44,3.5,4.6),(-1.4,.5,1.0,2.2),(-1.4,.44,3.5,4.6)])
for a in (1.2,-1.4):lancet((XR,CY,0),(1,0,0),a,.5,1.0,2.2);lancet((XR,CY,0),(1,0,0),a,.44,3.5,4.6)
# ---- front gables: windows, half-timbering, bargeboards, finials -------------------------
for a in (-1.0,1.0):lancet((g1,FY,0),FRONT,a,.36,3.65,4.75);planter((g1,FY,0),FRONT,a,3.47,.62)
lancet((g1,FY,0),FRONT,0,.46,3.65,6.0,lights=1);planter((g1,FY,0),FRONT,0,3.47,.72)
for a in (-1.2,1.2):lancet((g2,FY,0),FRONT,a,.32,4.2,4.95)
lancet((g2,FY,0),FRONT,0,1.0,4.75,5.5,lights=2,tracery=True)
yT=FY-.06
for xc,wh in ((g1,.5),(g2,.76)):
    def gx(z):return (GT-z)/KG
    for s in (-1,1):
        T.box((xc+s*(wh+1.4)/2,yT,EAVE+.32),(1.4-wh,.14,.16),M['timber'],.012,grain=True,band=band())       # tie beam
        T.box((xc+s*(wh+.06),yT,(EAVE+.4+gtop(wh+.06)-.04)/2),(.13,.13,gtop(wh+.06)-.04-EAVE-.4),M['timber'],.012,grain=True,band=band())  # posts
        T.beam((xc+s*1.3,yT,EAVE+.42),(xc+s*(wh+.12),yT,EAVE+1.0),.11,.11,M['timber'],band=band())          # braces
        T.beam((xc+s*(GH+.1),GY+.02,EAVE-.06),(xc-s*.1,GY+.02,RIDGE+.1*KG+.04),.09,.26,M['timber'],band=band())  # bargeboards
        for k in range(1,7):
            f=k/7;p=Vector((xc+s*(GH+.1),GY-.06,EAVE-.06)).lerp(Vector((xc,GY-.06,RIDGE+.04)),f);D.box(p,(.05,.04,.05),M['iron'],.004)
    zc=6.95;T.box((xc,yT,zc),(2*gx(zc)-.1,.14,.15),M['timber'],.012,grain=True,band=band())                   # collar
    T.box((xc,yT,(zc+GT)/2),(.15,.14,GT-zc),M['timber'],.012,grain=True,band=band())                          # king post
    T.box((xc,GY-.06,RIDGE-.55),(.16,.16,.7),M['timber'],.015,grain=True,band=band())                         # pendant
    finial(xc,GY-.06,RIDGE-.2,RIDGE+.75)
# ---- main roof: gable end left, hip right; front gables share the ridge ---------------------
LM=math.hypot(EY,RIDGE-EAVE)
dorm=[dormer(-1.8,roof_y(6.25),6.25,.3,1.3,7.1),dormer(1.93,roof_y(5.62),5.62,.22,1.3,6.3)]
plane((0,CY-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LM,lambda t:XV,lerp(XE,XRH,LM),in_gable+dorm+[in_tower,in_stack],**BIG)
plane((0,CY+EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LM,lerp(-XE,-XRH,LM),lambda t:-XV,[in_stack],**BIG)
plane((XE,0,EAVE),(0,1,0),(-EY,0,RIDGE-EAVE),LM,lerp(CY-EY,CY,LM),lerp(CY+EY,CY,LM),[in_tower],**BIG)
for xc in GXS:
    LG=math.hypot(GH,RIDGE-EAVE)
    plane((xc-GH,0,EAVE),(0,-1,0),(GH,0,RIDGE-EAVE),LG,lambda t:-CY,lambda t:-GY,[under_main,in_stack])
    plane((xc+GH,0,EAVE),(0,1,0),(-GH,0,RIDGE-EAVE),LG,lambda t:GY,lambda t:CY,[under_main,in_stack])
    capped((xc,GY,RIDGE+.05),(xc,CY,RIDGE+.05),7)
capped((XV,CY,RIDGE+.05),(XRH,CY,RIDGE+.05),22,[in_stack])
for sy in (-1,1):capped((XE,CY+sy*EY,EAVE+.05),(XRH,CY,RIDGE+.05),9,[in_tower])
for sy in (-1,1):T.beam((XV-.02,CY+sy*(EY+.02),EAVE-.08),(XV-.02,CY,RIDGE+.1),.07,.24,M['timber'],band=band())  # verge boards
T.beam((XV,CY+EY+.02,EAVE-.06),(XE,CY+EY+.02,EAVE-.06),.04,.2,M['timber'],band=band())
T.beam((XE+.02,CY-EY,EAVE-.06),(XE+.02,CY+EY,EAVE-.06),.04,.2,M['timber'],band=band())
T.beam((TX+TW,CY-EY-.02,EAVE-.06),(XE,CY-EY-.02,EAVE-.06),.04,.2,M['timber'],band=band())
# gable-end stack with two clay pots
S.box((*STK,6.6),(.8,.7,3.8),M['stone'],.01)
S.box((*STK,8.56),(.92,.82,.12),M['stone'],.015,rect=stone(),w=(.45,0))
for dx in (-.2,.2):D.vessel((STK[0]+dx,STK[1],8.62),[(.11,0),(.105,.3),(.115,.38),(.13,.42),(.10,.42),(.085,.36),(0,.36)],M['pot'])
# ---- ground storey: timber string rail, bay window, portal ---------------------------------
for a,b in ((XL,PX-PW),(PX+PW,TX-TW)):T.beam((a,FY-.07,3.12),(b,FY-.07,3.12),.14,.15,M['timber'],band=band())
bx=g1;foot=[(bx-1.05,-2.3),(bx+1.05,-2.3),(bx+.65,-3.05),(bx-.65,-3.05)]
S.prism([(x,y,1.1) for x,y in foot],(0,0,1),2.2,M['stone'])
S.prism([(bx-1.15,-2.3,2.27),(bx+1.15,-2.3,2.27),(bx+.73,-3.14,2.27),(bx-.73,-3.14,2.27)],(0,0,1),.14,M['stone'])
S.prism([(bx-1.12,-2.3,.5),(bx+1.12,-2.3,.5),(bx+.71,-3.11,.5),(bx-.71,-3.11,.5)],(0,0,1),.1,M['stone'])
slab(G['roof'],[(bx-.76,-3.17,2.34),(bx+.76,-3.17,2.34),(bx+.5,-2.38,3.0),(bx-.5,-2.38,3.0)],.07,M['slate'])
for s in (-1,1):slab(G['roof'],[(bx+s*.76,-3.17,2.34),(bx+s*1.2,-2.3,2.34),(bx+s*.5,-2.38,3.0)],.07,M['slate'])
for a in (-.4,0,.4):lancet((bx,-3.05,0),FRONT,a,.26,.75,1.85,decor=True,sill=False,hood=False)
for s in (-1,1):
    sc=(bx+s*.82,-2.725,0);lancet(sc,(s*.75,-.4,0),0,.24,.75,1.85,decor=True,sill=False,hood=False)
    T.box((bx+s*.65,-3.06,1.38),(.1,.1,1.66),M['timber'],.01,grain=True,band=band())
    T.box((bx+s*.2,-3.07,1.38),(.07,.06,1.66),M['timber'],.008,grain=True,band=band())
# portal: deep porch block with archivolts, steep gablet, pinnacle piers, plank double door
P,R=facade((PX,FY-.5,0),FRONT,-PW,PW,[(0,DWD,.3,DSP)],th=.5,top=lambda a:PT2)
S.prism([P(-1.0,-.25,PT2-.01),P(1.0,-.25,PT2-.01),P(0,-.25,PT2+1.2)],FRONT,.5,M['stone'])
for s in (-1,1):S.beam(P(s*1.1,-.25,PT2-.06),P(0,-.25,PT2+1.31),.62,.16,M['stone'],grain=False,rect=stone())
pyramid(S,(PX,FY-.75),.1,PT2+1.36,PT2+1.75,M['stone'])
for j in range(4):   # carved quatrefoil in the gablet
    t=math.pi/2*j;cx_,cz_=.13*math.cos(t),PT2+.48+.13*math.sin(t)
    for k in range(10):
        t0,t1=math.tau*k/10,math.tau*(k+1)/10;seg(S,P(cx_+.11*math.cos(t0),.03,cz_+.11*math.sin(t0)),P(cx_+.11*math.cos(t1),.03,cz_+.11*math.sin(t1)),FRONT,.04,.06,M['stone'])
for k,off in enumerate((.1,.3)):ring(P,R,FRONT,0,DWD,DSP,off,.18,.03+.04*k,.1,jamb=.3,n=11)
for s in (-1,1):
    S.box(P(s*1.2,.06,1.7),(.32,.62,3.4),M['stone'],.012)
    S.box(P(s*1.2,.06,3.46),(.4,.7,.12),M['stone'],.012,rect=stone())
    S.box(P(s*1.2,.06,3.7),(.22,.22,.36),M['stone'],.012,rect=stone())
    pyramid(S,(PX+s*1.2,FY-.56),.14,3.88,4.15,M['stone'])
    lantern(G,M,(PX+s*1.2,FY-1.25,2.45),arm=((PX+s*1.2,FY-.88,2.45),(0,-1,0)))
# door leaves in the main wall behind the porch, glazed tympanum over a transom
Pd,Rd=frame((PX,FY,0),FRONT)
lancet((PX,FY,0),FRONT,0,DWD,DSP+.07,DSP+.07,lights=2,sill=False,hood=False)
S.box(Pd(0,-TH/2,DSP),(DWD,TH,.14),M['stone'],.012,Rd,rect=stone())
LEAF={-1:Geo('SM_SBNorthridgeHouse5_Door1',asset),1:Geo('SM_SBNorthridgeHouse5_Door2',asset)};G.update(door1=LEAF[-1],door2=LEAF[1])  # each leaf opens at runtime
for s in (-1,1):
    L=LEAF[s];L.box(Pd(s*DWD/4,-.12,(.3+DSP-.07)/2),(DWD/2-.01,.08,DSP-.07-.3),M['door'],.006,Rd)
    for z in (.75,1.45,2.0):L.box(Pd(s*DWD*.27,-.075,z),(DWD*.4,.015,.06),M['iron'],.004,Rd)
    for k in range(10):
        t=math.tau*k/10;L.box(Pd(s*.15+.06*math.cos(t),-.07,1.15+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,Rd@Matrix.Rotation(-t,3,'Y'))
    L.box(Pd(s*.15,-.075,1.21),(.06,.012,.06),M['iron'],.004,Rd)
S.box((PX,FY-.5-.25,.15),(2.2,.5,.3),M['stone'],.012)        # step (top = door sill 0.3)
S.box((PX,FY-.5-.25-.35,.075),(2.5,.3,.15),M['stone'],.012)  # lower step
# banners on iron brackets
banner((0,FY,0),FRONT,-4.88,2.75,1.15,.36)
banner((0,FY,0),FRONT,-1.72,2.75,1.15,.36)
banner((0,FY,0),FRONT,1.68,2.75,1.15,.36)
# iron downpipes
for x in (-5.25,-2.1,2.0,5.38):
    y=FY-.07;D.beam((x,y,WT-.05),(x,y,.12),.07,.07,M['iron'],grain=False);D.box((x,y-.02,.08),(.1,.1,.08),M['iron'],.008)
    for zz in (1.0,2.6,4.2):D.box((x,FY-.02,zz),(.1,.06,.03),M['iron'],.004)
# ---- corner tower: lancets, corbel table, crenellated parapet, slate spire ----------------------
TF,TR=TYC-TW,TYC+TW
facade((TX,TF,0),FRONT,-TW,TW,[(-.6,.4,1.0,4.2),(.55,.42,.9,2.2),(-.6,.34,5.6,6.2),(.55,.36,5.65,6.2)],top=lambda a:TS)
for a,w,b,s in [(-.6,.4,1.0,4.2),(.55,.42,.9,2.2),(-.6,.34,5.6,6.2),(.55,.36,5.65,6.2)]:lancet((TX,TF,0),FRONT,a,w,b,s)
banner((TX,TF,0),FRONT,.55,5.15,3.05,.66,d=.1)
facade((TX,TR,0),(0,1,0),-TW,TW,top=lambda a:TS)
facade((TX-TW,TYC,0),(-1,0,0),-(TW-TH),TW-TH,[(.55,.28,1.2,2.3)],top=lambda a:TS);lancet((TX-TW,TYC,0),(-1,0,0),.55,.28,1.2,2.3)
facade((TX+TW,TYC,0),(1,0,0),-(TW-TH),TW-TH,[(-.55,.28,1.2,2.3),(-.55,.3,5.85,6.25)],top=lambda a:TS)
for b,s,w in ((1.2,2.3,.28),(5.85,6.25,.3)):lancet((TX+TW,TYC,0),(1,0,0),-.55,w,b,s)
OH=TW+.25
for o in (Vector((0,-1,0)),Vector((0,1,0)),Vector((1,0,0)),Vector((-1,0,0))):
    r=UP.cross(o);c=Vector((TX,TYC,0))+o*TW
    for a in np.linspace(-1.2,1.2,7):S.box(c+r*a+o*.125+UP*7.05,(.2,.25,.4) if o.x==0 else (.25,.2,.4),M['stone'],.012,rect=stone())
    side=o.x==0;S.box(Vector((TX,TYC,(TS+PT)/2))+o*(OH-.15),(2*OH,.3,PT-TS) if side else (.3,2*OH-.6,PT-TS),M['stone'],.012)
    for a in ((-1.4,-.47,.47,1.4) if side else (-.47,.47)):S.box(Vector((TX,TYC,PT+.175))+o*(OH-.15)+r*a,(.4,.3,.35) if side else (.3,.4,.35),M['stone'],.012,rect=stone())
S.box((TX,TYC,TS-.03),(2*(OH-.29),2*(OH-.29),.1),M['stone'],.01)
S.box((TX,TYC,TS+.2),(2.0,2.0,.4),M['stone'],.01)
hip(TX,TYC,1.08,1.08,TS+.37,9.5,capw=.18)
finial(TX,TYC,9.42,9.95)
for dx in (-1,1):D.vessel((TX+dx*(OH-.15),TR+.1,PT+.35),[(.09,0),(.085,.24),(.095,.3),(.105,.34),(.08,.34),(.07,.29),(0,.29)],M['pot'])
# ---- export, preview scene, manifest -----------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'ground not at z=0: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
def leaf_part(obj,name,s):
    """Front (-Y) door leaf hinged on its s side (-1 left, +1 right): pivot at the bottom of that edge on the inner
    (+Y) face; travel opens it inward. Checked by swinging it: it must end inside the building, clear of the jamb."""
    vs=[obj.matrix_world@v.co for v in obj.data.vertices];hx=(min if s<0 else max)(v.x for v in vs)
    pv=Vector((hx,max(v.y for v in vs),min(v.z for v in vs)));travel=90 if s<0 else -90
    sw=[pv+Matrix.Rotation(math.radians(travel),3,'Z')@(v-pv) for v in vs]
    assert min(v.y for v in sw)>pv.y-1e-3 and min(s*(pv.x-v.x) for v in sw)>-1e-3,f'{name} does not swing inward'
    print('DOOR_SWING',name,[round(c,3) for c in pv],travel,'open depth',round(max(v.y for v in sw)-pv.y,3))
    return (obj,name,tuple(pv),(0,0,1),'swing',travel,'closed')
OUT=HERE/NAME
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
# Textures stay in ../Textures (FBX references them relatively); embedding 2k scans makes a huge GLB.
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
PARTS=kit.export_parts([leaf_part(bpy.data.objects[G['door1'].name],'Door1',-1),leaf_part(bpy.data.objects[G['door2'].name],'Door2',1)],str(OUT),'SM_SilverbrookNorthridgeHouse5')
for o in (bpy.data.objects[G['door1'].name],bpy.data.objects[G['door2'].name]):o.select_set(False)   # leaves ship as their own meshes, not in the house FBX
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-40,-40,0),(40,-40,0),(40,40,0),(-40,40,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,target,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=(Vector(target)-Vector(loc)).to_track_quat('-Z','Y').to_euler();pre.objects.link(o);c.clip_end=300
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-12,-17,5.5),(0,0,4.6),lens=38);cam('Front',(0,-40,5.0),(0,0,5.0),12.8);cam('Side',(-40,0,5.0),(0,0,5.0),12.8);cam('Rear',(0,40,5.0),(0,0,5.0),12.8)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'NorthridgeHouse5-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120_000,f'house triangle budget: {tris}'
manifest=dict(asset=NAME,town_asset='BLD-GreenlandsNorthridgeHouse-01',variant=5,reference=dict(file='NorthridgeHouse5-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_northridgehouse5.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_'+NAME,parts=PARTS,slot_m=list(SLOT),outputs=[NAME+'.blend',NAME+'.glb',NAME+'.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('NORTHRIDGEHOUSE5_BUILT',manifest['bounds_m'],tris)
