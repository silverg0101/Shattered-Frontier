"""Silverbrook Townhouse 4 from docs/Game-Design/concept-art/Silverbrook/Townhouse-4.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhouse4.py
Original procedural geometry interpreting the elevation (6 m wide, ~10 m to the gable in the
concept, compressed to the 9 m slot): squared sandstone ground storey with a pointed-arch door
on three steps and a pent-roofed three-light window; a jettied half-timbered upper storey on
carved brackets; an off-centre timber gable; a slate hip roof; a left stone stack with three
pots; a right-hand octagonal oriel turret with a slate spire; wall lantern and stag banner.
Asset BLD-GreenlandsTownhouse-01, variant 4.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,UP,tile_plane,deck,wall,lantern,frame
import bpy
import numpy as np
scene=kit.reset(4128)
NAME='SilverbrookTownhouse4'
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Townhouse-4.png'
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

M=dict(  # SB_* rows repeat Cottage1's arguments exactly; package-only surfaces are SB_Townhouse4_*
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Townhouse4_Stone','sandstone_blocks_05',dark=(.42,.36,.28)),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_Townhouse4_Banner','denim_fabric_03',dark=(.16,.19,.34),normal=.6,damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_Townhouse4_Gilt',(.55,.36,.10),.45,.8),
    emblem=kit.solid('SB_Townhouse4_Emblem',(.72,.66,.55),.7))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('sandstone_blocks_05',(.06,.035),dark=.7,frac=.01,std=.22,bright=1.0)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
G=dict(stone=Geo('SM_SBTownhouse4_Masonry',asset),plaster=Geo('SM_SBTownhouse4_Plaster',asset),timber=Geo('SM_SBTownhouse4_Timber',asset),roof=Geo('SM_SBTownhouse4_Roof',asset),detail=Geo('SM_SBTownhouse4_Details',asset))

# ---- measured layout (metres) -------------------------------------------------------
SLOT=(8,7,9)                   # concept README 'Fits' w x d x h
W2,SF,SB=2.6,-2.5,2.85         # stone storey: half width, front and rear faces
SZ,FZ=3.25,3.5                 # stone top, jetty floor top
UW,UD=2.75,2.85                # upper storey half extents (front jettied 0.35, sides 0.15)
EAVE,RIDGE,EX,EY,RX=5.5,8.05,3.05,3.15,1.6
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
PLATE=EAVE+(EY-UD)*KF-.04      # wall plate tucks under the front/rear eaves
XG,GH,GE=-.55,2.05,PLATE+.02   # front gable: centre, roof half, eave
KG=(RIDGE-GE)/GH;GY=-UD-.35
TX,TY,TR=2.35,-2.6,.85         # oriel turret centre and wall circumradius
T0,T1,RC,TA=4.3,6.12,1.05,7.75 # turret wall bottom/top, cone eave radius, cone apex
XC,YC=-2.1,.3                  # stack
STEP=.45                       # three steps; door sill
XD,DW,DS=-1.05,.9,1.77         # door centre, width, spring (equilateral pointed head)
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return RIDGE-abs(x-XG)*KG
GW=(RIDGE-.05-PLATE)/KG
in_gable=lambda p:abs(p.x-XG)<GW+.04 and p.y<0 and p.z<gable_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.47 and abs(p.y-YC)<.42
in_turret=lambda p:math.hypot(p.x-TX,p.y-TY)<RC+.02 and p.z<TA-(TA-T1)*math.hypot(p.x-TX,p.y-TY)/RC
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/2.5) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
def post(x,y,z0,z1,o,w=.2):G['timber'].box((x,y,(z0+z1)/2),(w if o=='y' else .12,.12 if o=='y' else w,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
def frustum(geo,cx,cy,z0,r0,z1,r1,mat,phase,n=8,**kw):
    """Closed n-gon frustum (both radii > 0), outward winding asserted."""
    ring=lambda z,r:[Vector((cx+r*math.cos(phase+math.tau*k/n),cy+r*math.sin(phase+math.tau*k/n),z)) for k in range(n)]
    b,t=ring(z0,r0),ring(z1,r1);s=len(geo.f)
    for k in range(n):geo.poly([b[k],b[(k+1)%n],t[(k+1)%n],t[k]],mat,**kw)
    geo.poly(t,mat,**kw);geo.poly(b[::-1],mat,**kw);assert _vol(geo,s)>0,'inside-out frustum'

def lights(c,o,w,h,n,wall_th=.2,planter=False):
    """Leaded window of n lights: oak frame, mullions, sill, diamond quarries (kit.window generalised).
    c: bottom centre of the glazed area on the outer wall face."""
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
    return (w+2*t)

# ---- main hip roof --------------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,in_stack,in_turret])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_turret])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
capped((-RX,0,RIDGE+.05),(RX,0,RIDGE+.05),12)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.05),(sx*RX,0,RIDGE+.05),12)
for a,b in ((-EX,XG-GH),(XG+GH,EX)):G['timber'].beam((a,-EY-.02,EAVE-.06),(b,-EY-.02,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((-EX,EY+.02,EAVE-.06),(EX,EY+.02,EAVE-.06),.04,.2,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.06),(sx*(EX+.02),EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- front gable roof and bargeboards ---------------------------------------------------
LG=math.hypot(GH,RIDGE-GE)
plane((XG-GH,0,GE),(0,-1,0),(GH,0,RIDGE-GE),LG,lambda t:0,lambda t:-GY,[under_main])
plane((XG+GH,0,GE),(0,1,0),(-GH,0,RIDGE-GE),LG,lambda t:GY,lambda t:0,[under_main,in_turret])
capped((XG,GY,RIDGE+.05),(XG,-.3,RIDGE+.05),10)
for s in (-1,1):G['timber'].beam((XG+s*(GH+.05),GY-.04,GE-.08),(XG-s*.16,GY-.04,RIDGE+.16*KG+.03),.08,.26,M['timber'],band=band())
# ---- stack: squared stone, raised panel, three pots ---------------------------------------
G['stone'].box((XC,YC,(5.6+8.35)/2),(.9,.8,8.35-5.6),M['stone'],.01)
G['stone'].box((XC,YC,8.0),(.96,.86,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,8.39),(1.0,.9,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
G['stone'].box((XC,YC-.41,7.45),(.36,.04,.4),M['ashlar'],.008,rect=kit.rng.choice(STONES))   # carved panel (motif not modelled)
for dx in (-.28,0,.28):G['detail'].vessel((XC+dx,YC,8.44),[(.1,0),(.095,.32),(.105,.4),(.12,.44),(.09,.44),(.075,.38),(0,.38)],M['pot'])
# ---- stone ground storey -------------------------------------------------------------------
WX,WW,WH,WZ=1.2,1.65,1.3,.85   # ground window: centre, glazed width, height, sill
APEX=STEP+DS+DW*math.sqrt(3)/2
wall(G['stone'],M['stone'],(0,SF,0),(0,-1,0),W2,SZ,[(XD,DW,STEP,APEX),(WX,WW+.18,WZ-.09,WZ+WH+.09)],th=.35)
wall(G['stone'],M['stone'],(0,SB,0),(0,1,0),W2,SZ,[(.9,.98,1.0,2.1)],th=.35)
for s in (-1,1):wall(G['stone'],M['stone'],(s*W2,(SF+SB)/2,0),(s,0,0),(SB-SF)/2-.35,SZ,th=.35)
# pointed door head: spandrel strips (convex quads, winding asserted), jamb quoins, voussoirs, leaf
P,R=frame((XD,SF,STEP),(0,-1,0));o_=Vector((0,-1,0))
for s in (-1,1):
    arc=[(-s*DW/2+s*DW*math.cos(t),DS+DW*math.sin(t)) for t in np.linspace(0,math.pi/3,10)]
    for (a0,z0),(a1,z1) in zip(arc,arc[1:]):
        pts=[(s*DW/2,z0),(a0,z0),(a1,z1),(s*DW/2,z1)]
        pts=[p for i,p in enumerate(pts) if all(math.dist(p,q)>1e-6 for q in pts[:i])]
        G['stone'].prism([P(a,-.175,z) for a,z in pts],o_,.35,M['stone'])
    for k in range(5):
        t=math.pi/3*(k+.5)/5;rm=DW+.15-.012;radial=R@Vector((s*math.cos(t),0,math.sin(t)));tang=R@Vector((-s*math.sin(t),0,math.cos(t)))
        G['stone'].box(P(-s*DW/2+s*rm*math.cos(t),-.15,DS+rm*math.sin(t)),(rm*math.pi/15-.014,.4,.3),M['ashlar'],.012,Matrix((tang,o_,radial)).transposed(),rect=kit.rng.choice(STONES))
G['stone'].box(P(0,-.15,DS+DW*.866+.2),(.2,.4,.42),M['ashlar'],.012,R,rect=kit.rng.choice(STONES))   # keystone
z=-STEP;i=0
while z<DS-.05:
    hgt=min(.36,DS-z);qw=.32 if i%2 else .46
    for s in (-1,1):G['stone'].box(P(s*(DW/2-.012+qw/2),-.15,z+hgt/2),(qw,.4,hgt-.012),M['ashlar'],.012,R,rect=kit.rng.choice(STONES))
    z+=hgt;i+=1
D=Geo('SM_SBTownhouse4_Door1',asset);G['door1']=D;kit.DOORS.append(dict(geo=D,r=R@Vector((1,0,0)),o=o_))   # leaf, straps, ring, vision light
leaf=[(-DW/2,0),(DW/2,0)]+[(DW/2-DW*(1-math.cos(t)),DS+DW*math.sin(t)) for t in np.linspace(0,math.pi/3,9)]+[(-DW/2+DW*(1-math.cos(t)),DS+DW*math.sin(t)) for t in np.linspace(math.pi/3,0,9)[1:]]
D.prism([P(a,-.2,z) for a,z in leaf],o_,.07,M['door'])
for zz in (.42,1.45):
    D.box(P(-DW*.12,-.162,zz),(DW*.72,.012,.06),M['iron'],.004,R)
    D.box(P(-DW/2+.05,-.162,zz),(.1,.014,.14),M['iron'],.004,R)
    D.box(P(DW*.24,-.162,zz),(.06,.014,.12),M['iron'],.004,R)
for k in range(10):
    t=math.tau*k/10;D.box(P(DW*.27+.06*math.cos(t),-.158,.95+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
D.box(P(DW*.27,-.162,1.01),(.07,.012,.07),M['iron'],.004,R)
pane=[(-.09,1.75),(.09,1.75),(.09,1.98),(0,2.1),(-.09,1.98)]
D.prism([P(a,-.162,z) for a,z in pane],o_,.012,M['glass'])
for a0,z0,a1,z1 in ((0,1.75,0,2.1),(-.09,1.9,.09,1.9)):D.beam(P(a0,-.153,z0),P(a1,-.153,z1),.012,.01,M['lead'],grain=False)
# three steps, each a separate course so no side faces are coplanar
for k,(wd,go) in enumerate(((1.75,.84),(1.62,.56),(1.5,.28))):
    G['stone'].box((XD,SF-go/2+.01,.15*k+.075),(wd,go+.02,.15),M['ashlar'],.012)
# ground window: posts, lintel, pent slate roof on brackets
lights((WX,SF,WZ),(0,-1,0),WW,WH,3,.35,planter=True)
for x in (WX-WW/2-.33,WX+WW/2+.33):post(x,SF-.05,0,2.4,'y',.18)
rail((WX-WW/2-.45,SF-.05,2.33),(WX+WW/2+.45,SF-.05,2.33),.14)
PX0,PX1,PL=WX-1.15,WX+1.2,math.hypot(.58,.4)
plane((PX0,SF-.58,2.42),(1,0,0),(0,.58,.4),PL,lambda t:0,lambda t:PX1-PX0,[])
for x in (PX0+.08,PX1-.08):G['timber'].prism([(x,SF+.02,2.3),(x,SF+.02,2.8),(x,SF-.55,2.41)],(1,0,0),.1,M['timber'])
# ---- jetty: floor plate, bressumer, carved brackets -------------------------------------
G['timber'].box((0,(SF-.45+UD)/2,(SZ+FZ)/2),(2*UW+.1,UD-SF+.45,FZ-SZ),M['timber'],.015,grain=True,band=band())
G['timber'].box((0,-UD-.05,FZ-.02),(2*UW+.14,.16,.16),M['timber'],.015,grain=True,band=band())
for x in (-2.35,-1.6,-.45,.68,1.78,2.45):
    G['timber'].prism([(x,SF+.02,SZ),(x,SF+.02,2.85),(x,SF-.15,2.85),(x,SF-.42,SZ)],(1,0,0),.18,M['timber'])
# ---- upper storey: plaster walls and timber framing ----------------------------------------
FH=PLATE-FZ;FWZ,FWW,FWH=4.15,1.8,.85
wall(G['plaster'],M['plaster'],(0,-UD,FZ),(0,-1,0),UW,FH,[(XG,FWW+.18,FWZ-FZ-.09,FWZ-FZ+FWH+.09)],th=.2)
wall(G['plaster'],M['plaster'],(0,UD,FZ),(0,1,0),UW,FH,[(-.6,.98,.6,1.6)],th=.2)
for s in (-1,1):
    wall(G['plaster'],M['plaster'],(s*UW,0,FZ),(s,0,0),UD-.2,FH,[(s*(-1.2 if s<0 else .9),.98,.6,1.6)],th=.2)
    top=[(y/10,main_z(UW,y/10)-.05) for y in range(-round(UD*10),round(UD*10)+1) if main_z(UW,y/10)-.05>PLATE+.01]
    G['plaster'].prism([(s*(UW-.1),y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),.2,M['plaster'])
# gable face above the plate, around a two-light window
GWZ=6.05;OB,OT,OH=GWZ-.09,GWZ+.85+.09,(.8+.18)/2
def gx(z):return (RIDGE-.05-z)/KG
for piece in ([(-gx(PLATE),PLATE),(gx(PLATE),PLATE),(gx(OB),OB),(-gx(OB),OB)],
              [(-gx(OB),OB),(-OH,OB),(-OH,OT),(-gx(OT),OT)],[(OH,OB),(gx(OB),OB),(gx(OT),OT),(OH,OT)],[(-gx(OT),OT),(gx(OT),OT),(0,RIDGE-.05)]):
    G['plaster'].prism([(XG+x,-UD+.1,z) for x,z in piece],(0,1,0),.2,M['plaster'])
y=-UD-.03
for x in (-UW+.08,XG-FWW/2-.22,XG+FWW/2+.22,UW-.08):post(x,y,FZ,PLATE,'y')
rail((-UW,y,PLATE-.07),(UW,y,PLATE-.07),.18)
for a,b in ((-UW,XG-FWW/2-.1),(XG+FWW/2+.1,UW)):rail((a,y,FWZ-.2),(b,y,FWZ-.2),.12)
rail((-UW+.12,y,FZ+.12),(XG-FWW/2-.3,y,PLATE-.2),.14);rail((XG+FWW/2+.3,y,PLATE-.2),(1.55,y,FZ+.12),.14)
rail((-UW+.12,y,FWZ-.12),(XG-FWW/2-.3,y,FZ+.1),.12)
# gable framing: studs, sill, collar, king post, braces
for x in (XG-OH-.06,XG+OH+.06):post(x,y,PLATE,gable_z(x)-.06,'y',.14)
rail((XG-gx(OB-.1),y,OB-.08),(XG+gx(OB-.1),y,OB-.08),.12)
zc=OT+.1;rail((XG-gx(zc)+.05,y,zc),(XG+gx(zc)-.05,y,zc),.14);post(XG,y,zc+.07,RIDGE-.1,'y',.14)
for s in (-1,1):rail((XG+s*(gx(PLATE)-.25),y,PLATE+.05),(XG+s*(OH+.12),y,OB-.12),.1)
# side and rear framing
for s in (-1,1):
    x=s*(UW+.03)
    for yy in (-UD+.1,0,UD-.1):post(x,yy,FZ,PLATE,'x')
    rail((x,-UD,PLATE-.07),(x,UD,PLATE-.07),.18);rail((x,-UD,FZ+.95),(x,UD,FZ+.95))
y=UD+.03
for x in (-UW+.08,0,UW-.08):post(x,y,FZ,PLATE,'y')
rail((-UW,y,PLATE-.07),(UW,y,PLATE-.07),.18);rail((-UW,y,FZ+.95),(UW,y,FZ+.95))
# ---- oriel turret on the right front corner -------------------------------------------------
AP=TR*math.cos(math.pi/8);SIDE=2*TR*math.sin(math.pi/8);PH=-math.pi/2-math.pi/8
vert=lambda k,r,z:Vector((TX+r*math.cos(PH+math.tau*k/8),TY+r*math.sin(PH+math.tau*k/8),z))
frustum(G['timber'],TX,TY,3.85,.3,4.15,TR+.02,M['timber'],PH)
frustum(G['timber'],TX,TY,4.15,TR+.07,T0,TR+.07,M['timber'],PH)
built=[]
for k in range(8):
    phi=-math.pi/2+math.tau*k/8;o=Vector((math.cos(phi),math.sin(phi),0));c=Vector((TX,TY,T0))+o*AP
    if not (c.y<-UD+.05 or c.x>UW-.05):continue
    built.append(k);win=k in (0,2)
    wall(G['plaster'],M['plaster'],c,o,SIDE/2,T1-T0,[(0,.48,.36,1.54)] if win else [],th=.15)
    if win:lights(c+UP*.45,o,.3,1.0,1,.15)
    for z in (T0+.1,T1-.08):G['timber'].beam(vert(k,TR+.03,z),vert(k+1,TR+.03,z),.1,.12,M['timber'],band=band())
for k in sorted({j for k in built for j in (k,k+1)}):G['timber'].box(vert(k,TR+.01,(T0+T1)/2),(.11,.11,T1-T0),M['timber'],.012,Matrix.Rotation(PH+math.tau*k/8,3,'Z'),grain=True,band=band())
frustum(G['timber'],TX,TY,T1,RC,T1+.08,RC,M['timber'],PH)
apex=Vector((TX,TY,TA))
for k in range(8):
    a,b=vert(k,RC/math.cos(math.pi/8),T1+.08),vert(k+1,RC/math.cos(math.pi/8),T1+.08);O=(a+b)/2;L=(apex-O).length;hb=(b-a).length/2
    plane(O,b-a,apex-O,L,(lambda hb,L:lambda t:-hb*(1-t/L))(hb,L),(lambda hb,L:lambda t:hb*(1-t/L))(hb,L),[])
    capped(a+UP*.04,apex+UP*.02,6)
G['detail'].vessel((TX,TY,TA-.12),[(.09,0),(.06,.14),(.1,.24),(.06,.32),(.03,.62),(.045,.66),(.02,.7),(0,.82)],M['iron'],12)
# ---- downpipes ---------------------------------------------------------------------------
for x,y,z0,z1 in ((-UW-.03,-UD-.08,FZ+.02,EAVE-.12),(W2+.035,SF+.09,.08,SZ-.02)):
    G['detail'].box((x,y,(z0+z1)/2),(.08,.08,z1-z0),M['iron'],.01)
    G['detail'].box((x,y,z1-.07),(.14,.14,.14),M['iron'],.01)
G['detail'].box((W2+.035,SF+.03,.06),(.1,.2,.06),M['iron'],.01)
# ---- upper windows ---------------------------------------------------------------------
lights((XG,-UD,FWZ),(0,-1,0),FWW,FWH,3,.2,planter=True)
lights((XG,-UD-.02,GWZ),(0,-1,0),.8,.85,2,.2)
lights((.6,UD,FZ+.69),(0,1,0),.8,.82,2,.2)
lights((-UW,-1.2,FZ+.69),(-1,0,0),.8,.82,2,.2)
lights((UW,.9,FZ+.69),(1,0,0),.8,.82,2,.2)
lights((-.9,SB,1.09),(0,1,0),.8,.92,2,.35)
# ---- lantern and stag banner ---------------------------------------------------------------
lantern(G,M,(-2.15,SF-.42,2.25),arm=((-2.15,SF-.04,2.25),(0,-1,0)))
def stag(geo,origin,right,up,normal,s):
    o=Vector(origin)+Vector(normal)*.008;R_,U_,N_=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+R_*(a*s)+U_*(z*s)
    geo.prism([p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N_,.02,M['emblem'])
    for sx in (-1,1):
        geo.prism([p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N_,.02,M['emblem'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['emblem'],grain=False)
AY,AZ=SF+.2,2.85
G['detail'].beam((-W2+.02,AY,AZ),(-W2-.72,AY,AZ),.045,.045,M['iron'],grain=False)
G['detail'].beam((-W2+.02,AY,AZ-.5),(-W2-.45,AY,AZ-.02),.03,.03,M['iron'],grain=False)
G['detail'].box((-W2-.02,AY,AZ-.22),(.05,.1,.6),M['iron'],.004)
for k in range(8):
    t0,t1=math.tau*k/8,math.tau*(k+1)/8;c=Vector((-W2-.24,AY,AZ-.14));r=.1
    G['detail'].beam(c+Vector((r*math.cos(t0),0,r*math.sin(t0))),c+Vector((r*math.cos(t1),0,r*math.sin(t1))),.016,.016,M['iron'],grain=False)
bx_=-W2-.45;bb=[(bx_-.25,AZ-.17),(bx_+.25,AZ-.17),(bx_+.25,AZ-1.0),(bx_,AZ-1.12),(bx_-.25,AZ-1.0)]
G['detail'].prism([(x,AY,z) for x,z in bb],(0,1,0),.02,M['banner'])
for x in (bx_-.2,bx_+.2):G['detail'].beam((x,AY,AZ-.02),(x,AY,AZ-.17),.012,.012,M['iron'],grain=False)
for n in (-1,1):
    pts=[(x,AY+n*.01,z) for x,z in bb]
    for a,b in zip(pts,pts[1:]+pts[:1]):G['detail'].beam(Vector(a)+Vector((0,n*.008,0)),Vector(b)+Vector((0,n*.008,0)),.02,.035,M['gilt'],grain=False)
    stag(G['detail'],(bx_,AY+n*.01,AZ-.6),(-n,0,0),(0,0,1),(0,n,0),.55)

# ---- export, preview scene, manifest -------------------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,-lo[2])))
assert abs(lo[2])<.02,f'model does not stand on the ground: min z {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/NAME
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
PARTS=kit.split_doors(str(OUT),'SM_SilverbrookTownhouse4')
# Door leaves: own meshes hinged on the -r jamb, opening 90 deg inward; check the swing ends inside the building.
for d,row in zip(kit.DOORS,PARTS):
    ob_=bpy.data.objects[d['geo'].name];pv=Vector(row['pivot_m']);rz=Matrix.Rotation(math.radians(row['travel']),3,'Z');o_h=Vector((d['o'].x,d['o'].y,0)).normalized()
    vs=[pv+rz@(ob_.matrix_world@v.co-pv) for v in ob_.data.vertices];c_=sum(vs,Vector())/len(vs)
    assert (c_-pv).dot(o_h)<-.3 and max((v-pv).dot(o_h) for v in vs)<.02,f"{row['name']} does not swing inward"
    print('DOOR_SWING',row['name'],row['pivot_m'],row['travel'],'open centre inward %.2f m'%-(c_-pv).dot(o_h))
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-10.5,-14,5.2),(80,0,-37),lens=34);cam('Front',(0,-30,4.6),(90,0,0),9.6);cam('Side',(-30,0,4.6),(90,0,-90),9.6);cam('Rear',(0,30,4.6),(90,0,180),9.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Townhouse4-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'house triangle budget exceeded: {tris}'
manifest=dict(asset=NAME,reference=dict(file='Townhouse4-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_townhouse4.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookTownhouse4',parts=PARTS,slot_m=list(SLOT),outputs=[f'{NAME}.blend',f'{NAME}.glb',f'{NAME}.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('TOWNHOUSE4_BUILT',manifest['bounds_m'],tris)
