"""Silverbrook Townhouse 3 from docs/Game-Design/concept-art/Silverbrook/Townhouse-3.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhouse3.py
Original procedural geometry interpreting the elevation (6 m across the stone storey;
the drawn 10 m height is compressed to the 9 m slot, mostly in the roof): pale coursed
sandstone ground storey with two round-headed doors and a canted bay window, jettied
half-timbered upper storey on carved brackets, full-width front gable with a three-light
window and finial, steep-hipped blue-grey slate roof, right ridge stack, iron downpipes.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,door,lantern,frame
import bpy
import numpy as np
scene=kit.reset(4183)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Townhouse-3.png'
asset=bpy.data.collections.new('SilverbrookTownhouse3');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Townhouse3_Slate','grey_roof_tiles_02',dark=(.10,.12,.24),lichen='lichen_rock',damp=False,aged=.55),
    stone=kit.scan('SB_Townhouse3_Ashlar','sandstone_blocks_05',dark=(.45,.40,.33)),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('sandstone_blocks_05',(.05,.035),dark=.7,frac=.01,std=.22)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBTownhouse3_Masonry',asset),plaster=Geo('SM_SBTownhouse3_Plaster',asset),timber=Geo('SM_SBTownhouse3_Timber',asset),roof=Geo('SM_SBTownhouse3_Roof',asset),detail=Geo('SM_SBTownhouse3_Details',asset))
# ---- measured layout (metres; elevation read at ~102 px/m across, heights compressed to the slot) ----
SLOT=(8,7,9)                   # town-layout slot w x d x h (concept README 'Fits')
W2,GF,GB=3.1,-2.3,2.7          # ground storey: wall half-width, front face y, rear face y
UW,UD=3.35,2.7                 # upper storey half extents: jetty .4 at the front, .25 at the sides
YF=-UD
JZ,FZ=3.3,3.58                 # top of the stone storey, upper floor (bressumer top)
EAVE,RIDGE=5.85,8.3           # main hip roof
EX,EY,RX=3.7,3.0,2.75          # eave half-extents, ridge half-length (steep hips)
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
GW,GH,GR=2.3,2.55,RIDGE        # front gable: wall half, roof half, apex at the main ridge (eaves level with the main eaves)
GE=EAVE;KG=(GR-GE)/GH;GY=YF-.38
XC,YC=2.45,-.55                # stack on the right of the front slope, as drawn
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return GR-abs(x)*KG
in_gable=lambda p:abs(p.x)<GW+.04 and p.y<0 and p.z<gable_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.45 and abs(p.y-YC)<.4
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.3,.7) if kit.rng.random()<.22 else 0
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

def prism(geo,points,axis,th,mat,**kw):
    """kit.prism orients by its first three points, which flips concave outlines (arch spandrels);
    orient by the Newell normal instead and assert the closed solid has positive volume."""
    pts=[Vector(p) for p in points];axis=Vector(axis).normalized();n=Vector((0,0,0))
    for a,b in zip(pts,pts[1:]+pts[:1]):n+=a.cross(b)
    if n.dot(axis)<0:pts.reverse()
    o=axis*th/2;faces=[[p+o for p in pts],[p-o for p in reversed(pts)]]+[[a-o,b-o,b+o,a+o] for a,b in zip(pts,pts[1:]+pts[:1])]
    vol=sum(f[0].dot(f[j].cross(f[j+1])) for f in faces for j in range(1,len(f)-1))
    assert vol>0,'inside-out prism'
    for f in faces:geo.poly(f,mat,**kw)
def spandrels(geo,mat,c,o,a,w,spring,th=.3):
    """Square the head of a round-arched opening (replaces wall(arch=...))."""
    P,_=frame(c,o);rad=w/2
    for side in (-1,1):
        arc=[(a+side*rad*math.cos(t),spring+rad*math.sin(t)) for t in np.linspace(0,math.pi/2,9)]
        prism(geo,[P(x,-th/2,z) for x,z in arc+[(a+side*rad,spring+rad)]],o,th,mat)

def lights(c,o,w,h,n=2,wall_th=.3,planter=False,sill=True):
    """Oak-framed leaded window of n lights (kit.window generalised; no shutters, as drawn).
    c: bottom centre of the glazed area on the outer face."""
    P,R=frame(c,o);t=.09;fd=wall_th+.04;gd=-.14 if wall_th>.2 else -.06
    def B(geo,a,d,z,size,mat,bevel=.008,**kw):geo.box(P(a,d,z),size,mat,bevel,R,**kw)
    for a in (-(w/2+t/2),w/2+t/2):B(G['timber'],a,-wall_th/2+.02,h/2,(t,fd,h+2*t),M['timber'],grain=True,band=band())
    for z in (h+t/2,-t/2):B(G['timber'],0,-wall_th/2+.02,z,(w,fd,t),M['timber'],grain=True,band=band())
    pw=(w-.06*(n-1))/n
    for i in range(n):
        ac=-w/2+pw/2+i*(pw+.06)
        if i:B(G['timber'],ac-pw/2-.03,gd+.05,h/2,(.06,.12,h),M['timber'],grain=True,band=band())
        B(G['detail'],ac,gd,h/2,(pw,.01,h),M['glass'],0)
        lo,hi=ac-pw/2,ac+pw/2;slope=1.7;step=.10*slope          # diamond quarries, as kit.window
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
                    if math.hypot(a1-a0,z1-z0)>.02:G['detail'].beam(P(a0,gd+.012,z0),P(a1,gd+.012,z1),.009,.012,M['lead'],grain=False)
    if sill:B(G['timber'],0,.05,-t-.025,(w+2*t+.12,.14,.05),M['timber'],grain=True,band=band())
    if planter:
        B(G['timber'],0,.15,-t-.2,(w+2*t+.1,.24,.2),M['planks'],.006)
        B(G['detail'],0,.15,-t-.095,(w+2*t+.04,.19,.02),M['soil'],0)
        for a in (-w/2,w/2):G['timber'].beam(P(a,.03,-t-.55),P(a,.25,-t-.3),.06,.06,M['timber'],band=band())
def hole(a,w,z,h,base=0):return (a,w+.18,z-.09-base,z+h+.09-base)
# ---- main hip roof ----------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,in_stack])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
capped((-RX,0,RIDGE+.05),(RX,0,RIDGE+.05),18)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.05),(sx*RX,0,RIDGE+.05),11)
for s in (-1,1):G['timber'].beam((s*EX,-EY-.02,EAVE-.06),(s*GH,-EY-.02,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((-EX,EY+.02,EAVE-.06),(EX,EY+.02,EAVE-.06),.04,.2,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.06),(sx*(EX+.02),EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- front gable roof, bargeboards and finial ----------------------------------------
LG=math.hypot(GH,GR-GE)
plane((-GH,0,GE),(0,-1,0),(GH,0,GR-GE),LG,lambda t:0,lambda t:-GY,[under_main])
plane((GH,0,GE),(0,1,0),(-GH,0,GR-GE),LG,lambda t:GY,lambda t:0,[under_main])
capped((0,GY,GR+.05),(0,-.13,GR+.05),12)
for s in (-1,1):G['timber'].beam((s*(GH+.05),GY-.04,GE-.08),(-s*.16,GY-.04,GR+.16*KG+.03),.09,.26,M['timber'],band=band())
G['timber'].box((0,GY-.07,GR+.125),(.16,.16,.35),M['timber'],.02,grain=True,band=band())
G['timber'].box((0,GY-.07,GR+.325),(.22,.22,.05),M['timber'],.02,grain=True,band=band())
G['detail'].beam((0,GY-.07,GR+.35),(0,GY-.07,GR+.41),.07,.07,M['iron'],grain=False)
prism(G['detail'],[(-.045,GY-.07,GR+.41),(.045,GY-.07,GR+.41),(0,GY-.07,8.98)],(0,1,0),.03,M['iron'])
# ---- ridge stack -----------------------------------------------------------------------
G['stone'].box((XC,YC,6.975),(.8,.7,3.05),M['stone'],.01)
G['stone'].box((XC,YC,8.15),(.86,.76,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,8.5),(.92,.82,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.19,.19):G['detail'].vessel((XC+dx,YC,8.55),[(.1,0),(.095,.32),(.105,.37),(.115,.42),(.085,.42),(.07,.37),(0,.37)],M['pot'])
# ---- ground storey (coursed sandstone) ------------------------------------------------------
STEP,DS,DW,DX=.3,1.75,.85,2.2   # door sill = top step; arch spring above it; door width, door centres
WZ,WH=1.0,1.1                   # rear/side ground windows
wall(G['stone'],M['stone'],(0,GF,0),(0,-1,0),W2,JZ,[(s*DX,DW,STEP,STEP+DS+DW/2) for s in (-1,1)])
for s in (-1,1):spandrels(G['stone'],M['stone'],(0,GF,0),(0,-1,0),s*DX,DW,STEP+DS)
wall(G['stone'],M['stone'],(0,GB,0),(0,1,0),W2,JZ,[hole(x,.8,WZ,WH) for x in (-1.5,1.5)])
for s in (-1,1):wall(G['stone'],M['stone'],(s*W2,(GF+GB)/2,0),(s,0,0),(GB-GF)/2-.3,JZ,[hole(0,.8,WZ,WH)])
for s in (-1,1):
    x=s*DX;G['stone'].box((x,GF-.3,.075),(1.35,.6,.15),M['stone'],.015);G['stone'].box((x,GF-.15,.15),(1.2,.3,.3),M['stone'],.015)
    door(G,M,(x,GF,STEP),(0,-1,0),DW,DS,STONES,band=band())
for x in (-1.5,1.5):lights((x,GB,WZ),(0,1,0),.8,WH)
for s in (-1,1):lights((s*W2,(GF+GB)/2,WZ),(s,0,0),.8,WH)
# ---- canted bay window --------------------------------------------------------------------
BZ,BH,BX,BP=.8,2.46,1.05,.5     # base-panel top, glazing head, back half-width, projection
BE,BT,RW,RD=2.7,3.2,1.15,.6     # bay roof eave and top at the wall, roof half-width and depth
plan=[Vector((-BX,GF,0)),Vector((-.7,GF-BP,0)),Vector((.7,GF-BP,0)),Vector((BX,GF,0))]
prism(G['stone'],[(-BX-.05,GF,.075),(-.72,GF-BP-.05,.075),(.72,GF-BP-.05,.075),(BX+.05,GF,.075)],(0,0,1),.15,M['stone'])
for p in plan:post(p.x,p.y-(.07 if p.y>GF-.01 else 0),.15,BE-.04,'y',.14)
for p,q in zip(plan,plan[1:]):
    d=(q-p).normalized();o=d.cross(Vector((0,0,1)));m=(p+q)/2;L=(q-p).length;P,R=frame(m,o)
    G['timber'].box(P(0,-.06,(.15+BZ)/2),(L-.14,.12,BZ-.15),M['door'],.008,R)
    for a in np.linspace(-(L-.14)/2+.08,(L-.14)/2-.08,max(2,round(L/.35)+1)):G['timber'].box(P(a,.01,(.15+BZ)/2),(.05,.03,BZ-.25),M['timber'],.006,R,grain=True,band=band())
    lights(m+Vector((0,0,BZ+.12)),o,L-.32,BH-BZ-.12,2 if L>1 else 1,wall_th=.12,sill=False)
    G['timber'].box(P(0,-.04,BH+.1),(L-.1,.14,.2),M['timber'],.01,R,grain=True,band=band())
    G['timber'].box(P(0,.0,BZ+.01),(L-.1,.14,.06),M['timber'],.008,R,grain=True,band=band())
G['timber'].box((0,GF-RD/2,BE-.03),(2*RW,RD,.06),M['planks'],.006)
LB=math.hypot(RD,BT-BE);G['roof'].beam((-RW,GF-RD-.02,BE-.05),(RW,GF-RD-.02,BE-.05),.04,.14,M['timber'],band=band())
plane((0,GF-RD,BE),(1,0,0),(0,RD,BT-BE),LB,lerp(-RW,-RW+RD,LB),lerp(RW,RW-RD,LB),[])
plane((-RW,GF,BE),(0,-1,0),(RD,0,BT-BE),LB,lambda t:0,lerp(RD,0,LB),[])
plane((RW,GF-RD,BE),(0,1,0),(-RD,0,BT-BE),LB,lerp(0,RD,LB),lambda t:RD,[])
for s in (-1,1):capped((s*RW,GF-RD,BE+.04),(s*(RW-RD),GF,BT+.04),3)
G['detail'].box((0,GF-.04,BT+.02),(2*(RW-RD)+.1,.08,.1),M['lead'],.01)
# ---- jetty: bressumer, side bearers and carved brackets -------------------------------------
G['timber'].box((0,(YF-.02+GF+.3)/2,(JZ+FZ)/2),(2*UW+.04,GF+.3-YF+.02,FZ-JZ),M['timber'],.015,grain=True,band=band())
for s in (-1,1):G['timber'].box((s*(W2-.3+UW+.02)/2,(GF+.3+UD-.3)/2,(JZ+FZ)/2),(UW+.32-W2,UD-.6-GF,FZ-JZ),M['timber'],.015,grain=True,band=band())
for x in (-3.0,-2.2,-1.38,1.38,2.2,3.0):
    G['timber'].box((x,GF-.07,JZ-.24),(.16,.14,.48),M['timber'],.01,grain=True,band=band())
    G['timber'].box((x,GF-.15,JZ-.3),(.11,.04,.11),M['timber'],.006,grain=True,band=band())
    G['timber'].beam((x,GF-.1,JZ-.4),(x,YF+.1,JZ-.04),.12,.12,M['timber'],band=band())
for x in np.linspace(-UW+.15,UW-.15,15):G['timber'].box((x,YF+.02,JZ-.05),(.1,.1,.1),M['timber'],.008,grain=True,band=band())
# ---- upper storey walls (lime plaster) and front gable ---------------------------------------
PLATE=main_z(0,YF)-.04;GC=gable_z(GW)-.05;UWZ,UWH,UWX,UWW=4.1,1.0,1.46,1.05
for s in (-1,1):wall(G['plaster'],M['plaster'],(s*(GW+UW)/2,YF,FZ),(0,-1,0),(UW-GW)/2,PLATE-FZ)
wall(G['plaster'],M['plaster'],(0,YF,FZ),(0,-1,0),GW,GC-FZ,[hole(s*UWX,UWW,UWZ,UWH,FZ) for s in (-1,1)])
OZ,OH_,OW=6.32,.8,1.35;OB,OT,OH=OZ-.09,OZ+OH_+.09,OW/2+.09
def gx(z):return (GR-.05-z)/KG     # half-width of the gable face at height z
for piece in ([(-GW,GC),(GW,GC),(gx(OB),OB),(-gx(OB),OB)],[(-gx(OB),OB),(-OH,OB),(-OH,OT),(-gx(OT),OT)],
              [(OH,OB),(gx(OB),OB),(gx(OT),OT),(OH,OT)],[(-gx(OT),OT),(gx(OT),OT),(0,GR-.05)]):
    prism(G['plaster'],[(x,YF+.15,z) for x,z in piece],(0,1,0),.3,M['plaster'])
yq=-(RIDGE-GC)/KF                  # cheek walls close the gable sides down to the main slope
for s in (-1,1):
    if GC>PLATE+.01:prism(G['plaster'],[(s*GW,YF,PLATE),(s*GW,YF,GC),(s*GW,yq,GC)],(1,0,0),.1,M['plaster'])
wall(G['plaster'],M['plaster'],(0,UD,JZ),(0,1,0),UW,PLATE-JZ,[hole(x,.9,UWZ,UWH,JZ) for x in (-1.46,1.46)])
ZS=PLATE
for s in (-1,1):
    wall(G['plaster'],M['plaster'],(s*UW,0,FZ),(s,0,0),UD-.3,ZS-FZ,[hole(0,.7,UWZ,.9,FZ)])
    top=[(y,main_z(UW,y)-.06) for y in np.linspace(-(UD-.3),UD-.3,41)]
    prism(G['plaster'],[(s*(UW-.15),y,z) for y,z in [(UD-.3,ZS),(-(UD-.3),ZS)]+top],(1,0,0),.3,M['plaster'])
# ---- timber framing ------------------------------------------------------------------------
y=YF-.03
rail((-UW,y,FZ+.07),(UW,y,FZ+.07),.14);rail((-UW,y,5.3),(UW,y,5.3),.12);rail((-UW,y,PLATE-.08),(UW,y,PLATE-.08),.18)
for x in (-UW+.07,-GW+.08,-.72,0,.72,GW-.08,UW-.07):post(x,y,FZ+.14,PLATE-.17,'y',.14)
for s in (-1,1):
    rail((s*.07,y,3.92),(s*.72,y,3.92),.12);rail((s*(GW-.08),y,3.92),(s*(UW-.07),y,3.92),.12)
    rail((s*.1,y,4.0),(s*.66,y,5.22),.11)
    rail((s*(GW+.02),y,4.0),(s*(UW-.15),y,5.22),.11)
    rail((s*(GW-.1),y,PLATE+.08),(s*.95,y,gable_z(.95)-.32),.11)
zc=OT+.12
for s in (-1,1):post(s*.84,y,PLATE+.02,gable_z(.84)-.1,'y',.12)
rail((-gx(zc)+.05,y,zc),(gx(zc)-.05,y,zc),.14);post(0,y,zc+.07,GR-.1,'y',.14)
for s in (-1,1):rail((s*.1,y,zc+.1),(s*.55,y,gable_z(.55)-.12),.1)
for s in (-1,1):                   # side framing
    x=s*(UW+.03)
    for yy in (-UD+.1,-.6,.6,UD-.1):post(x,yy,FZ+.1,ZS-.05,'x',.14)
    rail((x,-UD,FZ+.07),(x,UD,FZ+.07),.14);rail((x,-UD,ZS-.05),(x,UD,ZS-.05),.16);rail((x,-UD,5.3),(x,UD,5.3),.12)
    rail((x,-UD+.2,4.0),(x,-.7,5.2),.11);rail((x,UD-.2,4.0),(x,.7,5.2),.11)
y=UD+.03                           # rear framing
rail((-UW,y,JZ+.08),(UW,y,JZ+.08),.16);rail((-UW,y,PLATE-.08),(UW,y,PLATE-.08),.18);rail((-UW,y,5.3),(UW,y,5.3),.12)
for x in (-UW+.07,-.8,0,.8,UW-.07):post(x,y,JZ+.16,PLATE-.17,'y',.14)
for s in (-1,1):rail((s*(UW-.15),y,3.6),(s*2.2,y,5.2),.11)
# ---- upper openings ------------------------------------------------------------------------
for s in (-1,1):lights((s*UWX,YF,UWZ),(0,-1,0),UWW,UWH,planter=True)
lights((0,YF,OZ),(0,-1,0),OW,OH_,3)
for x in (-1.46,1.46):lights((x,UD,UWZ),(0,1,0),.9,UWH)
for s in (-1,1):lights((s*UW,0,UWZ),(s,0,0),.7,.9)
# ---- lanterns, gutters and downpipes ---------------------------------------------------------------
for s in (-1,1):lantern(G,M,(s*1.3,GF-.42,2.4),arm=((s*1.3,GF-.09,2.4),(0,-1,0)))
for s in (-1,1):
    xp=s*(UW+.14);yp=GF
    G['detail'].beam((s*EX,-EY-.1,EAVE-.1),(s*GH,-EY-.1,EAVE-.1),.1,.08,M['iron'],grain=False)
    G['detail'].beam((xp,-EY-.1,EAVE-.12),(xp,yp,EAVE-.5),.07,.07,M['iron'],grain=False)
    G['detail'].vessel((xp,yp,0),[(.045,0),(.045,EAVE-.48),(0,EAVE-.48)],M['iron'],12)
    G['detail'].box((xp,yp-.08,.06),(.1,.2,.06),M['iron'],.006)
    for z in (.9,2.3):G['detail'].box(((xp+s*W2)/2,yp,z),(abs(xp)-W2+.02,.03,.04),M['iron'],.004)
    for z in (4.2,5.3):G['detail'].box(((xp+s*UW)/2,yp,z),(abs(xp)-UW+.02,.03,.04),M['iron'],.004)
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
from mathutils import Matrix
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'not grounded: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookTownhouse3'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
# Textures stay in ../Textures (FBX references them relatively); embedding 2k scans makes a 60 MB GLB.
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
cam('Beauty',(-11.5,-15.5,5.0),(80,0,-37),lens=38);cam('Front',(0,-30,4.6),(90,0,0),9.6);cam('Side',(-30,0,4.6),(90,0,-90),9.6);cam('Rear',(0,30,4.6),(90,0,180),9.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Townhouse3-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120_000,f'house triangle budget: {tris}'
manifest=dict(asset='SilverbrookTownhouse3',reference=dict(file='Townhouse3-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_townhouse3.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookTownhouse3',slot_m=list(SLOT),
    concept_asset='BLD-GreenlandsTownhouse-01',variant=3,outputs=['SilverbrookTownhouse3.blend','SilverbrookTownhouse3.glb','SilverbrookTownhouse3.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('TOWNHOUSE3_BUILT',manifest['bounds_m'],tris)
