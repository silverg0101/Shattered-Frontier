"""Silverbrook Northridge House 1 from docs/Game-Design/concept-art/Silverbrook/Northridge House-1.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_northridgehouse1.py
Original procedural geometry interpreting the elevation (drawn 14 m wide and ~12 m to the
spire tip, fitted to the 12 x 10 x 10 m slot): pale squared-stone house with a jettied,
half-timbered left cross wing over a canted bay, a central stone gable with a three-light
pointed window over a gabled stone porch and pointed double door, a round stair tower with a
conical slate spire and spire dormer, a hipped right wing with a dormer, two stacks,
navy stag banners, lanterns, gutters and downpipes.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,UP,tile_plane,deck,wall,window,lantern,frame
import bpy
import numpy as np
from mathutils import Matrix
scene=kit.reset(4131)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Northridge House-1.png'
asset=bpy.data.collections.new('SilverbrookNorthridgeHouse1');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_NorthridgeHouse1_Ashlar','sandstone_blocks_05',dark=(.45,.40,.33)),
    trim=kit.scan('SB_NorthridgeHouse1_Dressed','sandstone_blocks_08',dark=(.45,.40,.33)),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_NorthridgeHouse1_Banner','denim_fabric_03',dark=(.10,.12,.28),damp=False,aged=.5),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_NorthridgeHouse1_Gilt',(.80,.55,.18),.35,1.0))
M['ashlar']=M['trim'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
DRESSED=kit.patches('sandstone_blocks_08',(.05,.035),dark=.7,frac=.01,std=.22)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
def rect():return kit.rng.choice(DRESSED)
G=dict(stone=Geo('SM_SBNorthridgeHouse1_Masonry',asset),trim=Geo('SM_SBNorthridgeHouse1_Trim',asset),plaster=Geo('SM_SBNorthridgeHouse1_Plaster',asset),
       timber=Geo('SM_SBNorthridgeHouse1_Timber',asset),roof=Geo('SM_SBNorthridgeHouse1_Roof',asset),detail=Geo('SM_SBNorthridgeHouse1_Details',asset))
S,T,D=G['stone'],G['trim'],G['detail']
# ---- measured layout (metres; elevation read at ~85 px/m across, ~92 px/m up, roofs and spire compressed to 10 m) ----
SLOT=(12,10,10)                # town-layout slot w x d x h (concept README 'Fits')
EAVE,RIDGE=5.65,8.5            # every eave and every ridge
D2=2.6;EY=D2+.3;KF=(RIDGE-EAVE)/EY              # main range: wall half depth, eave half depth, pitch
XL,LW,LH=-3.55,1.7,2.0;KG=(RIDGE-EAVE)/LH       # left cross wing (full depth): centre x, wall half, roof half
JZ,FL,JY=3.2,3.5,-2.9                           # wing jetty: bressumer underside, upper floor, upper front face
LY0,LY1=JY-.25,D2+.3                            # wing roof extent in y
CW,CH=1.85,2.1;KC=(RIDGE-EAVE)/CH               # central front gable: wall half, roof half
XR=5.35;EX=XR+.3;KS=2.0;RX=EX-(RIDGE-EAVE)/KS   # right end wall, eave, steep hip
XT,YT,TR=2.85,-D2,1.0                           # round tower centre and radius
ZC0,ZA,RC=7.05,9.55,1.45                        # tower cone eave, apex, eave radius
XD,DH,DW,DE,DR=4.55,.38,.3,6.85,7.15;KD=(DR-DE)/DH;YD=-(RIDGE-6.1)/KF   # right-wing dormer
STK=[(-4.7,1.2,6.2,8.85),(4.3,.4,7.4,9.15)]     # stacks: x, y, base, top
CAPEX=RIDGE-.05
def range_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,x-RX)*KS)
def wing_z(x):return RIDGE-abs(x-XL)*KG
def cg_z(x):return RIDGE-abs(x)*KC
def dormer_z(x):return DR-abs(x-XD)*KD
def cone_r(z):return RC*(ZA-z)/(ZA-ZC0)
in_wing=lambda p:abs(p.x-XL)<LH+.02 and LY0-.05<p.y<LY1+.05 and p.z<wing_z(p.x)-.005
in_cg=lambda p:abs(p.x)<CH+.02 and p.y<.02 and p.z<cg_z(p.x)-.005
def in_tower(p):
    r=math.hypot(p.x-XT,p.y-YT);return (r<TR+.06 and p.z<ZC0+.1) or (r<RC and p.z<ZA-(ZA-ZC0)*r/RC)
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
in_stack=lambda p:any(abs(p.x-x)<.47 and abs(p.y-y)<.42 for x,y,_,_ in STK)
under_range=lambda p:p.x>XL and p.z<range_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.4+.3*max(0,1-(p.z-EAVE)/2) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n,cover=()):
    a,b=Vector(a),Vector(b)
    for i in range(n):
        p,q=a+(b-a)*i/n,a+(b-a)*(i+1.04)/n
        if not any(c((p+q)/2) for c in cover):cap(p,q)
def post(x,y,z0,z1,o,s=.16):G['timber'].box((x,y,(z0+z1)/2),(s if o=='y' else .12,.12 if o=='y' else s,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.15):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
def fill(geo,pts,axis,th,mat):
    """Flat slab around a planar outline, caps fanned from pts[0] (Townhouse1)."""
    pts=[Vector(p) for p in pts];axis=Vector(axis).normalized()
    if sum((a.cross(pts[(i+1)%len(pts)]) for i,a in enumerate(pts)),Vector()).dot(axis)<0:pts.reverse()
    o=axis*th/2;n=len(pts)
    for i in range(1,n-1):geo.poly([pts[0]+o,pts[i]+o,pts[i+1]+o],mat);geo.poly([pts[0]-o,pts[i+1]-o,pts[i]-o],mat)
    for i,a in enumerate(pts):b=pts[(i+1)%n];geo.poly([a-o,b-o,b+o,a+o],mat)
def lathe(geo,c,profile,mat,seg=32,w=(0,0)):
    """Surface of revolution (TempleOfTheDawn); profile runs bottom-out to top-in so faces point outward."""
    cx,cy,cz=c;Sc=kit.SCALE[mat.name]
    for (ra,za),(rb,zb) in zip(profile,profile[1:]):
        if ra<1e-6 and rb<1e-6:continue
        rm=max((ra+rb)/2,1e-3)
        for j in range(seg):
            t0,t1=math.tau*j/seg,math.tau*(j+1)/seg;q=[(ra,t0,za),(ra,t1,za),(rb,t1,zb),(rb,t0,zb)]
            if ra<1e-6:q.pop(1)
            elif rb<1e-6:q.pop(3)
            geo.poly([(cx+r*math.cos(t),cy+r*math.sin(t),cz+z) for r,t,z in q],mat,[(rm*t/Sc,z/Sc) for r,t,z in q],w)
# ---- walls with pointed and square openings (TempleOfTheDawn face()) -----------------------------
def pointed(w,spring):return lambda z:w/2 if z<=spring else math.sqrt(max(0,w*w-(z-spring)**2))-w/2
def lancet_hole(a,w,lo,spring):return (a,pointed(w,spring),lo,spring+w*math.sqrt(3)/2,spring)
def sq_hole(a,w,z0,h):return (a,lambda z:w/2+.09,z0-.09,z0+h+.09,z0+h+.09)   # glazing() frame aperture
def gable(half,eave,apex):return lambda z:(-half,half) if z<=eave else (-half*(apex-z)/(apex-eave),half*(apex-z)/(apex-eave))
def face(geo,mat,c,o,outline,z0,z1,holes=(),th=.3,breaks=()):
    """Wall slab (outer face through c) cut into courses of trapezoid prisms between the
    outline and the holes (a, hw(z), lo, hi, curve_from)."""
    P,R=frame(c,o);o=Vector(o).normalized()
    ks=sorted({z0,z1,*breaks,*(v for h in holes for v in h[2:5])});ks=[k for k in ks if z0<=k<=z1];zs=[ks[0]]
    for a,b in zip(ks,ks[1:]):
        if b-a<1e-6:continue
        n=max(1,math.ceil((b-a)/.07)) if any(h[4]<b and a<h[3] for h in holes) else 1
        zs+=[a+(b-a)*i/n for i in range(1,n+1)]
    for za,zb in zip(zs,zs[1:]):
        zm=(za+zb)/2;edges=[(outline(za)[0],outline(zb)[0])]
        for a,hw,lo,hi,_ in sorted([h for h in holes if h[2]<zm<h[3]],key=lambda h:h[0]):
            f=lambda z:max(0,hw(min(max(z,lo),hi)));edges+=[(a-f(za),a-f(zb)),(a+f(za),a+f(zb))]
        edges.append((outline(za)[1],outline(zb)[1]))
        for (x0,x1),(y0,y1) in zip(edges[::2],edges[1::2]):
            pts=[]
            for p in [(x0,za),(y0,za),(y1,zb),(x1,zb)]:
                if not pts or abs(p[0]-pts[-1][0])+abs(p[1]-pts[-1][1])>1e-5:pts.append(p)
            if len(pts)>1 and abs(pts[0][0]-pts[-1][0])+abs(pts[0][1]-pts[-1][1])<1e-5:pts.pop()
            area=sum(pts[i][0]*pts[i-1][1]-pts[i-1][0]*pts[i][1] for i in range(len(pts)))/2
            if len(pts)>=3 and abs(area)>1e-5:geo.prism([P(x,-th/2,z) for x,z in pts],o,th,mat)
def arch_pts(a,w,spring,off=0,n=8):
    """Equilateral pointed arch offset outward by `off`: right spring -> apex -> left spring."""
    R_=w+off;ta=math.acos(min(1,(w/2)/R_));right=[(a-w/2+R_*math.cos(t),spring+R_*math.sin(t)) for t in [ta*i/n for i in range(n+1)]]
    return right+[(2*a-x,z) for x,z in reversed(right[:-1])]
def ring(P,pts,d,size,depth=None):
    for (x0,z0),(x1,z1) in zip(pts,pts[1:]):
        a,b=P(x0,d,z0),P(x1,d,z1);ext=(b-a).normalized()*.02;T.beam(a-ext,b+ext,size,depth or size,M['trim'],grain=False,rect=rect())
def lancet(c,o,a,w,lo,spring,th=.3,lights=1,decor=False,sill=True):
    """Leaded pointed window: lit glass, lead cames, stone mullions, dressed hood and sill.
    In a face() hole, or (decor) laid on a solid face with full-height jambs."""
    P,R=frame(c,o);top=spring+w*math.sqrt(3)/2;hw=pointed(w,spring);dg=.03 if decor else -th*.45;dr=.06 if decor else .045
    out=[(a-w/2-.02,lo),(a+w/2+.02,lo)]+arch_pts(a,w,spring,.02)
    D.prism([P(x,dg,z) for x,z in out],o,.03,M['glass'])
    dl=dg+.022;z=lo+.25
    while z<top-.1:
        h=hw(z);D.beam(P(a-h,dl,z),P(a+h,dl,z),.014,.014,M['lead'],grain=False);z+=.25
    for i in range(1,lights):
        x=-w/2+i*w/lights;zt=spring+math.sqrt(max(0,w*w-(abs(x)+w/2)**2))
        T.box(P(a+x,dg,(lo+zt)/2),(.08,.14,zt-lo),M['trim'],.01,R,rect=rect())
    for k in range(1,2*lights):
        x=-w/2+k*w/(2*lights)
        if k%2:D.beam(P(a+x,dl,lo),P(a+x,dl,spring+math.sqrt(max(0,w*w-(abs(x)+w/2)**2))-.02),.014,.014,M['lead'],grain=False)
    ring(P,arch_pts(a,w,spring,.09),dr,.11)
    for s in (-1,1):ring(P,[(a+s*(w/2+.09),lo if decor else spring-.3),(a+s*(w/2+.09),spring)],dr,.11)
    if sill:
        if decor:T.box(P(a,.06,lo-.06),(w+.3,.12,.12),M['trim'],.012,R,rect=rect())
        else:T.box(P(a,(-th+.1)/2,lo-.06),(w+.26,th+.1,.14),M['trim'],.012,R,rect=rect())
def glazing(c,o,w,h,lights=2,th=.3,sill=True,planter=False):
    """Leaded lights in an oak frame (Townhouse1); c: bottom centre of the glass on the outer face."""
    P,R=frame(c,o);t=.09;fd=th+.04;gd=-min(.14,th/2-.01)
    def B(geo,a,d,z,size,mat,bevel=.008,**kw):geo.box(P(a,d,z),size,mat,bevel,R,**kw)
    for a in (-(w/2+t/2),w/2+t/2):B(G['timber'],a,-th/2+.02,h/2,(t,fd,h+2*t),M['timber'],grain=True,band=band())
    for z in (h+t/2,-t/2):B(G['timber'],0,-th/2+.02,z,(w,fd,t),M['timber'],grain=True,band=band())
    pw=(w-.06*(lights-1))/lights
    for i in range(lights):
        ac=-w/2+pw/2+i*(pw+.06)
        if i:B(G['timber'],ac-pw/2-.03,gd+.05,h/2,(.06,.1,h),M['timber'],grain=True,band=band())
        B(D,ac,gd,h/2,(pw,.01,h),M['glass'],0)
        lo,hi=ac-pw/2,ac+pw/2;slope=1.7;step=.10*slope
        for sign in (1,-1):
            for k in range(-40,41):
                b=k*step;pts=[]
                for a in (lo,hi):
                    z=sign*slope*(a-ac)+b
                    if 0<=z<=h:pts.append((a,z))
                for z in (0,h):
                    a=ac+(z-b)/(sign*slope)
                    if lo<a<hi:pts.append((a,z))
                if len(pts)>=2:
                    (a0,z0),(a1,z1)=pts[0],pts[1]
                    if math.hypot(a1-a0,z1-z0)>.02:D.beam(P(a0,gd+.012,z0),P(a1,gd+.012,z1),.009,.012,M['lead'],grain=False)
    if sill:B(G['timber'],0,.05,-t-.025,(w+2*t+.12,.14,.05),M['timber'],grain=True,band=band())
    if planter:planter_box(P,R,w+2*t+.1,-t-.25)
def planter_box(P,R,w,z,brackets=True):
    G['timber'].box(P(0,.17,z),(w,.24,.2),M['planks'],.006,R)
    D.box(P(0,.17,z+.105),(w-.06,.19,.02),M['soil'],0,R)
    for a in (-w/2+.1,w/2-.1)*brackets:G['timber'].beam(P(a,.03,z-.35),P(a,.25,z-.1),.06,.06,M['timber'],band=band())
def banner(c,o,a,ztop,zbot,w,d=.14,emblem=True):
    """Navy banner with a pointed foot, gilt edging and rod on iron brackets (TownHall), gilt stag."""
    P,R=frame(c,o);pts=[(-w/2,ztop),(w/2,ztop),(w/2,zbot+.35),(0,zbot),(-w/2,zbot+.35)]
    D.prism([P(a+x,d,z) for x,z in pts],o,.02,M['banner'])
    for (x0,z0),(x1,z1) in zip(pts,pts[1:]+pts[:1]):D.beam(P(a+x0,d+.012,z0),P(a+x1,d+.012,z1),.03,.012,M['gilt'],grain=False)
    D.beam(P(a-w/2-.1,d,ztop+.05),P(a+w/2+.1,d,ztop+.05),.04,.04,M['gilt'],grain=False)
    for s in (-1,1):D.box(P(a+s*(w/2+.04),d/2,ztop+.05),(.04,d+.04,.04),M['iron'],.004,R)
    if emblem:stag(D,P(a,d+.011,(ztop+zbot)/2+.12),R@Vector((1,0,0)),UP,Vector(o).normalized(),w*1.05)
def stag(geo,origin,right,up,normal,s):
    """Gilt stag head with antlers (Townhouse1)."""
    o=Vector(origin)+Vector(normal)*.008;Rv,U,N=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+Rv*(a*s)+U*(z*s)
    fill(geo,[p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,M['gilt'])
    for sx in (-1,1):
        fill(geo,[p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
def finial(x,y,z0,z1):
    """Oak post with an iron spike and knop (the concept's gable finials)."""
    G['timber'].box((x,y,z0+.35),(.15,.15,.7),M['timber'],.02,grain=True,band=band())
    G['timber'].box((x,y,z0+.72),(.21,.21,.05),M['timber'],.02,grain=True,band=band())
    D.beam((x,y,z0+.74),(x,y,z1),.045,.045,M['iron'],grain=False)
    D.vessel((x,y,z0+.85),[(0,0),(.06,.03),(.075,.09),(.05,.15),(0,.18)],M['iron'],8)
def pipe(pts):
    for a,b in zip(pts,pts[1:]):D.beam(a,b,.07,.07,M['iron'],grain=False)
# ---- left cross wing roof (full depth, gable both ends) -----------------------------------------
LG=math.hypot(LH,RIDGE-EAVE)
plane((XL-LH,0,EAVE),(0,-1,0),(LH,0,RIDGE-EAVE),LG,lambda t:-LY1,lambda t:-LY0,[in_stack])
plane((XL+LH,0,EAVE),(0,1,0),(-LH,0,RIDGE-EAVE),LG,lambda t:LY0,lambda t:LY1,[under_range,in_cg])
capped((XL,LY0,RIDGE+.05),(XL,LY1,RIDGE+.05),14)
for y in (LY0-.04,LY1+.04):
    for s in (-1,1):G['timber'].beam((XL+s*(LH+.05),y,EAVE-.08),(XL-s*.16,y,RIDGE+.16*KG+.03),.08,.26,M['timber'],band=band())
finial(XL,LY0-.06,RIDGE+.05,9.2)
# ---- main range: front and rear slopes, steep right hip -----------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lambda t:XL,lerp(EX,RX,LF),[in_wing,in_cg,in_tower,in_dormer,in_stack])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lambda t:-XL,[in_wing,in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
capped((XL+.3,0,RIDGE+.05),(RX,0,RIDGE+.05),18,[in_stack])
for sy in (-1,1):capped((EX,sy*EY,EAVE+.05),(RX,0,RIDGE+.05),7,[in_tower])
G['timber'].beam((XL+LH,EY+.02,EAVE-.06),(EX,EY+.02,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((EX+.02,-EY,EAVE-.06),(EX+.02,EY,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((XT+RC,-EY-.02,EAVE-.06),(EX,-EY-.02,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((XL-LH-.02,LY0,EAVE-.06),(XL-LH-.02,LY1,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- central front gable ---------------------------------------------------------------------------
LC=math.hypot(CH,RIDGE-EAVE)
plane((-CH,0,EAVE),(0,-1,0),(CH,0,RIDGE-EAVE),LC,lambda t:0,lambda t:EY,[under_range,in_wing])
plane((CH,0,EAVE),(0,1,0),(-CH,0,RIDGE-EAVE),LC,lambda t:-EY,lambda t:0,[under_range,in_tower])
capped((0,-EY,RIDGE+.05),(0,-.4,RIDGE+.05),7)
for s in (-1,1):G['timber'].beam((s*(CH+.05),-EY-.04,EAVE-.08),(-s*.16,-EY-.04,RIDGE+.16*KC+.03),.08,.26,M['timber'],band=band())
finial(0,-EY-.06,RIDGE+.05,9.45)
# ---- right-wing dormer (Cottage1) --------------------------------------------------------------------
LD=math.hypot(DH,DR-DE);yb=(RIDGE-DR)/KF-.12
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:yb,lambda t:-YD+.16,[under_range])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:-yb,[under_range])
capped((XD,YD-.16,DR+.04),(XD,-yb,DR+.04),3)
for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
DB=range_z(XD,YD)-.1;DT=dormer_z(XD+DW)-.04
wall(G['plaster'],M['plaster'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.53,.11,.67)],th=.12)
fill(G['plaster'],[(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,DT),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,DT)],(0,1,0),.12,M['plaster'])
ym=-(RIDGE-DT)/KF
for s in (-1,1):fill(G['plaster'],[(XD+s*DW,YD,DB),(XD+s*DW,YD,DT),(XD+s*DW,ym,DT)],(1,0,0),.08,M['plaster'])
for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.1,.1,DE-DB),M['timber'],.01,grain=True,band=band())
G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.12),M['timber'],.01,grain=True,band=band())
glazing((XD,YD,DB+.2),(0,-1,0),.35,.38,1,th=.12)
# ---- stacks -----------------------------------------------------------------------------------------
for x,y,z0,z1 in STK:
    S.box((x,y,(z0+z1)/2),(.8,.72,z1-z0),M['stone'],.01)
    T.box((x,y,z1-.4),(.88,.8,.1),M['trim'],.015,rect=rect())
    T.box((x,y,z1+.06),(.94,.86,.12),M['trim'],.015,rect=rect(),w=(.45,0))
    for dx in (-.19,.19):D.vessel((x+dx,y,z1+.12),[(.1,0),(.095,.2),(.105,.25),(.12,.29),(.09,.29),(.075,.24),(0,.24)],M['pot'])
# ---- walls ------------------------------------------------------------------------------------------
PLATE=range_z(0,-D2)-.04                         # main range wall plate (roof underside at the outer face)
GC=wing_z(XL+LW)-.05                             # wing eave-side wall tops and gable foot
FRONT=lambda z:(-CW,XR) if z<=PLATE+1e-6 else (-min(CW,(CAPEX-z)/KC),min(CW,(CAPEX-z)/KC))
WIN_C=(0,1.2,4.65,5.55);WIN_F=[(-1.3,.32,4.45,5.05),(1.3,.32,4.45,5.05)];WIN_R=(4.6,.45,.95,1.95)
face(S,M['stone'],(0,-D2,0),(0,-1,0),FRONT,0,CAPEX,[lancet_hole(*WIN_C)]+[lancet_hole(*h) for h in WIN_F]+[lancet_hole(*WIN_R)],breaks=[PLATE])
face(S,M['stone'],(XL,-D2,0),(0,-1,0),lambda z:(-LW,LW),0,FL)                       # wing ground storey (bay in front)
WU=(0,1.5,3.95,1.05)
face(S,M['stone'],(XL,JY,0),(0,-1,0),lambda z:(-LW,LW),FL,GC,[sq_hole(*WU)]+[lancet_hole(s*1.2,.3,4.0,4.6) for s in (-1,1)])
face(G['plaster'],M['plaster'],(XL,JY,0),(0,-1,0),gable(LW,GC,CAPEX),GC,CAPEX)
face(S,M['stone'],(XL,D2,0),(0,1,0),lambda z:(-LW,LW),0,GC,[sq_hole(0,.7,1.0,1.1),sq_hole(0,.7,4.0,1.0)])
face(G['plaster'],M['plaster'],(XL,D2,0),(0,1,0),gable(LW,GC,CAPEX),GC,CAPEX)
face(S,M['stone'],(XL-LW,0,0),(-1,0,0),lambda z:(-D2+.3,D2-.3),0,FL,[sq_hole(0,.7,1.0,1.1)])
face(S,M['stone'],(XL-LW,0,0),(-1,0,0),lambda z:(-D2+.3,D2),FL,GC,[sq_hole(0,.7,4.0,1.0)])
REAR=[(-0,.7,1.0,1.1),(-3.6,.7,1.0,1.1),(-0,.7,4.0,1.0),(-3.6,.7,4.0,1.0)]      # a = -x on the rear face
face(S,M['stone'],(0,D2,0),(0,1,0),lambda z:(-XR,CW),0,PLATE,[sq_hole(*h) for h in REAR])
face(S,M['stone'],(XR,0,0),(1,0,0),lambda z:(-D2+.3,D2-.3),0,PLATE,[sq_hole(0,.7,1.0,1.1),sq_hole(0,.7,4.0,1.0)])
top=[(y,range_z(XR,y)-.05) for y in np.linspace(-D2+.3,D2-.3,25)];top=[(y,z) for y,z in top if z>PLATE+.01]
fill(S,[(XR-.15,y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),.3,M['stone'])
# plinth course (sides, rear, right wing front)
T.box(((XR+XL-LW)/2,D2+.02,.22),(XR-XL+LW+.08,.1,.44),M['trim'],.01)
for x in (XL-LW-.02,XR+.02):T.box((x,0,.22),(.1,2*D2,.44),M['trim'],.01)
T.box(((XT+TR+XR)/2+.02,-D2-.02,.22),(XR-XT-TR+.04,.1,.44),M['trim'],.01)
# ---- windows ----------------------------------------------------------------------------------------
lancet((0,-D2,0),(0,-1,0),*WIN_C,lights=3)
for h in WIN_F:lancet((0,-D2,0),(0,-1,0),*h)
lancet((0,-D2,0),(0,-1,0),*WIN_R)
P,R=frame((0,-D2,0),(0,-1,0));planter_box(P,R,1.7,4.36,False)
glazing((XL,JY,WU[2]),(0,-1,0),WU[1],WU[3],3,planter=True)
for s in (-1,1):lancet((XL,JY,0),(0,-1,0),s*1.2,.3,4.0,4.6)
lancet((XL,JY,0),(0,-1,0),0,.36,6.35,6.85,decor=True)
for z,h in ((1.0,1.1),(4.0,1.0)):
    glazing((XL,D2,z),(0,1,0),.7,h);glazing((XL-LW,0,z),(-1,0,0),.7,h);glazing((XR,0,z),(1,0,0),.7,h)
    for x in (0,3.6):glazing((x,D2,z),(0,1,0),.7,h)
# ---- left wing: jetty bressumer, corbels, half-timbering ---------------------------------------
G['timber'].box((XL,JY-.03+(.37)/2-.02,(JZ+FL)/2),(2*LW+.12,.37,FL-JZ),M['timber'],.015,grain=True,band=band())
for x in (XL-LW+.1,XL+LW-.1):fill(G['timber'],[(x,-D2-.01,JZ),(x,JY-.03,JZ),(x,JY-.03,JZ-.12),(x,-D2-.01,JZ-.45)],(1,0,0),.14,M['timber'])
y=JY-.03
for a in (-1.62,1.62):post(XL+a,y,FL,GC-.02,'y')
rail((XL-LW,y,FL+.07),(XL+LW,y,FL+.07),.14);rail((XL-LW,y,GC-.07),(XL+LW,y,GC-.07),.16)
def gx(z):return (CAPEX-z)/KG
for s in (-1,1):post(XL+s*.42,y,GC,7.42,'y',.12);rail((XL+s*(LW-.15),y,GC+.05),(XL+s*.55,y,6.75),.1)
rail((XL-gx(7.5)+.05,y,7.5),(XL+gx(7.5)-.05,y,7.5),.14);post(XL,y,7.57,RIDGE-.12,'y',.14)
for s in (-1,1):rail((XL,y,7.62),(XL+s*.3,y,7.85),.09)
y=D2+.03                                          # wing rear gable framing
rail((XL-LW,y,GC-.07),(XL+LW,y,GC-.07),.16);post(XL,y,GC,RIDGE-.12,'y',.14)
for s in (-1,1):rail((XL+s*(LW-.15),y,GC+.05),(XL+s*.1,y,7.3),.1)
# ---- canted bay under the jetty (Townhouse1) ---------------------------------------------------------
A0,A1,BP,Y0=XL-1.35,XL+1.35,.55,-D2
BAY=[Vector(p) for p in ((A0,Y0),(A0+BP,Y0-BP),(A1-BP,Y0-BP),(A1,Y0))]
def offset(pts,ds):
    out=[];lines=[]
    for (a,b),d in zip(zip(pts,pts[1:]),ds):
        e=(b-a).normalized();n=Vector((e.y,-e.x));lines.append((a+n*d,e))
    def meet(l1,l2):
        (p,e),(q,f)=l1,l2;den=e.x*f.y-e.y*f.x;t=((q-p).x*f.y-(q-p).y*f.x)/den;return p+e*t
    wall_line=(Vector((0,Y0+.02)),Vector((1,0)))
    return [meet(wall_line,lines[0])]+[meet(lines[i],lines[i+1]) for i in range(len(lines)-1)]+[meet(lines[-1],wall_line)]
def slab(geo,pts,z0,z1,mat):fill(geo,[(p.x,p.y,(z0+z1)/2) for p in pts],(0,0,1),z1-z0,mat)
BZ,BT,BR_=.95,2.55,2.7
slab(S,offset(BAY,[.0]*3),0,BZ,M['stone'])
slab(G['timber'],offset(BAY,[.05]*3),BZ,BZ+.08,M['timber'])
slab(G['timber'],offset(BAY,[.06]*3),BT,BR_,M['timber'])
for i,(a,b) in enumerate(zip(BAY,BAY[1:])):
    e=(b-a);L=e.length;e=e.normalized();o=Vector((e.y,-e.x,0));mid=(a+b)/2
    glazing((mid.x,mid.y,BZ+.17),o,L-.2,BT-BZ-.35,3 if i==1 else 1,th=.12,sill=False)
for p in BAY[1:3]:G['timber'].box((p.x,p.y,(BZ+.08+BT)/2),(.13,.13,BT-BZ-.08),M['timber'],.01,grain=True,band=band())
EB=offset(BAY,[.1]*3);BRISE,BRUN=JZ-BR_,BP+.1;pitch=math.atan2(BRISE,BRUN)
def corner_k(a,b,c):
    u,v=(a-b).normalized(),(c-b).normalized();return math.acos(max(-1,min(1,u.dot(v))))
for i,(a,b) in enumerate(zip(EB,EB[1:])):
    e=(b-a);L=e.length;e3=Vector((e.x,e.y,0)).normalized();h=Vector((-e3.y,e3.x,0))
    ka=1/math.tan(corner_k(EB[i-1],a,b)/2) if i>0 else 1/math.tan(corner_k(Vector((a.x+1,a.y)),a,b))
    kb=1/math.tan(corner_k(a,b,EB[i+2])/2) if i<2 else 1/math.tan(corner_k(a,b,Vector((b.x-1,b.y))))
    r_end=BRUN if i==1 else L/(ka+kb);Ls=r_end/math.cos(pitch);c=math.cos(pitch);u=h*math.cos(pitch)+Vector((0,0,math.sin(pitch)))
    tile_plane(G['roof'],(a.x,a.y,BR_),e3,u,Ls,(lambda t,ka=ka:ka*t*c),(lambda t,kb=kb,L=L:L-kb*t*c),M['slate'],SLATES,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],(a.x,a.y,BR_),e3,u,Ls,(lambda t,ka=ka:ka*t*c),(lambda t,kb=kb,L=L:L-kb*t*c),M['timber'])
for p,s in ((EB[1],1),(EB[2],-1)):
    tp=Vector((p.x+s*BRUN*math.tan(math.radians(22.5)),Y0,BR_+BRISE));cap(Vector((p.x,p.y,BR_+.04)),tp+Vector((0,0,.04)),.16)
# ---- centre: timber framing over the stone gable -------------------------------------------------------
y=-D2-.03
for s in (-1,1):
    post(s*(CW-.08),y,3.3,(CAPEX-(CW-.08)*KC)-.05,'y')
    rail((s*CW,y,3.3),(s*1.47,y,3.3),.14)
    rail((s*CW,y,5.95),(s*.85,y,5.95),.14)
    post(s*1.0,y,6.02,7.0,'y',.12)
rail((-.97,y,7.07),(.97,y,7.07),.14);post(0,y,7.14,RIDGE-.12,'y',.14)
for s in (-1,1):rail((0,y,7.25),(s*.45,y,7.6),.09);rail((s*(CW-.1),y,6.02),(s*1.1,y,6.9),.1)
# right wing framing
for x in (XT+TR+.1,XR-.08):post(x,y,3.3,PLATE-.02,'y')
rail((XT+TR,y,3.3),(XR,y,3.3),.14);rail((XT+TR,y,PLATE-.08),(XR,y,PLATE-.08),.16)
# ---- porch: stone gable with a pointed double door up three steps ---------------------------------------
PY,PW,PE,PR,PT=-3.75,1.25,3.3,3.95,.45           # front face, half width, wall top, roof ridge, front wall th
STEP,DWID,SPR=.45,1.15,1.85                        # door sill (= top step), door width, arch spring
KP=(PR-PE)/PW;PA=PR+.35                           # roof pitch, parapet apex
face(S,M['stone'],(0,PY,0),(0,-1,0),gable(PW,PE+.25,PA),0,PA,[lancet_hole(0,DWID,STEP,SPR)],th=PT,breaks=[PE+.25])
for s in (-1,1):S.box((s*(PW-.175),(PY+PT-D2)/2,PE/2),(.35,-D2-PY-PT,PE),M['stone'],0)
LPo=(PW+.2)*math.hypot(1,KP);ze=PE-.2*KP
for s in (-1,1):
    O=(s*(PW+.2),0,ze);u=Vector((-s*(PW+.2),0,PR-ze))
    lo,hi=((lambda t:D2,lambda t:-(PY+PT)+.05) if s<0 else (lambda t:PY+PT-.05,lambda t:-D2))
    tile_plane(G['roof'],O,(0,s,0),u.normalized(),LPo,lo,hi,M['slate'],SLATES,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,(0,s,0),u.normalized(),LPo,lo,hi,M['timber'])
capped((0,PY+PT-.05,PR+.05),(0,-D2,PR+.05),3)
P,R=frame((0,PY,0),(0,-1,0))
for s in (-1,1):                                  # dressed coping on the parapet rakes
    off=Vector((s*(PA-PE-.25),0,PW)).normalized()*.075
    T.beam(P(s*PW+off.x,-PT/2,PE+.25+off.z),P(off.x,-PT/2,PA+off.z),.15,PT+.1,M['trim'],grain=False,rect=rect())
    T.box(P(s*(PW-.1),-PT/2,PE+.18),(.32,PT+.12,.14),M['trim'],.012,R,rect=rect())
T.box(P(0,-PT/2,PA+.14),(.1,.12,.28),M['trim'],.01,R,rect=rect());T.box(P(0,-PT/2,PA+.17),(.24,.12,.08),M['trim'],.01,R,rect=rect())
for k in range(4):                                # quatrefoil roundel in the porch gable
    t=math.pi/2*k;cx,cz=.1*math.cos(t),3.82+.1*math.sin(t)
    ring(P,[(cx+.1*math.cos(math.tau*j/10),cz+.1*math.sin(math.tau*j/10)) for j in range(11)],.03,.045)
for off,d,size in ((.09,.06,.13),(.24,.05,.15)):  # two dressed orders round the door
    ring(P,arch_pts(0,DWID,SPR,off),d,size)
    for s in (-1,1):ring(P,[(s*(DWID/2+off),STEP),(s*(DWID/2+off),SPR)],d,size)
door_pts=[(-DWID/2-.02,STEP),(DWID/2+.02,STEP)]+arch_pts(0,DWID,SPR,.02)
D.prism([P(x,-PT+.12,z) for x,z in door_pts],(0,-1,0),.08,M['door'])
G['timber'].box(P(0,-PT+.17,(STEP+SPR+DWID*.866)/2-.02),(.05,.03,SPR+DWID*.866-STEP-.04),M['timber'],.004,R,grain=True,band=band())
for z in (STEP+.35,SPR-.15):
    for s in (-1,1):D.box(P(s*.32,-PT+.17,z),(.5,.012,.06),M['iron'],.004,R)
for s in (-1,1):
    for k in range(10):
        t=math.tau*k/10;D.box(P(s*.14+.06*math.cos(t),-PT+.18,1.2+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
    D.box(P(s*.14,-PT+.17,1.26),(.07,.01,.07),M['iron'],.004,R)
for k in range(3):S.box((0,PY-.35*(k+1)/2,(3-k)*.15/2),(2.3+.2*k,.35*(k+1),(3-k)*.15),M['stone'],.015)
for s in (-1,1):lantern(G,M,(s*1.06,PY-.42,2.45),arm=((s*1.06,PY-.09,2.45),(0,-1,0)))
# ---- round tower with conical slate spire ------------------------------------------------------------
lathe(S,(XT,YT,0),[(TR,0),(TR,ZC0)],M['stone'])
lathe(T,(XT,YT,0),[(TR+.06,0),(TR+.06,.45),(TR,.5)],M['trim'])
lathe(T,(XT,YT,0),[(TR,3.3),(TR+.07,3.3),(TR+.07,3.42),(TR,3.45)],M['trim'])
lathe(T,(XT,YT,0),[(TR,6.45),(TR+.1,6.45),(TR+.1,6.58),(TR,6.62)],M['trim'])
for k in range(20):
    t=math.tau*(k+.5)/20;rad=Vector((math.cos(t),math.sin(t),0));tan=Vector((-math.sin(t),math.cos(t),0))
    G['timber'].box(Vector((XT,YT,6.83))+rad*(TR+.1),(.3,.11,.4),M['timber'],.01,Matrix((rad,tan,UP)).transposed(),grain=True,band=band())
prof=[(TR,ZC0),(RC+.04,ZC0)];n=11;zs=np.linspace(ZC0,ZA-.35,n+1)
for z0,z1 in zip(zs,zs[1:]):prof+=[(cone_r(z1),z1),(cone_r(z1)+.035,z1)]
prof+=[(0,ZA)];lathe(G['roof'],(XT,YT,0),prof,M['slate'],32,(.6,.3))
D.beam((XT,YT,ZA-.15),(XT,YT,9.95),.05,.05,M['iron'],grain=False)
D.vessel((XT,YT,9.55),[(0,0),(.07,.04),(.09,.11),(.06,.18),(0,.21)],M['iron'],8)
def tower_window(t,w,lo,spring,lights=1):
    o=Vector((math.cos(t),math.sin(t),0));c=Vector((XT,YT,0))+o*(TR+.005);P,R=frame(c,o);top=spring+w*.866
    S.box(P(0,-.125,(lo-.12+top+.2)/2),(w+.34,.25,top+.2-lo+.12),M['stone'],.01,R)
    lancet(c,o,0,w,lo,spring,lights=lights,decor=True)
F=-math.pi/2
tower_window(F,.7,1.0,2.35,2);tower_window(F,.4,3.9,4.55);tower_window(F,.36,5.4,5.9)
tower_window(F+1.1,.36,5.4,5.9)
# spire dormer
zb,zea,zap,hd=7.4,7.95,8.27,.28;y0,y1=YT-cone_r(zb)-.04,YT-.2
G['plaster'].prism([(XT-hd,(y0+y1)/2,zb),(XT+hd,(y0+y1)/2,zb),(XT+hd,(y0+y1)/2,zea),(XT,(y0+y1)/2,zap),(XT-hd,(y0+y1)/2,zea)],(0,1,0),y1-y0,M['plaster'])
pd=(zap-zea)/hd;zs_=lambda u:zap+.06-abs(u-XT)*pd
for s in (-1,1):
    e=XT+s*(hd+.08);G['roof'].prism([(e,y0-.12,zs_(e)),(XT,y0-.12,zs_(XT)),(XT,y1,zs_(XT)),(e,y1,zs_(e))],(s*pd,0,1),.06,M['slate'])
    G['timber'].beam((e+s*.02,y0-.14,zs_(e)-.05),(XT-s*.05,y0-.14,zap+.1+.05*pd),.05,.14,M['timber'],band=band())
    G['timber'].box((XT+s*(hd-.04),y0-.02,(zb+zea)/2),(.07,.07,zea-zb),M['timber'],.008,grain=True,band=band())
G['timber'].box((XT,y0-.02,zb+.03),(2*hd+.06,.09,.07),M['timber'],.008,grain=True,band=band())
lancet((XT,y0,0),(0,-1,0),0,.22,7.5,7.72,decor=True,sill=False)
# ---- banners, gutters and downpipes -------------------------------------------------------------------
for s in (-1,1):banner((0,-D2,0),(0,-1,0),s*1.65,2.75,1.3,.42,d=.45)
banner((0,-D2,0),(0,-1,0),4.6,5.4,3.55,.66)
G['detail'].box((XT+RC+(EX-XT-RC)/2,-EY-.08,EAVE-.12),(EX-XT-RC,.1,.1),M['iron'],.01)
D.box((EX+.08,0,EAVE-.12),(.1,2*EY+.16,.1),M['iron'],.01)
D.box((XL-LH-.08,(LY0+LY1)/2,EAVE-.12),(.1,LY1-LY0,.1),M['iron'],.01)
D.box(((XL+LH+EX)/2,EY+.08,EAVE-.12),(EX-XL-LH,.1,.1),M['iron'],.01)
pipe([(EX+.08,-2.3,EAVE-.12),(EX+.08,-2.3,5.3),(XR+.08,-2.3,4.95),(XR+.08,-2.3,.05)])
pipe([(XL-LH-.08,-2.3,EAVE-.12),(XL-LH-.08,-2.3,5.3),(XL-LW-.08,-2.3,4.95),(XL-LW-.08,-2.3,.05)])
for x in (-CW-.1,XT+TR+.12):pipe([(x,-D2-.08,PLATE-.1),(x,-D2-.08,.05)])
for z in (1.0,2.6,4.6):
    for x in (-CW-.1,XT+TR+.12):D.box((x,-D2-.03,z),(.1,.08,.05),M['iron'],.004)
    for x in (XR+.05,XL-LW-.05):D.box((x,-2.3,z),(.1,.1,.05),M['iron'],.004)
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'not grounded: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookNorthridgeHouse1'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-13.5,-17.5,6.2),(80,0,-37),lens=36);cam('Front',(0,-30,5.0),(90,0,0),12.6);cam('Side',(-30,0,5.0),(90,0,-90),12.6);cam('Rear',(0,30,5.0),(90,0,180),12.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'NorthridgeHouse1-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120_000,f'house triangle budget: {tris}'
manifest=dict(asset='SilverbrookNorthridgeHouse1',asset_id='BLD-GreenlandsNorthridgeHouse-01',variant=1,reference=dict(file='NorthridgeHouse1-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_northridgehouse1.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookNorthridgeHouse1',slot_m=list(SLOT),outputs=['SilverbrookNorthridgeHouse1.blend','SilverbrookNorthridgeHouse1.glb','SilverbrookNorthridgeHouse1.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('NORTHRIDGEHOUSE1_BUILT',manifest['bounds_m'],tris)
