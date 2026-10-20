"""Silverbrook Northridge House 2 from docs/Game-Design/concept-art/Silverbrook/Northridge House-2.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_northridgehouse2.py
Original procedural geometry interpreting the elevation (drawn 14 m wide, 14 m to the spire
tips; scaled uniformly by ~0.68 into the 12 x 10 x 10 m slot): a pale squared-stone house with
two round corner towers under slate cones, a central half-timbered gable over a tall pointed
tracery window, two smaller timber gables over paired lancets, slate-roofed canted bay windows,
a stone gabled porch with a pointed arch over a double plank door, end stacks, hipped slate main
roof, stag banners and lanterns. Asset BLD-GreenlandsNorthridgeHouse-01, variant 2.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,UP,tile_plane,deck,lantern,frame,wall
import bpy
import numpy as np
scene=kit.reset(4212)
NAME='SilverbrookNorthridgeHouse2'
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Northridge House-2.png'
asset=bpy.data.collections.new(NAME);scene.collection.children.link(asset)

def _vol(geo,start):
    """Signed volume of the faces added since `start`, relative to their first vertex (float-safe)."""
    faces=geo.f[start:];o=geo.v[faces[0][0]].copy();v=0
    for f in faces:
        p=[geo.v[i]-o for i in f];v+=sum(p[0].dot(p[j].cross(p[j+1])) for j in range(1,len(p)-1))
    return v
_box,_prism=Geo.box,Geo.prism
def _box_checked(self,centre,size,mat,*a,**kw):
    # kit's check sums float32 triple products of world points; re-check a failing box at the origin.
    try:_box(self,centre,size,mat,*a,**kw)
    except AssertionError:_box(Geo('winding-check',None),(0,0,0),size,mat,*a,**kw)
def _prism_checked(self,*a,**kw):
    s=len(self.f);_prism(self,*a,**kw);assert _vol(self,s)>0,'inside-out prism'
Geo.box,Geo.prism=_box_checked,_prism_checked

M=dict(  # SB_* rows repeat Cottage1's arguments exactly; package-only surfaces are SB_NorthridgeHouse2_*
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_NorthridgeHouse2_Ashlar','sandstone_blocks_05',dark=(.45,.40,.33)),
    trim=kit.scan('SB_NorthridgeHouse2_Dressed','sandstone_blocks_08',dark=(.45,.40,.33)),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_NorthridgeHouse2_Banner','denim_fabric_03',dark=(.16,.19,.34),normal=.6,damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_NorthridgeHouse2_Gilt',(.55,.36,.10),.45,.8),
    emblem=kit.solid('SB_NorthridgeHouse2_Emblem',(.72,.66,.55),.7))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('sandstone_blocks_05',(.06,.035),dark=.7,frac=.01,std=.22,bright=1.0)
DRESSED=kit.patches('sandstone_blocks_08',(.05,.035),dark=.7,frac=.01,std=.22)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
def rect():return kit.rng.choice(DRESSED)
G=dict(stone=Geo('SM_SBNorthridgeHouse2_Masonry',asset),trim=Geo('SM_SBNorthridgeHouse2_Trim',asset),plaster=Geo('SM_SBNorthridgeHouse2_Plaster',asset),
       timber=Geo('SM_SBNorthridgeHouse2_Timber',asset),roof=Geo('SM_SBNorthridgeHouse2_Roof',asset),detail=Geo('SM_SBNorthridgeHouse2_Details',asset))

# ---- measured layout (metres; concept ~95 px/m after scaling into the slot) ------------------
SLOT=(12,10,10)                # concept README 'Fits' w x d x h
W2,YF,YR,TH=4.6,-3.0,3.0,.45   # main block half width, front and rear faces, wall thickness
PLATE=5.3                      # wall plate
EAVE,RIDGE,EX,EY,RX=5.1,7.9,4.9,3.3,2.6
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
GY,GE=YF-.3,5.25               # front gables: roof front edge, eave
CG=(0,1.65,RIDGE)              # central gable: x, roof half, apex (= main ridge)
SG=[(s*2.55,.9,7.35) for s in (-1,1)]   # side gables over the bays
TX,TY,TR,TN=4.3,YF,.8,10       # corner towers: |x|, y, circumradius, sides
T1,RC,TC,TA=6.4,1.05,6.78,8.75 # tower wall top, cone eave radius and height, apex
XC,YC=4.9,1.0                  # end stacks (outside the side walls)
STEP=.36                       # porch floor / door sill
PF=YF-.9                       # porch front face
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gz(g,x):return g[2]-abs(x-g[0])*(g[2]-GE)/g[1]
GABLES=[CG]+SG
def in_gable_of(g):return lambda p:abs(p.x-g[0])<g[1]+.04 and p.y<0 and p.z<gz(g,p.x)-.005
def in_turret(p):
    r=math.hypot(abs(p.x)-TX,p.y-TY);return r<RC+.02 and p.z<TA-(TA-TC)*r/RC
in_stack=lambda p:abs(abs(p.x)-XC)<.36 and abs(p.y-YC)<.5
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/2.5) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
BIG=dict(size=(.38,.52,.024),exposure=.25)
def plane(O,e,toward,L,lo,hi,covered,**kw):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8),**kw)
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
def post(x,y,z0,z1,o,w=.2):G['timber'].box((x,y,(z0+z1)/2),(w if o=='y' else .12,.12 if o=='y' else w,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
def pipe(a,b,s=.08):G['detail'].beam(a,b,s,s,M['iron'],grain=False)

# ---- helpers: pointed openings in coursed walls (after TempleOfTheDawn), frusta, leaded lights
def pointed(w,spring):return lambda z:w/2 if z<=spring else math.sqrt(max(0,w*w-(z-spring)**2))-w/2
def lancet_hole(a,w,lo,spring):return (a,pointed(w,spring),lo,spring+w*math.sqrt(3)/2,spring)
def face(geo,mat,c,o,outline,z0,z1,holes=(),th=.45,breaks=()):
    """Wall slab (outer face through c, z absolute) cut into courses of trapezoid prisms between
    the outline (z -> (left, right)) and pointed holes (a, hw(z), lo, hi, curve_from)."""
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
    """Pointed (equilateral) arch offset outward by `off`: right spring -> apex -> left spring."""
    R_=w+off;ta=math.acos(min(1,(w/2)/R_));right=[(a-w/2+R_*math.cos(t),spring+R_*math.sin(t)) for t in [ta*i/n for i in range(n+1)]]
    return right+[(2*a-x,z) for x,z in reversed(right[:-1])]
def ring(geo,P,pts,d,size,mat,**kw):
    kw.setdefault('grain',False)
    for (x0,z0),(x1,z1) in zip(pts,pts[1:]):
        a,b=P(x0,d,z0),P(x1,d,z1);ext=(b-a).normalized()*.02;geo.beam(a-ext,b+ext,size,size,mat,**kw)
def tkw(t):return dict(grain=True,band=band()) if t=='timber' else dict(rect=rect())
def lancet(c,o,a,w,lo,spring,th,t='trim',sill=True):
    """Leaded lancet in a hole cut by face(): glass, cames, hood ring and jambs, sill."""
    P,R=frame(c,o);top=spring+w*math.sqrt(3)/2;hw=pointed(w,spring);dg=-th*.45
    G['detail'].prism([P(x,dg,z) for x,z in [(a-w/2-.02,lo),(a+w/2+.02,lo)]+arch_pts(a,w,spring,.02)],o,.03,M['glass'])
    dl=dg+.022;z=lo+.22
    while z<top-.1:
        h=hw(z);G['detail'].beam(P(a-h,dl,z),P(a+h,dl,z),.014,.014,M['lead'],grain=False);z+=.22
    for x in ((-w/4,w/4) if w>.4 else (0,)):
        zt=spring+math.sqrt(max(0,w*w-(abs(x)+w/2)**2));G['detail'].beam(P(a+x,dl,lo),P(a+x,dl,zt),.014,.014,M['lead'],grain=False)
    geo=G['timber'] if t=='timber' else G['trim'];sz=.1 if w>.4 else .08
    ring(geo,P,arch_pts(a,w,spring,sz*.8),.04,sz,M[t],**tkw(t))
    for s in (-1,1):ring(geo,P,[(a+s*(w/2+sz*.8),lo),(a+s*(w/2+sz*.8),spring)],.04,sz,M[t],**tkw(t))
    if sill:geo.box(P(a,(-th+.1)/2,lo-.05),(w+2*sz+.08,th+.1,.1),M[t],.012,R,**tkw(t))
def frustum(geo,cx,cy,z0,r0,z1,r1,mat,phase,n=TN,**kw):
    """Closed n-gon frustum (both radii > 0), outward winding asserted."""
    rg=lambda z,r:[Vector((cx+r*math.cos(phase+math.tau*k/n),cy+r*math.sin(phase+math.tau*k/n),z)) for k in range(n)]
    b,t=rg(z0,r0),rg(z1,r1);s=len(geo.f)
    for k in range(n):geo.poly([b[k],b[(k+1)%n],t[(k+1)%n],t[k]],mat,**kw)
    geo.poly(t,mat,**kw);geo.poly(b[::-1],mat,**kw);assert _vol(geo,s)>0,'inside-out frustum'
def lights(c,o,w,h,n,wall_th=.2,planter=False):
    """Leaded window of n lights with oak frame, mullions, sill and diamond quarries."""
    P,R=frame(c,o);t=.09;fd=wall_th+.04;gd=-min(.14,wall_th-.04)
    def B(geo,a,d,z,size,mat,bevel=.008,**kw):geo.box(P(a,d,z),size,mat,bevel,R,**kw)
    for a in (-(w/2+t/2),w/2+t/2):B(G['timber'],a,-wall_th/2+.02,h/2,(t,fd,h+2*t),M['timber'],grain=True,band=band())
    for z in (h+t/2,-t/2):B(G['timber'],0,-wall_th/2+.02,z,(w,fd,t),M['timber'],grain=True,band=band())
    pw=(w-.06*(n-1))/n
    for i in range(1,n):B(G['timber'],-w/2+i*(pw+.06)-.03,gd+.05,h/2,(.06,.12,h),M['timber'],grain=True,band=band())
    for i in range(n):
        ac=-w/2+pw/2+i*(pw+.06);B(G['detail'],ac,gd,h/2,(pw,.01,h),M['glass'],0)
        lo,hi=ac-pw/2,ac+pw/2;slope=1.7;step=.10*slope
        for sign in (1,-1):
            for k in range(-30,31):
                b=k*step;pts=[]
                for a in (lo,hi):
                    z=sign*slope*(a-ac)+b
                    if 0<=z<=h:pts.append((a,z))
                for z in (0,h):
                    a=ac+(z-b)/(sign*slope)
                    if lo<a<hi:pts.append((a,z))
                if len(pts)>=2:
                    (a0,z0),(a1,z1)=pts[0],pts[1]
                    if math.hypot(a1-a0,z1-z0)>.02:G['detail'].beam(P(a0,gd+.012,z0),P(a1,gd+.012,z1),.009,.012,M['lead'],grain=False)
    B(G['timber'],0,.05,-t-.025,(w+2*t+.12,.14,.05),M['timber'],grain=True,band=band())
    if planter:
        B(G['timber'],0,.15,-t-.2,(w+2*t+.1,.24,.2),M['planks'],.006)
        B(G['detail'],0,.15,-t-.095,(w+2*t+.04,.19,.02),M['soil'],0)
        for a in (-w/2,w/2):G['timber'].beam(P(a,.03,-t-.55),P(a,.25,-t-.3),.06,.06,M['timber'],band=band())

# ---- main hip roof --------------------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
front_cover=[in_gable_of(g) for g in GABLES]+[in_turret,in_stack]
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),front_cover,**BIG)
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_stack],**BIG)
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_turret,in_stack],**BIG)
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_turret,in_stack],**BIG)
capped((-RX,0,RIDGE+.05),(RX,0,RIDGE+.05),10)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.05),(sx*RX,0,RIDGE+.05),12)
G['timber'].beam((-EX,EY+.02,EAVE-.06),(EX,EY+.02,EAVE-.06),.04,.2,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY+1.0,EAVE-.06),(sx*(EX+.02),EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- front gables: roofs, bargeboards with pendants, finials -----------------------------------
for g in GABLES:
    xg,gh,ga=g;kg=(ga-GE)/gh;L=math.hypot(gh,ga-GE);cov=[under_main]+([in_turret] if xg else [])
    plane((xg-gh,0,GE),(0,-1,0),(gh,0,ga-GE),L,lambda t:0,lambda t:-GY,cov,**BIG)
    plane((xg+gh,0,GE),(0,1,0),(-gh,0,ga-GE),L,lambda t:GY,lambda t:0,cov,**BIG)
    yend=-(RIDGE-ga)/KF-.15 if ga<RIDGE else -.4
    capped((xg,GY,ga+.05),(xg,yend,ga+.05),max(3,round((yend-GY)/.35)))
    for s in (-1,1):
        G['timber'].beam((xg+s*(gh+.05),GY-.04,GE-.08),(xg-s*.16,GY-.04,ga+.16*kg+.03),.08,.26,M['timber'],band=band())
        G['timber'].box((xg+s*(gh-.02),GY-.04,GE-.2),(.12,.12,.36),M['timber'],.02,grain=True,band=band())   # pendant
    G['timber'].box((xg,GY-.04,ga+.12),(.15,.15,.3),M['timber'],.02,grain=True,band=band())
    G['timber'].box((xg,GY-.04,ga+.29),(.21,.21,.05),M['timber'],.02,grain=True,band=band())
    top=ga+(.85 if xg==0 else .7)
    pipe((xg,GY-.04,ga+.3),(xg,GY-.04,top),.05)
    for dz,s_ in ((.12,.13),(.36,.09)):G['detail'].box((xg,GY-.04,ga+.3+dz),(s_,s_,.06),M['iron'],.01)
    for s in (-1,1):pipe((xg,GY-.04,top-.25),(xg+s*.12,GY-.04,top-.1),.03)   # fleur-de-lis arms
# ---- front wall (squared stone) ------------------------------------------------------------
DW,DS=1.3,STEP+1.35            # door width, spring (absolute)
CWZ,CWS,CW=4.15,5.15,1.0       # central tracery window: sill, spring, width
UZ,US,UW=3.65,4.3,.32          # paired lancets over the bays
front_holes=[lancet_hole(0,DW,STEP,DS),lancet_hole(0,CW,CWZ,CWS)]+[lancet_hole(s*1.0,.3,CWZ,4.7) for s in (-1,1)]
for s in (-1,1):front_holes+=[lancet_hole(s*2.55+d,UW,UZ,US) for d in (-.32,.32)]+[lancet_hole(s*1.62,.22,UZ,US)]
face(G['stone'],M['stone'],(0,YF,0),(0,-1,0),lambda z:(-W2,W2),0,PLATE,front_holes,TH)
# central gable: stone up to the bressumer, then plaster with timber framing
CB=6.35;kc=(CG[2]-GE)/CG[1]
def cgx(z):return (CG[2]-.05-z)/kc
face(G['stone'],M['stone'],(0,YF,0),(0,-1,0),lambda z:(-cgx(z),cgx(z)),PLATE,CB,front_holes,TH)
face(G['plaster'],M['plaster'],(0,YF,0),(0,-1,0),lambda z:(-cgx(z),cgx(z)),CB+.1,CG[2]-.05,(),.25)
y=YF-.03
G['timber'].box((0,y-.02,CB+.05),(2*cgx(CB)+.1,.16,.2),M['timber'],.015,grain=True,band=band())
post(0,y,CB+.15,CG[2]-.1,'y',.16)
for s in (-1,1):
    post(s*.5,y,CB+.15,CG[2]-.05-.5*kc-.04,'y',.14)
    rail((s*.08,y,CB+.25),(s*.42,y,CB+.75),.1)
rail((-cgx(7.15)+.04,y,7.15),(cgx(7.15)-.04,y,7.15),.12)
for x in (-1.1,-.45,.45,1.1):   # carved brackets under the bressumer
    G['timber'].prism([(x,YF+.02,CB-.05),(x,YF+.02,CB-.45),(x,YF-.2,CB-.05)],(1,0,0),.12,M['timber'])
# side gables: plaster and timber over a stone wall, a pair of timber-framed lancets
for g in SG:
    xg,gh,ga=g;kg=(ga-GE)/gh;sgx=lambda z,ga=ga,kg=kg:(ga-.05-z)/kg
    holes=[lancet_hole(d,.22,5.45,5.85) for d in (-.17,.17)]
    face(G['plaster'],M['plaster'],(xg,YF,0),(0,-1,0),lambda z,f=sgx:(-f(z),f(z)),PLATE+.1,ga-.05,holes,.25)
    for d in (-.17,.17):lancet((xg,YF,0),(0,-1,0),d,.22,5.45,5.85,.25,'timber')
    G['timber'].box((xg,y-.02,PLATE+.05),(2*sgx(PLATE)+.14,.16,.2),M['timber'],.015,grain=True,band=band())
    for s in (-1,1):
        post(xg+s*.45,y,PLATE+.15,ga-.05-.51*kg-.02,'y',.12)
        G['timber'].prism([(xg+s*.85,YF+.02,PLATE-.05),(xg+s*.85,YF+.02,PLATE-.5),(xg+s*.85,YF-.24,PLATE-.05)],(1,0,0),.14,M['timber'])
    rail((xg-sgx(6.4)+.04,y,6.4),(xg+sgx(6.4)-.04,y,6.4),.12);post(xg,y,6.46,ga-.12,'y',.12)
# openings in the front wall
lancet((0,YF,0),(0,-1,0),0,CW,CWZ,CWS,TH)
P,R=frame((0,YF,0),(0,-1,0))
G['trim'].box(P(0,-TH*.45+.02,(CWZ+CWS+.15)/2),(.07,.06,CWS+.15-CWZ),M['trim'],.01,R,rect=rect())   # mullion
q=CWS+.42                                                                                     # quatrefoil in the head
for dx,dz in ((.11,0),(-.11,0),(0,.11),(0,-.11)):
    ring(G['trim'],P,[(dx+.1*math.cos(t),q+dz+.1*math.sin(t)) for t in np.linspace(0,math.tau,9)],-TH*.45+.02,.03,M['trim'],rect=rect())
for s in (-1,1):
    lancet((0,YF,0),(0,-1,0),s*1.0,.3,CWZ,4.7,TH)
    lancet((0,YF,0),(0,-1,0),s*1.62,.22,UZ,US,TH)
    for d in (-.32,.32):lancet((0,YF,0),(0,-1,0),s*2.55+d,UW,UZ,US,TH)
    G['timber'].box((s*2.55,YF-.17,UZ-.32),(1.45,.24,.2),M['planks'],.006)        # window box
    G['detail'].box((s*2.55,YF-.17,UZ-.215),(1.39,.19,.02),M['soil'],0)
    for x in (s*2.55-.6,s*2.55+.6):G['timber'].beam((x,YF-.02,UZ-.55),(x,YF-.25,UZ-.42),.06,.06,M['timber'],band=band())
# door: dressed two-order pointed surround, double plank leaves, straps and rings
for off,d in ((.08,.02),(.22,.06)):ring(G['trim'],P,arch_pts(0,DW,DS,off),d,.12,M['trim'],rect=rect())
for s in (-1,1):
    for off,d in ((.08,.02),(.22,.06)):ring(G['trim'],P,[(s*(DW/2+off),STEP),(s*(DW/2+off),DS)],d,.12,M['trim'],rect=rect())
# Each leaf is its own mesh so it can open: inner face 5 mm short of the wall's inner face and 5 mm inside the jamb,
# so the pointed head swings into the house clear of the soffit.
DL=[Geo(f'SM_SBNorthridgeHouse2_Door{k}',asset) for k in (1,2)];G['door1'],G['door2']=DL;LD=-TH+.04
half=[(0,STEP),(DW/2-.005,STEP)]+arch_pts(0,DW,DS,-.005)[:9]   # right spring -> apex; mirrored for the left leaf
for L,s in zip(DL,(-1,1)):
    L.prism([P(s*a,LD,z) for a,z in half],(0,-1,0),.07,M['door'])
    L.box(P(s*.01,LD+.045,(STEP+DS+DW*.866)/2),(.02,.014,DS+DW*.866-STEP-.02),M['iron'],.004,R)   # half the meeting strap
for z in (STEP+.4,DS-.15):
    for s in (-1,1):DL[s>0].box(P(s*DW*.27,LD+.045,z),(DW*.4,.012,.06),M['iron'],.004,R)
for s in (-1,1):
    for k in range(10):
        t=math.tau*k/10;DL[s>0].box(P(s*.15+.07*math.cos(t),LD+.055,1.25+.07*math.sin(t)),(.045,.014,.014),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
    DL[s>0].box(P(s*.15,LD+.05,1.32),(.07,.012,.07),M['iron'],.004,R)
# ---- porch: stone gable front with a pointed arch, side walls, slate roof, steps ---------------
PW2,PE,PA=1.15,3.16,4.0        # half width, gable eave line, apex of the coped gable face
kp=(PA-PE)/PW2
face(G['stone'],M['stone'],(0,PF,0),(0,-1,0),lambda z:(-PW2,PW2) if z<=PE else (-(PA-z)/kp,(PA-z)/kp),0,PA,[lancet_hole(0,1.4,STEP,1.8)],.3,[PE])
P2,R2=frame((0,PF,0),(0,-1,0))
for off,d in ((.07,.02),(.18,.05)):ring(G['trim'],P2,arch_pts(0,1.4,1.8,off),d,.1,M['trim'],rect=rect())
for s in (-1,1):
    for off,d in ((.07,.02),(.18,.05)):ring(G['trim'],P2,[(s*(.7+off),STEP),(s*(.7+off),1.8)],d,.1,M['trim'],rect=rect())
    off=Vector((s*(PA-PE),0,PW2)).normalized()*.06
    G['trim'].beam(P2(s*PW2+off.x,-.15,PE+off.z),P2(off.x,-.15,PA+off.z),.12,.36,M['trim'],grain=False,rect=rect())   # coping
for dx,dz in ((.09,0),(-.09,0),(0,.09),(0,-.09)):
    ring(G['trim'],P2,[(dx+.085*math.cos(t),3.42+dz+.085*math.sin(t)) for t in np.linspace(0,math.tau,9)],.01,.035,M['trim'],rect=rect())
G['trim'].box((0,PF-.04,PA+.12),(.14,.14,.26),M['trim'],.01,rect=rect())
G['trim'].box((0,PF-.04,PA+.3),(.08,.08,.1),M['trim'],.01,rect=rect())
for s in (-1,1):G['stone'].box((s*1.025,(PF+.3+YF)/2,1.5),(.25,YF-PF-.3,3.0),M['stone'],.01)
G['stone'].box((0,(PF+YF)/2,STEP/2),(1.8,YF-PF,STEP),M['stone'],.01)
LP=math.hypot(1.3,.95);PR=PA-.15
plane((-1.3,0,PR-.95),(0,-1,0),(1.3,0,.95),LP,lambda t:-YF,lambda t:-PF-.28,[],size=(.24,.34,.02),exposure=.16)
plane((1.3,0,PR-.95),(0,1,0),(-1.3,0,.95),LP,lambda t:PF+.28,lambda t:YF,[],size=(.24,.34,.02),exposure=.16)
capped((0,PF+.28,PR+.04),(0,YF,PR+.04),3)
for k in range(3):G['stone'].box((0,PF-.14-.3*k,(3-k)*.06),(1.9,.3,(3-k)*.12),M['stone'],.012)
lantern(G,M,(0,(PF+YF)/2,2.95));pipe((0,(PF+YF)/2,3.05),(0,(PF+YF)/2,PR-.05),.025)
for s in (-1,1):lantern(G,M,(s*.98,PF-.42,2.2),arm=((s*.98,PF-.02,2.2),(0,-1,0)))
# ---- canted bay windows under the side gables -------------------------------------------------
BC,BP,OV,BZ0,BW,BH,BTOP,BE,KB=.55,.4,.12,.75,.92,1.2,2.55,2.6,.9
T=math.tan(math.radians(22.5));SQ=math.sqrt(1+KB*KB);LB=(BP+OV)*SQ;SL=(BP+OV)*math.sqrt(2)
for xb in (-2.55,2.55):
    V=[Vector(v) for v in ((xb-BC-BP,YF,0),(xb-BC,YF-BP,0),(xb+BC,YF-BP,0),(xb+BC+BP,YF,0))]
    G['stone'].prism([p+Vector((0,0,BZ0/2)) for p in V],(0,0,1),BZ0,M['stone'])
    for i in range(3):
        A,B=V[i],V[i+1];mid=(A+B)/2;d=(B-A).normalized();o=Vector((d.y,-d.x,0));half=(B-A).length/2
        ww=.82 if i==1 else .3
        wall(G['timber'],M['timber'],mid+Vector((0,0,BZ0)),o,half,BTOP-BZ0,[(0,ww+.18,BW-.09-BZ0,BW+BH+.09-BZ0)],th=.18,grain=True,band=band())
        lights(mid+Vector((0,0,BW)),o,ww,BH,3 if i==1 else 1,.18,planter=i==1)
        Pb,Rb=frame(mid,o);G['timber'].box(Pb(0,.03,BTOP-.06),(2*half+.06,.08,.14),M['timber'],.01,Rb,grain=True,band=band())
    for v,ang in zip(V,(45,22.5,-22.5,-45)):
        G['timber'].box(v+Vector((0,0,(BZ0+BTOP)/2)),(.14,.14,BTOP-BZ0),M['timber'],.012,Matrix.Rotation(math.radians(ang),3,'Z'),grain=True,band=band())
    def h(t):return t/SQ
    FL,FR=Vector((xb-BC-OV*T,YF-BP-OV,BE)),Vector((xb+BC+OV*T,YF-BP-OV,BE));QL=FL+Vector((-(BP+OV),BP+OV,0))
    def bay_plane(O,e,m,lo,hi):
        u=Vector((m[0],m[1],KB)).normalized();tile_plane(G['roof'],O,e,u,LB,lo,hi,M['slate'],SLATES,lichen=lichen,tone=(.35,.8),size=(.22,.32,.02),exposure=.15)
        deck(G['roof'],O,e,u,LB,lo,hi,M['timber'],cell=.3)
    bay_plane(Vector((xb,FL.y,BE)),(1,0,0),(0,1),lambda t:-(BC+OV*T)+T*h(t),lambda t:(BC+OV*T)-T*h(t))
    r2=math.sqrt(.5)
    bay_plane(QL,(r2,-r2,0),(r2,r2),lambda t:h(t),lambda t:SL-T*h(t))
    bay_plane(FR,(r2,r2,0),(-r2,r2),lambda t:T*h(t),lambda t:SL-h(t))
    ZW=BE+(BP+OV)*KB;XH=BC+OV*T-T*(BP+OV)
    for F,x in ((FL,xb-XH),(FR,xb+XH)):capped(F+Vector((0,0,.04)),(x,YF-.02,ZW+.04),3)
    G['timber'].beam((xb-XH-.1,YF-.04,ZW-.02),(xb+XH+.1,YF-.04,ZW-.02),.06,.1,M['timber'],band=band())
# ---- corner towers: ten-sided stone drums, lancets, corbel table, slate cones --------------------
PH=-math.pi/2-math.pi/TN;AP=TR*math.cos(math.pi/TN);HS=TR*math.sin(math.pi/TN)
for sx in (-1,1):
    cx=sx*TX
    for k in range(TN):
        phi=-math.pi/2+math.tau*k/TN;o=Vector((math.cos(phi),math.sin(phi),0));c=Vector((cx,TY,0))+o*AP
        win=k==0 or k==(1 if sx>0 else TN-1)   # front and front-outer faces, clear of the hip eave
        holes=[lancet_hole(0,.26,3.2,3.75),lancet_hole(0,.26,4.9,5.5)] if win else []
        face(G['stone'],M['stone'],c,o,lambda z:(-HS,HS),0,T1,holes,.3)
        if win:
            lancet(c,o,0,.26,3.2,3.75,.3);lancet(c,o,0,.26,4.9,5.5,.3)
        Pk,Rk=frame(c,o)
        G['trim'].box(Pk(0,.07,6.15),(.14,.14,.3),M['trim'],.01,Rk,rect=rect())   # corbel
    frustum(G['trim'],cx,TY,0,TR+.17,.75,TR+.03,M['trim'],PH)                    # battered plinth
    frustum(G['trim'],cx,TY,2.85,TR+.06,2.97,TR+.06,M['trim'],PH)                 # string course
    frustum(G['trim'],cx,TY,6.3,TR+.02,6.42,TR+.14,M['trim'],PH)
    frustum(G['stone'],cx,TY,6.42,TR+.14,TC,TR+.14,M['stone'],PH)
    rv=RC/math.cos(math.pi/TN);apex=Vector((cx,TY,TA))
    vert=lambda k,z:Vector((cx+rv*math.cos(PH+math.tau*k/TN),TY+rv*math.sin(PH+math.tau*k/TN),z))
    frustum(G['timber'],cx,TY,TC-.04,TR+.15,TC+.02,rv,M['timber'],PH)
    for k in range(TN):
        a,b=vert(k,TC+.02),vert(k+1,TC+.02);O=(a+b)/2;L=(apex-O).length;hb=(b-a).length/2
        plane(O,b-a,apex-O,L,(lambda hb,L:lambda t:-hb*(1-t/L))(hb,L),(lambda hb,L:lambda t:hb*(1-t/L))(hb,L),[],size=(.24,.33,.02),exposure=.16)
        capped(a+UP*.04,apex+UP*.02,5)
    G['detail'].vessel((cx,TY,TA-.12),[(.09,0),(.06,.14),(.1,.24),(.06,.32),(.03,.66),(.045,.7),(.02,.74),(0,.9)],M['iron'],12)
# ---- rear and side walls ----------------------------------------------------------------------
face(G['stone'],M['stone'],(0,YR,0),(0,1,0),lambda z:(-W2,W2),0,PLATE,[lancet_hole(a,.42,z0,z0+.75) for a in (-3,-1,1,3) for z0 in (1.0,3.5)],TH)
for a in (-3,-1,1,3):
    for z0 in (1.0,3.5):lancet((0,YR,0),(0,1,0),a,.42,z0,z0+.75,TH)
for s in (-1,1):
    ws=[a*s for a in (-1.0,2.2)]
    face(G['stone'],M['stone'],(s*W2,0,0),(s,0,0),lambda z:(YF,YR),0,PLATE,[lancet_hole(a,.42,z0,z0+.75) for a in ws for z0 in (1.0,3.5)],TH)
    for a in ws:
        for z0 in (1.0,3.5):lancet((s*W2,0,0),(s,0,0),a,.42,z0,z0+.75,TH)
    G['timber'].box((s*(W2-.15),0,PLATE+.05),(.3,2*YR-1.2,.1),M['timber'],.01,grain=True,band=band())   # wall plate under the hip
    G['stone'].box((s*(W2+.02),0,.22),(.1,2*YR,.44),M['stone'],.01)
G['stone'].box((0,YR+.02,.22),(2*W2+.08,.1,.44),M['stone'],.01)
# ---- end stacks, downpipes ----------------------------------------------------------------------
for s in (-1,1):
    x=s*XC;G['stone'].box((x,YC,3.8),(.6,.9,7.6),M['stone'],.01)
    G['stone'].box((x,YC,7.0),(.68,.98,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
    G['stone'].box((x,YC,7.66),(.72,1.02,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
    for dy in (-.22,.22):G['detail'].vessel((x,YC+dy,7.72),[(.1,0),(.095,.3),(.105,.38),(.12,.42),(.09,.42),(.078,.36),(0,.36)],M['pot'])
    px=s*(W2+.06);pipe((px,-1.9,.06),(px,-1.9,5.3));pipe((px,-1.9,.1),(px+s*.15,-1.9,.06))
    for z in (1.0,2.4,3.8,5.0):G['detail'].box((s*(W2+.03),-1.9,z),(.07,.03,.04),M['iron'],.004)
# ---- stag banners on iron brackets either side of the porch ----------------------------------------
def stag(geo,origin,right,up,normal,s):
    o=Vector(origin)+Vector(normal)*.008;R_,U_,N_=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+R_*(a*s)+U_*(z*s)
    geo.prism([p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N_,.02,M['emblem'])
    for sx in (-1,1):
        geo.prism([p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N_,.02,M['emblem'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['emblem'],grain=False)
AZ,BY=2.85,YF-.32
for s in (-1,1):
    bx=s*1.43
    pipe((bx,YF+.02,AZ),(bx,BY-.04,AZ),.045);pipe((bx,YF+.02,AZ-.45),(bx,BY+.1,AZ-.02),.03)
    G['detail'].box((bx,YF-.02,AZ-.2),(.1,.05,.55),M['iron'],.004)
    pipe((bx-.27,BY,AZ-.06),(bx+.27,BY,AZ-.06),.03)
    bb=[(bx-.22,AZ-.1),(bx+.22,AZ-.1),(bx+.22,AZ-1.3),(bx,AZ-1.46),(bx-.22,AZ-1.3)]
    G['detail'].prism([(x,BY,z) for x,z in bb],(0,1,0),.02,M['banner'])
    for x in (bx-.18,bx+.18):pipe((x,BY,AZ-.06),(x,BY,AZ-.1),.012)
    for n in (-1,1):
        pts=[(x,BY+n*.01,z) for x,z in bb]
        for a,b in zip(pts,pts[1:]+pts[:1]):G['detail'].beam(Vector(a)+Vector((0,n*.008,0)),Vector(b)+Vector((0,n*.008,0)),.02,.03,M['gilt'],grain=False)
        stag(G['detail'],(bx,BY+n*.01,AZ-.75),(-n,0,0),(0,0,1),(0,n,0),.6)

# ---- export, preview scene, manifest -------------------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,-lo[2])))
assert abs(lo[2])<.02,f'model does not stand on the ground: min z {lo[2]}'
assert all(h_-l<=s+1e-3 for l,h_,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h_-l,3) for l,h_ in zip(lo,hi)]}'
OUT=HERE/NAME
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
# Textures stay in ../Textures (FBX references them relatively); embedding 2k scans makes a huge GLB.
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
# Door leaves: own meshes, hinged at the outer jamb on the inner (+Y) face; each opens 90 deg inward (+Y, into the house).
from mathutils.bvhtree import BVHTree
def bvh(o,vs=None):return BVHTree.FromPolygons(vs or [o.matrix_world@v.co for v in o.data.vertices],[p.vertices[:] for p in o.data.polygons])
LEAVES=[]
for k,s in ((1,-1),(2,1)):                         # Door1 left leaf, Door2 right leaf (seen from the street)
    leaf=bpy.data.objects[f'SM_SBNorthridgeHouse2_Door{k}'];vs=[leaf.matrix_world@v.co for v in leaf.data.vertices];tr=-90*s
    piv=Vector((s*max(s*v.x for v in vs),max(v.y for v in vs),min(v.z for v in vs)));rot=Matrix.Rotation(math.radians(tr),3,'Z')
    sw=[piv+rot@(v-piv) for v in vs]
    assert min(v.y for v in sw)>piv.y-1e-3 and max(v.y for v in sw)>piv.y+.4,f'Door{k} does not swing inward'
    hits={o.name:len({b for _,b in bvh(leaf,sw).overlap(bvh(o))}-{b for _,b in bvh(leaf,vs).overlap(bvh(o))}) for o in objs if 'Door' not in o.name}
    print(f'DOOR{k}_SWING pivot',[round(c,3) for c in piv],'travel',tr,'open y %.3f..%.3f'%(min(v.y for v in sw),max(v.y for v in sw)),'new contacts',{n:c for n,c in hits.items() if c})
    assert not any(hits.values()),f'Door{k} swings through {hits}'
    LEAVES.append((leaf,f'Door{k}',tuple(piv),(0,0,1),'swing',tr,'closed'))
PARTS=kit.export_parts(LEAVES,str(OUT),'SM_SilverbrookNorthridgeHouse2')
for leaf,*_ in LEAVES:leaf.select_set(False)
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-14.5,-19,6.4),(80,0,-37),lens=34);cam('Front',(0,-30,4.9),(90,0,0),11.8);cam('Side',(-30,0,4.9),(90,0,-90),11.8);cam('Rear',(0,30,4.9),(90,0,180),11.8)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'NorthridgeHouse2-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'house triangle budget exceeded: {tris} {[(o.name,len(o.data.polygons)) for o in objs]}'
manifest=dict(asset=NAME,reference=dict(file='NorthridgeHouse2-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_northridgehouse2.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookNorthridgeHouse2',parts=PARTS,slot_m=list(SLOT),outputs=[f'{NAME}.blend',f'{NAME}.glb',f'{NAME}.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('NORTHRIDGEHOUSE2_BUILT',manifest['bounds_m'],tris,{o.name:sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs})
