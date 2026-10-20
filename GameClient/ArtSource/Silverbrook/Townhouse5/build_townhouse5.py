"""Silverbrook Townhouse 5 from docs/Game-Design/concept-art/Silverbrook/Townhouse-5.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhouse5.py
Original procedural geometry interpreting the elevation (6 m wide, ~11 m to the finial,
heights compressed ~0.8 to fit the 9 m slot): rough-faced ashlar ground storey with a
pointed-arch door and a slate-roofed shop front, jettied half-timbered upper storey, a
tall front cross gable with balcony and finial, a slate main roof hipped to the right,
right dormer and stack.
"""
import sys,math,json,hashlib,shutil
import numpy as np
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,tile_plane,deck,wall,frame,lantern
import bpy
scene=kit.reset(4805)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Townhouse-5.png'
asset=bpy.data.collections.new('SilverbrookTownhouse5');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Townhouse5_Ashlar','sandstone_blocks_05',dark=(.45,.40,.33)),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_Townhouse5_Banner','denim_fabric_03',dark=(.16,.19,.34),normal=.6,damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_Townhouse5_Gilt',(.80,.55,.18),.35,1.0))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('sandstone_blocks_05',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.0)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBTownhouse5_Masonry',asset),plaster=Geo('SM_SBTownhouse5_Plaster',asset),timber=Geo('SM_SBTownhouse5_Timber',asset),roof=Geo('SM_SBTownhouse5_Roof',asset),detail=Geo('SM_SBTownhouse5_Details',asset))
# ---- measured layout (metres; elevation ~96 px/m across, heights x0.8) -----------------
SLOT=(8,7,9)                   # town-layout slot w x d x h (concept README 'Fits')
W2,D2=2.9,2.3                  # ground-storey outer wall half-extents
JZ,JET=3.3,.3                  # jetty height and front projection
YF=-D2-JET                     # jettied upper front wall face
EAVE,RIDGE=5.75,8.1            # main roof: ridge along x, hipped at the right end only
YR,EY,EX,RX=-.15,2.75,3.25,1.6 # ridge y, eave half-depth, right eave x, ridge right end
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
XG,GW,GH,GE,GR=-.9,2.0,2.3,5.75,8.45   # front cross gable (runs front to back): centre, wall half, roof half, eave, apex
KG=(GR-GE)/GH;GY=YF-.35;GB=D2+.3
XD,DH,DW,DE,DR=2.05,.5,.38,6.95,7.5    # dormer on the main front slope
YD=YR-(RIDGE-6.2)/KF;KD=(DR-DE)/DH
XC,YC=2.25,.55                 # stack behind the ridge, right
def main_z(x,y):return min(RIDGE-abs(y-YR)*KF,RIDGE-max(0,x-RX)*KS)
def gable_z(x):return GR-abs(x-XG)*KG
def dormer_z(x):return DR-abs(x-XD)*KD
in_gable=lambda p:abs(p.x-XG)<GH+.04 and p.z<gable_z(p.x)-.005
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.47 and abs(p.y-YC)<.42
under_main=lambda p:p.x>XG and p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/2) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
def post(x,y,z0,z1,o,s=.2):G['timber'].box((x,y,(z0+z1)/2),(s if o=='y' else .12,.12 if o=='y' else s,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())

def cames(P,ac,pw,h,d,z0=0,geo=None):
    """Diamond quarries: two families of lead cames clipped to one pane (as kit.window)."""
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
                (a0,za),(a1,zb)=pts[0],pts[1]
                if math.hypot(a1-a0,zb-za)>.02:(geo or G['detail']).beam(P(a0,d,z0+za),P(a1,d,z0+zb),.009,.012,M['lead'],grain=False)
def lights(c,o,w,h,n,planter=False,wall_th=.3):
    """kit.window generalised to n leaded lights (no shutters: the concept has none).
    c: bottom centre of the glazed area on the outer wall face."""
    P,R=frame(c,o);t=.09;fd=wall_th+.04;bd=band()
    def B(geo,a,d,z,size,mat,bevel=.008,**kw):geo.box(P(a,d,z),size,mat,bevel,R,**kw)
    for a in (-(w/2+t/2),w/2+t/2):B(G['timber'],a,-wall_th/2+.02,h/2,(t,fd,h+2*t),M['timber'],grain=True,band=bd)
    for z in (h+t/2,-t/2):B(G['timber'],0,-wall_th/2+.02,z,(w,fd,t),M['timber'],grain=True,band=bd)
    pw=(w-.06*(n-1))/n
    for i in range(n):
        ac=-w/2+pw/2+i*(pw+.06)
        if i:B(G['timber'],ac-pw/2-.03,-.09,h/2,(.06,.12,h),M['timber'],grain=True,band=bd)
        B(G['detail'],ac,-.14,h/2,(pw,.01,h),M['glass'],0);cames(P,ac,pw,h,-.128)
    B(G['timber'],0,.05,-t-.025,(w+2*t+.12,.14,.05),M['timber'],grain=True,band=bd)
    if planter:
        B(G['timber'],0,.15,-t-.2,(w+2*t+.1,.24,.2),M['planks'],.006)
        B(G['detail'],0,.15,-t-.095,(w+2*t+.04,.19,.02),M['soil'],0)
        for a in (-w/2,w/2):G['timber'].beam(P(a,.03,-t-.55),P(a,.25,-t-.3),.06,.06,M['timber'],band=bd)
def hole(a,w,z,h,base=0):return (a,w+.18,z-.09-base,z+h+.09-base)

# ---- main roof (hipped right; its left end dies into the cross gable) -------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,YR-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lambda t:XG,lerp(EX,RX,LF),[in_gable,in_dormer,in_stack])
plane((0,YR+EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lambda t:-XG,[in_gable,in_stack])
plane((EX,YR,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
XR0=XG+(GR-RIDGE)/KG           # where the main ridge leaves the cross-gable roof
capped((XR0,YR,RIDGE+.05),(RX,YR,RIDGE+.05),6)
for sy in (-1,1):capped((EX,YR+sy*EY,EAVE+.05),(RX,YR,RIDGE+.05),9)
for y in (YR-EY-.02,YR+EY+.02):G['timber'].beam((XG+GH,y,EAVE-.06),(EX,y,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((EX+.02,YR-EY,EAVE-.06),(EX+.02,YR+EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- cross gable roof, bargeboards, finial -------------------------------------------------
LG=math.hypot(GH,GR-GE)
plane((XG-GH,0,GE),(0,-1,0),(GH,0,GR-GE),LG,lambda t:-GB,lambda t:-GY,[])
plane((XG+GH,0,GE),(0,1,0),(-GH,0,GR-GE),LG,lambda t:GY,lambda t:GB,[under_main])
capped((XG,GY,GR+.05),(XG,GB,GR+.05),18)
for y in (GY,GB):
    yb=y-.04 if y<0 else y+.04
    for s in (-1,1):G['timber'].beam((XG+s*(GH+.05),yb,GE-.08),(XG-s*.16,yb,GR+.16*KG+.03),.08,.26,M['timber'],band=band())
G['timber'].beam((XG-GH-.02,GY,GE-.06),(XG-GH-.02,GB,GE-.06),.04,.2,M['timber'],band=band())
G['timber'].box((XG,GY-.06,GR+.12),(.14,.14,.4),M['timber'],.02,grain=True,band=band())   # finial
G['timber'].box((XG,GY-.06,GR+.33),(.2,.2,.05),M['timber'],.02,grain=True,band=band())
G['detail'].beam((XG,GY-.06,GR+.35),(XG,GY-.06,8.95),.04,.04,M['iron'],grain=False)
G['detail'].box((XG,GY-.06,GR+.42),(.09,.09,.07),M['iron'],.01)
# ---- dormer --------------------------------------------------------------------------
LD=math.hypot(DH,DR-DE)
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:.6,lambda t:-YD+.16,[under_main])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:-.6,[under_main])
capped((XD,YD-.16,DR+.04),(XD,YR-(RIDGE-DR)/KF,DR+.04),4)
for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
DB=main_z(XD,YD)-.1
wall(G['plaster'],M['plaster'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[hole(0,.4,.22,.42)],th=.12)
G['plaster'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,dormer_z(XD+DW)-.04),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,dormer_z(XD+DW)-.04)],(0,1,0),.12,M['plaster'])
ym=YR-(RIDGE-(dormer_z(XD+DW)-.04))/KF
for s in (-1,1):G['plaster'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,dormer_z(XD+DW)-.04),(XD+s*DW,ym,dormer_z(XD+DW)-.04)],(1,0,0),.08,M['plaster'])
for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
lights((XD,YD,DB+.22),(0,-1,0),.4,.42,1,wall_th=.12)
# ---- stack ---------------------------------------------------------------------------
G['stone'].box((XC,YC,7.3),(.8,.7,1.9),M['stone'],.01)
G['stone'].box((XC,YC,8.3),(.86,.76,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,8.41),(.92,.82,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.17,.17):G['detail'].vessel((XC+dx,YC,8.47),[(.1,0),(.095,.32),(.105,.4),(.12,.44),(.09,.44),(.075,.38),(0,.38)],M['pot'])

# ---- ground storey (rough-faced ashlar) -------------------------------------------------
STEP,XDOOR,DOORW,SPRING=.39,-1.6,.95,1.6      # door sill on the top step; two-centred arch
RR=.85*DOORW;TA=math.acos((RR-DOORW/2)/RR);APEX=SPRING+RR*math.sin(TA)
SHOP,SHOPW,SHOPZ=.2,.75,.15                   # shop door (one step)
GZ,GH_=.75,1.35                               # shop window and side light
GWIN=[(.86,.26,1),(1.95,1.35,2)]
front=[(XDOOR,DOORW,STEP,STEP+APEX),(SHOP,SHOPW,SHOPZ,2.2)]+[hole(x,w,GZ,GH_) for x,w,_ in GWIN]
wall(G['stone'],M['stone'],(0,-D2,0),(0,-1,0),W2,JZ,front)
RWIN=[(-1.5,.55),(1.6,.55)];UWZ,UWH=1.0,1.0
wall(G['stone'],M['stone'],(0,D2,0),(0,1,0),W2,JZ,[hole(-x,w,UWZ,UWH) for x,w in RWIN])
wall(G['stone'],M['stone'],(W2,0,0),(1,0,0),D2-.3,JZ,[hole(.3,.55,UWZ,UWH)])
wall(G['stone'],M['stone'],(-W2,0,0),(-1,0,0),D2-.3,JZ,[hole(-.6,.55,UWZ,UWH)])
for x,w,n in GWIN:lights((x,-D2,GZ),(0,-1,0),w,GH_,n)
for x,w in RWIN:lights((x,D2,UWZ),(0,1,0),w,UWH,2)
lights((W2,.3,UWZ),(1,0,0),.55,UWH,2);lights((-W2,.6,UWZ),(-1,0,0),.55,UWH,2)
# pointed-arch door: own spandrel fills (convex strips), jamb quoins and voussoirs
P,R=frame((XDOOR,-D2,STEP),(0,-1,0));r=DOORW/2
arc=[(r-RR+RR*math.cos(t),SPRING+RR*math.sin(t)) for t in np.linspace(0,TA,9)]   # (r,spring) -> (0,apex)
for side in (-1,1):
    for (a0,z0),(a1,z1) in zip(arc,arc[1:]):
        pts=[(a0,z0),(a1,z1),(a1,APEX),(a0,APEX)] if APEX-z1>1e-4 else [(a0,z0),(a1,z1),(a0,APEX)]
        G['stone'].prism([P(side*a,-.15,z) for a,z in pts],(0,-1,0),.3,M['stone'])
D=Geo('SM_SBTownhouse5_Door1',asset);G['door1']=D;kit.DOORS.append(dict(geo=D,r=R@Vector((1,0,0)),o=Vector((0,-1,0))))
leaf=[(-r+.005,0),(r-.005,0)]+[(a-.005,z) for a,z in arc]+[(-a+.005,z) for a,z in reversed(arc[:-1])]
D.prism([P(a,-.2,z) for a,z in leaf],(0,-1,0),.07,M['door'])
for z in (.35,SPRING-.15):D.box(P(-r*.15,-.15,z),(DOORW*.8,.012,.05),M['iron'],.004,R)
for k in range(10):
    t=math.tau*k/10;D.box(P(r*.55+.06*math.cos(t),-.13,1.0+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
D.box(P(r*.55,-.155,1.06),(.07,.01,.07),M['iron'],.004,R)
z=0;i=0
while z<SPRING-.05:
    hgt=min(.3,SPRING-z);qw=.28 if i%2 else .4
    for side in (-1,1):G['stone'].box(P(side*(r-.012+qw/2),-.125,z+hgt/2),(qw,.35,hgt-.012),M['ashlar'],.012,R,rect=kit.rng.choice(STONES))
    z+=hgt;i+=1
dep=.28;n=5
for side in (-1,1):
    for k in range(n):
        t0,t1=TA*k/n,TA*(k+1)/n;tm=(t0+t1)/2;rm=RR+dep/2
        radial=R@Vector((side*math.cos(tm),0,math.sin(tm)));tang=R@Vector((-side*math.sin(tm),0,math.cos(tm)));o_=Vector((0,-1,0))
        G['stone'].box(P(side*(r-RR+rm*math.cos(tm)),-.125,SPRING+rm*math.sin(tm)),(rm*(t1-t0)-.014,.35,dep),M['ashlar'],.012,Matrix((tang,o_,radial)).transposed(),rect=kit.rng.choice(STONES))
for k in range(3):G['stone'].box((XDOOR,-D2-.15-.3*k,(3-k)*.065),(1.3,.3,(3-k)*.13),M['stone'],.015)
# shop door: plank lower leaf, leaded upper light
P,R=frame((SHOP,-D2,SHOPZ),(0,-1,0));lw=SHOPW-.02
D=Geo('SM_SBTownhouse5_Door2',asset);G['door2']=D;kit.DOORS.append(dict(geo=D,r=R@Vector((1,0,0)),o=Vector((0,-1,0))))   # whole leaf: planks, stiles, rail, light
D.box(P(0,-.2,.55),(lw,.07,1.1),M['door'],.006,R)
for a in (-(lw/2-.04),lw/2-.04):D.box(P(a,-.2,1.53),(.08,.07,.86),M['timber'],.006,R,grain=True,band=band())
D.box(P(0,-.2,2.0),(lw,.07,.1),M['timber'],.006,R,grain=True,band=band())
D.box(P(0,-.2,1.53),(lw-.16,.01,.86),M['glass'],0,R);cames(P,0,lw-.16,.86,-.18,1.1,D)
D.box(P(.22,-.15,1.0),(.05,.03,.12),M['iron'],.004,R)
G['stone'].box((SHOP,-D2-.17,.075),(.95,.34,.15),M['stone'],.015)
# shop front timber: fascia and posts flanking the glazing
G['timber'].box((1.2,-D2-.03,2.33),(3.3,.1,.16),M['timber'],.012,grain=True,band=band())
for x in (-.4,.62,1.15,2.78):post(x,-D2-.03,SHOPZ if x<0 else .44,2.25,'y',.12)
G['timber'].box((1.95,-D2-.04,.5),(1.6,.08,.36),M['planks'],.008)       # stallboard
# plinth course (open at the steps and the shop door)
for a0,a1 in ((-W2-.04,XDOOR-.67),(XDOOR+.67,SHOP-.42),(.62,W2+.04)):G['stone'].box(((a0+a1)/2,-D2-.02,.22),(a1-a0,.1,.44),M['stone'],.01)
G['stone'].box((0,D2+.02,.22),(2*W2+.08,.1,.44),M['stone'],.01)
for s in (-1,1):G['stone'].box((s*(W2+.02),0,.22),(.1,2*D2,.44),M['stone'],.01)
# ---- shop awning: slate lean-to with hipped ends on two posts ------------------------------
AX0,AX1,AY,AE,AT=-.85,3.05,-D2-.95,2.45,2.95
LA=math.hypot(-D2-AY,AT-AE);HIPA=.35
plane((0,AY,AE),(1,0,0),(0,-D2-AY,AT-AE),LA,lerp(AX0,AX0+HIPA,LA),lerp(AX1,AX1-HIPA,LA),[])
LH=math.hypot(HIPA,AT-AE)
plane((AX0,0,AE),(0,-1,0),(HIPA,0,AT-AE),LH,lambda t:D2,lambda t:-AY-(t/LH)*(-AY-D2),[])
plane((AX1,0,AE),(0,1,0),(-HIPA,0,AT-AE),LH,lambda t:AY+(t/LH)*(-D2-AY),lambda t:-D2,[])
capped((AX0,AY,AE+.04),(AX0+HIPA,-D2,AT+.04),3);capped((AX1,AY,AE+.04),(AX1-HIPA,-D2,AT+.04),3)
G['timber'].beam((AX0,AY+.06,AE-.06),(AX1,AY+.06,AE-.06),.1,.14,M['timber'],band=band())
for x in (AX0+.15,AX1-.15):
    G['stone'].box((x,AY+.12,.1),(.3,.3,.2),M['ashlar'],.015,rect=kit.rng.choice(STONES))
    post(x,AY+.12,.2,AE-.12,'y',.14)
    rail((x,AY+.12,AE-.12),(x,-D2-.02,AT-.1),.12)
    for s in (-1,1):
        if AX0<x+s*.45<AX1:rail((x,AY+.12,1.95),(x+s*.45,AY+.12,AE-.14),.08)
G['timber'].beam((AX0+.3,-D2-.06,AT-.1),(AX1-.3,-D2-.06,AT-.1),.1,.12,M['timber'],band=band())

# ---- jetty and upper storey ------------------------------------------------------------
G['timber'].box((0,(YF-.07-D2+.05)/2,JZ+.04),(2*W2+.1,(-D2+.05)-(YF-.07),.24),M['timber'],.015,grain=True,band=band())
for x in np.linspace(-W2+.15,W2-.15,8):
    G['timber'].box((x,(YF-.04-D2)/2,JZ-.22),(.14,-D2-(YF-.04),.27),M['timber'],.01,grain=True,band=band())
    G['timber'].beam((x,-D2-.03,JZ-.6),(x,YF+.02,JZ-.12),.1,.1,M['timber'],band=band())
PLATE=main_z(2,YF)-.04;PLATE_R=main_z(2,D2)-.04
GC=gable_z(XG+GW)-.05
UZ,UH=3.95,.8;XRW=1.95
# cross-gable front: plaster to GC, then strips round the gable window, then the apex
wall(G['plaster'],M['plaster'],(XG,YF,JZ),(0,-1,0),GW,GC-JZ,[hole(0,1.55,UZ,UH,JZ)])
GWZ,GWH,GWW=GC+.09,.68,.8;OH,OT=GWW/2+.09,GWZ+GWH+.09
def gx(z):return (GR-.05-z)/KG
for piece in ([(-GW,GC),(-OH,GC),(-OH,OT),(-gx(OT),OT)],[(OH,GC),(GW,GC),(gx(OT),OT),(OH,OT)],[(-gx(OT),OT),(gx(OT),OT),(0,GR-.05)]):
    G['plaster'].prism([(XG+x,YF+.15,z) for x,z in piece],(0,1,0),.3,M['plaster'])
wall(G['plaster'],M['plaster'],((XG+GW+W2)/2,YF,JZ),(0,-1,0),(W2-XG-GW)/2,PLATE-JZ,[hole(XRW-(XG+GW+W2)/2,.5,UZ,.9,JZ)])
# rear: cross-gable face and right section
wall(G['plaster'],M['plaster'],(XG,D2,JZ),(0,1,0),GW,GC-JZ,[hole(0,.7,UZ,UH,JZ)])
G['plaster'].prism([(XG-GW,D2-.15,GC),(XG+GW,D2-.15,GC),(XG,D2-.15,GR-.05)],(0,1,0),.3,M['plaster'])
wall(G['plaster'],M['plaster'],((XG+GW+W2)/2,D2,JZ),(0,1,0),(W2-XG-GW)/2,PLATE_R-JZ,[hole(-(2.0-(XG+GW+W2)/2),.5,UZ,.8,JZ)])
# sides: rectangle, then the roof profile on the hipped right
YS0,YS1=YF+.3,D2-.3;YM=(YS0+YS1)/2;ZS=main_z(W2,YS0)-.05
wall(G['plaster'],M['plaster'],(W2,YM,JZ),(1,0,0),(YS1-YS0)/2,ZS-JZ,[hole(0,.55,UZ,.8,JZ)])
top=[(y,main_z(W2-.15,y)-.05) for y in np.linspace(YS0,YS1,41)]
G['plaster'].prism([(W2-.15,y,z) for y,z in [(YS1,ZS),(YS0,ZS)]+top],(1,0,0),.3,M['plaster'])
wall(G['plaster'],M['plaster'],(-W2,YM,JZ),(-1,0,0),(YS1-YS0)/2,GC-JZ,[hole(-(.3-YM),.55,UZ,.8,JZ)])
# upper windows
lights((XG,YF,UZ),(0,-1,0),1.55,UH,3,planter=True)
lights((XRW,YF,UZ),(0,-1,0),.5,.9,1)
lights((XG,YF,GWZ),(0,-1,0),GWW,GWH,2)
lights((XG,D2,UZ),(0,1,0),.7,UH,2);lights((2.0,D2,UZ),(0,1,0),.5,.8,1)
lights((W2,YM,UZ),(1,0,0),.55,.8,2);lights((-W2,.3,UZ),(-1,0,0),.55,.8,2)
# ---- half-timbering ------------------------------------------------------------------
y=YF-.03
for x in (-W2+.07,XG-1.0,XG+1.0,XG+GW-.07,XRW-.37,XRW+.37,W2-.07):post(x,y,JZ+.16,(GC if x<XG+GW else PLATE)-.04,'y',.16)
for a0,a1 in ((-W2,XG+GW),(XG+GW,W2)):rail((a0,y,JZ+.22),(a1,y,JZ+.22),.12);rail((a0,y,UZ+UH+.2),(a1,y,UZ+UH+.2),.12)
rail((-W2,y,GC-.06),(XG+GW,y,GC-.06),.16);rail((XG+GW,y,PLATE-.08),(W2,y,PLATE-.08),.18)
for s in (-1,1):rail((XG+s*(GW-.12),y,JZ+.3),(XG+s*1.08,y,UZ+UH+.12),.12)       # braces flanking the triple light
for x0,x1 in ((XG+GW+.1,XRW-.45),(W2-.12,XRW+.45)):rail((x0,y,JZ+.3),(x1,y,UZ+UH+.12),.1)
for s in (-1,1):post(XG+s*(OH+.06),y,GC+.02,OT+.02,'y',.12)
zc=OT+.12;rail((XG-gx(zc)+.05,y,zc),(XG+gx(zc)-.05,y,zc),.14);post(XG,y,zc+.07,GR-.08,'y',.14)
for s in (-1,1):
    rail((XG+s*(GW-.15),y,GC+.04),(XG+s*(OH+.12),y,zc-.08),.1)
    rail((XG+s*.1,y,zc+.1),(XG+s*.6,y,GR-.05-.6*KG-.05),.1)
for s in (-1,1):                # side framing
    x=s*(W2+.03)
    for yy in (YS0-.2,YM-.55,YM+.55,YS1+.2):post(x,yy,JZ+.04,(GC if s<0 else ZS)-.06,'x',.14)
    rail((x,YF,JZ+.04),(x,D2,JZ+.04),.18);rail((x,YF,(GC if s<0 else ZS)-.08),(x,D2,(GC if s<0 else ZS)-.08),.16)
y=D2+.03                        # rear framing
for x in (-W2+.07,XG-.6,XG+.6,XG+GW-.07,W2-.07):post(x,y,JZ+.1,(GC if x<XG+GW else PLATE_R)-.04,'y',.16)
rail((-W2,y,JZ+.04),(W2,y,JZ+.04),.2);rail((-W2,y,GC-.06),(XG+GW,y,GC-.06),.16);rail((XG+GW,y,PLATE_R-.08),(W2,y,PLATE_R-.08),.18)
rail((XG-.3,y,GC+.05),(XG+.3,y,GC+.05),.1);post(XG,y,GC+.1,GR-.08,'y',.14)
# ---- balcony at the foot of the gable ----------------------------------------------------
BZ,BX,BD=5.0,1.55,.62
G['timber'].box((XG,YF-BD/2+.03,BZ+.06),(2*BX,BD+.06,.12),M['timber'],.012,grain=True,band=band())
for dx in (-1.45,-1.1,1.1,1.45):G['timber'].beam((XG+dx,YF-.02,BZ-.45),(XG+dx,YF-BD+.08,BZ-.01),.1,.12,M['timber'],band=band())
yb=YF-BD+.04
for s in (-1,1):post(XG+s*(BX-.05),yb,BZ+.12,BZ+.78,'y',.1)
for zr in (BZ+.2,BZ+.72):
    G['timber'].beam((XG-BX,yb,zr),(XG+BX,yb,zr),.1,.08,M['timber'],band=band())
    for s in (-1,1):G['timber'].beam((XG+s*(BX-.05),YF+.02,zr),(XG+s*(BX-.05),yb,zr),.1,.08,M['timber'],band=band())
for x in np.linspace(XG-BX+.17,XG+BX-.17,17):G['timber'].box((x,yb,BZ+.46),(.055,.055,.48),M['timber'],.008,grain=True,band=band())
for s in (-1,1):
    for yy in np.linspace(YF-.12,yb+.12,3):G['timber'].box((XG+s*(BX-.05),yy,BZ+.46),(.055,.055,.48),M['timber'],.008,grain=True,band=band())
# ---- gutters and downpipes ----------------------------------------------------------------
G['detail'].beam((XG+GH,YR-EY-.08,EAVE-.12),(EX+.04,YR-EY-.08,EAVE-.12),.1,.08,M['iron'],grain=False)
G['detail'].beam((XG-GH-.08,GY,GE-.12),(XG-GH-.08,GB,GE-.12),.1,.08,M['iron'],grain=False)
for top_,mid,wallp in (((EX-.04,YR-EY-.08,EAVE-.12),(EX-.04,YR-EY-.08,3.75),(W2+.05,-D2+.12,3.4)),((XG-GH-.08,-2.85,GE-.12),(XG-GH-.08,-2.85,3.75),(-W2-.05,-2.05,3.4))):
    G['detail'].beam(top_,mid,.07,.07,M['iron'],grain=False);G['detail'].beam(mid,wallp,.07,.07,M['iron'],grain=False)
    G['detail'].beam(wallp,(wallp[0],wallp[1],.12),.07,.07,M['iron'],grain=False);G['detail'].box((wallp[0],wallp[1],.08),(.1,.1,.08),M['iron'],.008)
    for zz in (1.0,2.4):G['detail'].box((wallp[0]-math.copysign(.03,wallp[0]),wallp[1],zz),(.06,.1,.03),M['iron'],.004)
# ---- hanging sign on the left wall: navy banner with a gilded stag -------------------------
def stag(geo,origin,right,up,normal,s):
    o=Vector(origin)+Vector(normal)*.008;R_,U,N=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+R_*(a*s)+U*(z*s)
    geo.prism([p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,M['gilt'])
    for sx in (-1,1):
        geo.prism([p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
def trim(geo,pts,normal,w=.035):
    n_=Vector(normal)*.008
    for a,b in zip(pts,pts[1:]+pts[:1]):geo.beam(Vector(a)+n_,Vector(b)+n_,.015,w,M['gilt'],grain=False)
SY,SZ,SX=-1.55,3.0,-W2-1.0
G['detail'].beam((-W2+.05,SY,SZ),(SX,SY,SZ),.05,.05,M['iron'],grain=False)
G['detail'].beam((-W2+.05,SY,SZ-.6),(-W2-.65,SY,SZ-.02),.035,.035,M['iron'],grain=False)
G['detail'].box((-W2-.03,SY,SZ-.3),(.06,.1,.8),M['iron'],.004)
for k in range(8):
    t0,t1=math.tau*k/8,math.tau*(k+1)/8;c=Vector((-W2-.35,SY,SZ-.16));rr=.12
    G['detail'].beam(c+Vector((rr*math.cos(t0),0,rr*math.sin(t0))),c+Vector((rr*math.cos(t1),0,rr*math.sin(t1))),.02,.02,M['iron'],grain=False)
bxc=SX+.42;sb=[(bxc-.28,SZ-.12),(bxc+.28,SZ-.12),(bxc+.28,SZ-.85),(bxc,SZ-1.05),(bxc-.28,SZ-.85)]
G['detail'].prism([(x,SY,z) for x,z in sb],(0,1,0),.03,M['banner'])
G['detail'].beam((bxc-.36,SY,SZ-.08),(bxc+.36,SY,SZ-.08),.03,.03,M['iron'],grain=False)
for x in (bxc-.24,bxc+.24):G['detail'].beam((x,SY,SZ-.02),(x,SY,SZ-.1),.015,.015,M['iron'],grain=False)
for n_ in (-1,1):
    trim(G['detail'],[(x,SY+n_*.015,z) for x,z in sb],(0,n_,0))
    stag(G['detail'],(bxc,SY+n_*.015,SZ-.62),(-n_,0,0),(0,0,1),(0,n_,0),.55)
# ---- lanterns -----------------------------------------------------------------------------
lantern(G,M,(-2.45,-D2-.42,2.3),arm=((-2.45,-D2-.09,2.3),(0,-1,0)))
lantern(G,M,(.95,-D2-.4,2.65))
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'not grounded: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookTownhouse5'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
PARTS=kit.split_doors(str(OUT),'SM_SilverbrookTownhouse5')
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
cam('Beauty',(-9,-12,5.5),(86,0,-37),lens=38);cam('Front',(0,-30,4.6),(90,0,0),9.8);cam('Side',(-30,0,4.6),(90,0,-90),9.8);cam('Rear',(0,30,4.6),(90,0,180),9.8)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Townhouse5-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120_000,f'house triangle budget: {tris}'
manifest=dict(asset='SilverbrookTownhouse5',reference=dict(file='Townhouse5-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_townhouse5.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookTownhouse5',parts=PARTS,slot_m=list(SLOT),outputs=['SilverbrookTownhouse5.blend','SilverbrookTownhouse5.glb','SilverbrookTownhouse5.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('TOWNHOUSE5_BUILT',manifest['bounds_m'],tris)
