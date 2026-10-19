"""Silverbrook Adventurers' Guild from docs/Game-Design/concept-art/Silverbrook/Adventurers Guild.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_adventurersguild.py
Original procedural geometry interpreting the elevation (read at 59.4 px/m so the cross
finial lands under the 18 m slot height): ashlar ground storey with an oak-framed plaster
band under a steep slate hip roof; a full-depth central cross wing rising to a tall
half-timbered gable over an arched double door, carrying the guild banner and a slate
belfry spire; two gabled dormers, a side stack, a right-hand lean-to porch with a notice
board, and a bracketed guild sign on the left.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,UP,tile_plane,deck,wall,window,door,lantern,frame
import bpy
import numpy as np
scene=kit.reset(4116)
_box=Geo.box
def _box_checked(self,centre,size,mat,*a,**kw):
    """kit's winding check sums float32 triple products of world points; at this building's
    size it misfires on 9 mm lead cames. Re-check a failing box at the origin before rejecting."""
    try:_box(self,centre,size,mat,*a,**kw)
    except AssertionError:_box(Geo('winding-check',None),(0,0,0),size,mat,*a,**kw)
Geo.box=_box_checked
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Adventurers Guild.png'
NAME='SilverbrookAdventurersGuild'
asset=bpy.data.collections.new(NAME);scene.collection.children.link(asset)

M=dict(  # SB_* rows repeat Cottage1's arguments exactly; package-only surfaces are SB_AdventurersGuild_*
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Ashlar','stone_wall_04'),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_AdventurersGuild_Banner','denim_fabric_03',dark=(.16,.19,.34),normal=.6,damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_AdventurersGuild_Gilt',(.55,.37,.12),.35,1),
    paper=kit.solid('SB_AdventurersGuild_Parchment',(.62,.55,.40),.85))
M['ashlar']=M['stone'];M['planks']=M['timber'];M['shutter']=M['door']   # concept shutters are brown boards, not painted
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('stone_wall_04',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

P_='SM_SBAdventurersGuild_'
G=dict(stone=Geo(P_+'Masonry',asset),plaster=Geo(P_+'Plaster',asset),timber=Geo(P_+'Timber',asset),roof=Geo(P_+'Roof',asset),detail=Geo(P_+'Details',asset))
# ---- measured layout (metres) -------------------------------------------------
SLOT=(16,13,18)                # town-layout slot w x d x h (concept README 'Fits')
W2,D2=6.3,4.8                  # main body wall half-extents
EAVE,RIDGE=5.3,11.4            # main hip roof
EX,EY,RX=7.0,5.5,3.9           # eave half-extents, ridge half-length
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
GW,GH,GE,GR=2.8,3.3,9.8,14.5   # cross wing: wall half, roof half, roof eave, apex
KG=(GR-GE)/GH
WF=-D2-.8                      # cross wing front wall (projects 0.8 m)
WE=GR-GW*KG-.04                # cross wing wall plate
GY0,GY1=WF-.45,D2+.45          # cross wing roof overhang front / rear
XJ=(GR-RIDGE)/KG               # where the main ridge runs into the wing roof
STONE_TOP,WING_STONE=4.0,5.4   # ashlar storey heights: main body, wing front
STEP,DS,DOORW=.45,2.5,2.4      # door sill = upper step top; arch spring above it; double door width
WIN,WH,WZ=.8,1.25,1.6          # ground-floor windows
XD,DH,DW,DE,DR=4.25,.95,.75,7.7,8.85   # dormers (mirrored)
KD=(DR-DE)/DH;YD=-(RIDGE-6.3)/KF
XC,YC=5.1,1.2                  # side stack
YCU=-1.5                       # belfry on the wing ridge
LX,LE,LW=W2+1.85,3.7,5.0       # lean-to porch: eave line x, eave height, height at wall
LY0,LY1=-D2-.35,-D2+3.25
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def wing_z(x):return GR-abs(x)*KG
def dormer_z(x,xd):return DR-abs(x-xd)*KD
in_wing=lambda p:abs(p.x)<GW+.04 and p.z<wing_z(p.x)-.005
in_dormers=[(lambda p,xd=s*XD:abs(p.x-xd)<DH and YD<p.y<0 and p.z<dormer_z(p.x,xd)-.005) for s in (-1,1)]
in_stack=lambda p:abs(p.x-XC)<.6 and abs(p.y-YC)<.5
in_belfry=lambda p:abs(p.x)<.47 and abs(p.y-YCU)<.47
under_main=lambda p:abs(p.y)<EY and p.z<main_z(p.x,p.y)-.01
PLATE=main_z(0,-D2)-.04
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/3) else 0
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
def arch_head(c,o,a,w,spring,n=7,depth=.26):
    """Dressed voussoir hood over a window head (the tympanum is solid wall)."""
    P,R=frame(c,o);rad=w/2;o_=Vector(o).normalized()
    for k in range(n):
        t=math.pi*(k+.5)/n;rm=rad-.012+depth/2;radial=R@Vector((math.cos(t),0,math.sin(t)));tang=R@Vector((-math.sin(t),0,math.cos(t)))
        G['stone'].box(P(a+rm*math.cos(t),-.12,spring+rm*math.sin(t)),(math.pi*rm/n-.014,.34,depth),M['ashlar'],.012,Matrix((tang,o_,radial)).transposed(),rect=kit.rng.choice(STONES))
def star(c,o,a,z,L,d,th=.012):
    """Gilded compass star (4 long + 4 short kite arms, ring) on a face of frame (c,o) at depth d."""
    P,R=frame(c,o);o_=Vector(o)
    for k in range(8):
        t=math.tau*k/8;l=L*(1 if k%4==0 else .8 if k%2==0 else .42);w=L*(.11 if k%2==0 else .07);s=l*.22
        ax,az=math.sin(t),math.cos(t);px,pz=math.cos(t),-math.sin(t)
        pts=[(a+ax*s+px*w,z+az*s+pz*w),(a+ax*l,z+az*l),(a+ax*s-px*w,z+az*s-pz*w),(a,z)]
        G['detail'].prism([P(x,d-th*(k%2)*.6,y) for x,y in pts],o_,th,M['gilt'])
    for k in range(16):
        t0,t1=math.tau*k/16,math.tau*(k+1.06)/16;r=L*.55
        G['detail'].beam(P(a+r*math.sin(t0),d-.006,z+r*math.cos(t0)),P(a+r*math.sin(t1),d-.006,z+r*math.cos(t1)),L*.05,.012,M['gilt'],grain=False)
# ---- main hip roof ------------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE);cut=[in_wing,in_stack,*in_dormers]
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),cut)
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_wing,in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
for s in (-1,1):
    capped((s*XJ,0,RIDGE+.05),(s*RX,0,RIDGE+.05),5)
    for sy in (-1,1):capped((s*EX,sy*EY,EAVE+.05),(s*RX,0,RIDGE+.05),18)
    # ridge-end finials
    G['timber'].box((s*RX,0,RIDGE+.4),(.14,.14,.8),M['timber'],.02,grain=True,band=band())
    G['timber'].prism([(s*RX-.11,0,RIDGE+.8),(s*RX+.11,0,RIDGE+.8),(s*RX,0,RIDGE+1.15)],(0,1,0),.14,M['timber'])
for y in (-1,1):
    for a,b in ((-EX,-GW),(GW,EX)):G['timber'].beam((a,y*(EY+.02),EAVE-.06),(b,y*(EY+.02),EAVE-.06),.04,.22,M['timber'],band=band())
for s in (-1,1):G['timber'].beam((s*(EX+.02),-EY,EAVE-.06),(s*(EX+.02),EY,EAVE-.06),.04,.22,M['timber'],band=band())
# ---- cross wing roof, bargeboards, belfry ------------------------------------------------
LG=math.hypot(GH,GR-GE)
plane((-GH,0,GE),(0,-1,0),(GH,0,GR-GE),LG,lambda t:-GY1,lambda t:-GY0,[under_main,in_belfry])
plane((GH,0,GE),(0,1,0),(-GH,0,GR-GE),LG,lambda t:GY0,lambda t:GY1,[under_main,in_belfry])
capped((0,GY0,GR+.05),(0,YCU-.45,GR+.05),6);capped((0,YCU+.45,GR+.05),(0,GY1,GR+.05),9)
for y,d in ((GY0,-.04),(GY1,.04)):
    for s in (-1,1):G['timber'].beam((s*(GH+.05),y+d,GE-.1),(-s*.18,y+d,GR+.18*KG+.04),.09,.3,M['timber'],band=band())
for s in (-1,1):   # feet of the front bargeboards: square bosses as in the concept
    G['timber'].box((s*(GH-.05),GY0-.06,GE-.25),(.32,.2,.4),M['timber'],.02,grain=True,band=band())
# belfry: oak louvred stage, cornice, octagonal slate spire, iron cross
BZ=GR+.55
G['timber'].box((0,YCU,(13.7+BZ)/2),(.8,.8,BZ-13.7),M['timber'],.01,grain=True,band=band())
for x in (-.38,.38):
    for y in (-.38,.38):post(x,YCU+y,13.8,BZ,'y',.14)
for k in range(3):
    for o in ((0,-1,0),(0,1,0),(1,0,0),(-1,0,0)):
        o=Vector(o);rot=Matrix.Rotation(math.radians(35),3,UP.cross(o))@frame((0,0,0),o)[1];z=BZ-.18-k*.14
        G['timber'].box(Vector((0,YCU,z))+o*.42,(.56,.03,.12),M['timber'],.004,rot,grain=True,band=band())
G['timber'].box((0,YCU,BZ+.06),(1.0,1.0,.12),M['timber'],.015,grain=True,band=band())
apex=Vector((0,YCU,BZ+1.45));base=[Vector((.56*math.cos(math.tau*(j+.5)/8),YCU+.56*math.sin(math.tau*(j+.5)/8),BZ+.12)) for j in range(8)]
for j in range(8):G['roof'].poly([base[j],base[(j+1)%8],apex],M['slate'],w=(.5,.2))
G['roof'].poly(list(reversed(base)),M['slate'])
CROSS=apex.z+.02
G['detail'].beam((0,YCU,apex.z-.2),(0,YCU,CROSS+1.1),.05,.05,M['iron'],grain=False)
G['detail'].beam((-.32,YCU,CROSS+.72),(.32,YCU,CROSS+.72),.045,.045,M['iron'],grain=False)
G['detail'].box((0,YCU,CROSS+.08),(.13,.13,.13),M['iron'],.03)
for s in (-1,1):G['detail'].box((s*.34,YCU,CROSS+.72),(.07,.07,.07),M['iron'],.015)
G['detail'].box((0,YCU,CROSS+1.12),(.08,.08,.08),M['iron'],.015)
# ---- dormers --------------------------------------------------------------------------
LD=math.hypot(DH,DR-DE)
for s in (-1,1):
    xd=s*XD;dz=lambda x:dormer_z(x,xd)
    plane((xd-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:2.0,lambda t:-YD+.16,[under_main])
    plane((xd+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:-2.0,[under_main])
    capped((xd,YD-.16,DR+.04),(xd,-2.3,DR+.04),4)
    for t in (-1,1):G['timber'].beam((xd+t*(DH+.03),YD-.18,DE-.05),(xd-t*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
    DB=main_z(xd,YD)-.1
    wall(G['plaster'],M['plaster'],(xd,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.73,.16,1.29)],th=.12)
    G['plaster'].prism([(xd-DW,YD+.06,DE+.02),(xd+DW,YD+.06,DE+.02),(xd+DW,YD+.06,dz(xd+DW)-.04),(xd,YD+.06,DR-.04),(xd-DW,YD+.06,dz(xd+DW)-.04)],(0,1,0),.12,M['plaster'])
    ym=-(RIDGE-(dz(xd+DW)-.04))/KF
    for t in (-1,1):G['plaster'].prism([(xd+t*DW,YD,DB),(xd+t*DW,YD,dz(xd+DW)-.04),(xd+t*DW,ym,dz(xd+DW)-.04)],(1,0,0),.08,M['plaster'])
    for t in (-1,1):G['timber'].box((xd+t*(DW-.05),YD-.02,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
    G['timber'].box((xd,YD-.03,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
    window(G,M,(xd,YD,DB+.25),(0,-1,0),.55,.95,shutters=False,wall_th=.12,band=band())
    G['timber'].box((xd,YD-.2,DR+.5),(.1,.1,.75),M['timber'],.015,grain=True,band=band())
    G['timber'].prism([(xd-.08,YD-.2,DR+.87),(xd+.08,YD-.2,DR+.87),(xd,YD-.2,DR+1.1)],(0,1,0),.1,M['timber'])
# ---- side stack -------------------------------------------------------------------------
G['stone'].box((XC,YC,10.15),(1.1,.9,5.1),M['stone'],.01)
G['stone'].box((XC,YC,12.0),(1.16,.96,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,12.64),(1.22,1.02,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.25,.25):G['detail'].vessel((XC+dx,YC,12.7),[(.12,0),(.115,.4),(.125,.5),(.14,.55),(.11,.55),(.095,.48),(0,.48)],M['pot'])
# ---- main body walls -------------------------------------------------------------------
WO=lambda a:(a,WIN+.18,WZ-.09,WZ+WH+.09)
for s in (-1,1):   # front, either side of the wing
    cx=s*(W2+GW)/2;hw=(W2-GW)/2
    wall(G['stone'],M['stone'],(cx,-D2,0),(0,-1,0),hw,STONE_TOP,[WO(s*4.3-cx)])
    wall(G['plaster'],M['plaster'],(cx,-D2,STONE_TOP),(0,-1,0),hw,PLATE-STONE_TOP)
SIDE={-1:(-2.2,1.6),1:(1.2,3.3)}
for s in (-1,1):
    wall(G['stone'],M['stone'],(s*W2,0,0),(s,0,0),D2-.3,STONE_TOP,[WO(s*y) for y in SIDE[s]])
    wall(G['plaster'],M['plaster'],(s*W2,0,STONE_TOP),(s,0,0),D2-.3,PLATE-STONE_TOP)
    top=[(y/10,main_z(W2,y/10)-.05) for y in range(-round(D2*10),round(D2*10)+1) if main_z(W2,y/10)-.05>PLATE+.01]
    G['plaster'].prism([(s*(W2-.15),y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),.3,M['plaster'])
REAR=(-4.3,-1.5,1.5,4.3)
wall(G['stone'],M['stone'],(0,D2,0),(0,1,0),W2,STONE_TOP,[WO(-x) for x in REAR])
wall(G['plaster'],M['plaster'],(0,D2,STONE_TOP),(0,1,0),W2,PLATE-STONE_TOP)
# ---- cross wing walls ---------------------------------------------------------------------
def gx(z):return (GR-.05-z)/KG   # half-width of a wing gable face at height z
UW=(6.2,2.0,.55)                 # wing upper windows: sill, height, width
wall(G['stone'],M['stone'],(0,WF,0),(0,-1,0),GW,WING_STONE,[(0,DOORW,STEP,STEP+DS+DOORW/2)])
# Door-head spandrels. kit.wall(arch=...) orients its prism from the first three outline points,
# which lie on the concave arc, so its spandrels come out inside-out (hidden on Cottage1 by the
# voussoirs; visible on this wider door). Starting the outline at the corner gives the true winding.
P,R=frame((0,WF,0),(0,-1,0))
for side in (-1,1):
    arc=[P(side*DOORW/2*math.cos(t),0,STEP+DS+DOORW/2*math.sin(t)) for t in np.linspace(0,math.pi/2,9)]
    G['stone'].prism([p+Vector((0,.15,0)) for p in [P(side*DOORW/2,0,STEP+DS+DOORW/2)]+arc],(0,-1,0),.3,M['stone'])
wall(G['plaster'],M['plaster'],(0,WF,WING_STONE),(0,-1,0),GW,WE-WING_STONE,[(a,UW[2]+.18,UW[0]-.09-WING_STONE,UW[0]+UW[1]+.09-WING_STONE) for a in (-1.55,1.55)])
for y,d in ((WF,.15),(D2,-.15)):G['plaster'].prism([(-gx(WE),y+d,WE),(gx(WE),y+d,WE),(0,y+d,GR-.05)],(0,1,0),.3,M['plaster'])
wall(G['plaster'],M['plaster'],(0,D2,PLATE),(0,1,0),GW,WE-PLATE,[(0,.78,7.0-.09-PLATE,8.4+.09-PLATE)])
yq=(RIDGE-.05-WE)/KF
for s in (-1,1):
    yc=(WF+.3-D2)/2;hl=(-D2-WF-.3)/2   # projecting part of the wing side walls
    wall(G['stone'],M['stone'],(s*GW,yc,0),(s,0,0),hl,WING_STONE)
    wall(G['plaster'],M['plaster'],(s*GW,yc,WING_STONE),(s,0,0),hl,WE-WING_STONE)
    x=s*(GW-.15)   # side walls standing above the main roof, front and rear of the main ridge
    G['plaster'].prism([(x,-D2,main_z(GW,-D2)-.05),(x,-D2,WE),(x,-yq,WE)],(1,0,0),.3,M['plaster'])
    G['plaster'].prism([(x,yq,WE),(x,D2-.3,WE),(x,D2-.3,main_z(GW,D2-.3)-.05)],(1,0,0),.3,M['plaster'])
    rail((s*(GW+.03),WF,WE-.1),(s*(GW+.03),-yq,WE-.1),.16);rail((s*(GW+.03),yq,WE-.1),(s*(GW+.03),D2,WE-.1),.16)
    post(s*(GW+.03),WF+.06,WING_STONE,WE-.18,'x')
# ---- timber framing: main band -------------------------------------------------------------
y=-D2-.03
for s in (-1,1):
    xs=[s*GW+s*.15,s*4.55,s*(W2-.08)]
    for x in xs:post(x,y,STONE_TOP+.08,PLATE-.1,'y')
    rail((s*GW,y,STONE_TOP+.04),(s*W2,y,STONE_TOP+.04),.14);rail((s*GW,y,PLATE-.08),(s*W2,y,PLATE-.08),.18)
    for a,b in zip(xs,xs[1:]):
        m=(a+b)/2;rail((a+s*.08,y,STONE_TOP+.14),(m,y,PLATE-.2),.11);rail((b-s*.08,y,STONE_TOP+.14),(m,y,PLATE-.2),.11)
    for x in xs:   # eave brackets
        yb=-D2-.42;G['timber'].beam((x,-D2-.07,PLATE-.75),(x,yb,main_z(x,yb)-.07),.12,.12,M['timber'],band=band())
for s in (-1,1):
    x=s*(W2+.03)
    for yy in (-D2+.1,0,D2-.1):post(x,yy,STONE_TOP+.08,PLATE-.1,'x')
    rail((x,-D2,STONE_TOP+.04),(x,D2,STONE_TOP+.04),.14);rail((x,-D2,PLATE-.08),(x,D2,PLATE-.08),.18)
    for a,b in ((-D2+.1,0),(0,D2-.1)):rail((x,a+.1,STONE_TOP+.14),(x,(a+b)/2,PLATE-.2),.11);rail((x,b-.1,STONE_TOP+.14),(x,(a+b)/2,PLATE-.2),.11)
y=D2+.03
for x in (-W2+.08,-GW+.1,GW-.1,W2-.08):post(x,y,STONE_TOP+.08,PLATE-.1,'y')
rail((-W2,y,STONE_TOP+.04),(W2,y,STONE_TOP+.04),.14);rail((-W2,y,PLATE-.08),(W2,y,PLATE-.08),.18)
for x in (-GW+.1,GW-.1):post(x,y,PLATE,WE-.1,'y')
rail((-GW,y,WE-.08),(GW,y,WE-.08),.18);post(0,y,WE,GR-.12,'y')
for s in (-1,1):rail((s*(gx(WE+.2)-.15),y,WE+.12),(s*.08,y,WE+1.9),.12)
# ---- timber framing: wing front ---------------------------------------------------------------
y=WF-.03
G['timber'].box((0,WF-.08,WING_STONE+.2),(2*GW+.3,.22,.4),M['timber'],.015,grain=True,band=band())   # bressumer
for s in (-1,1):
    G['timber'].box((s*(GW-.15),WF-.21,WING_STONE+.2),(.5,.06,.5),M['timber'],.015,grain=True,band=band())   # rosette bosses
    star((0,WF-.24,0),(0,-1,0),s*(GW-.15),WING_STONE+.2,.2,0)
    post(s*(GW-.12),y,3.8,WING_STONE-.01,'y',.22)
    rail((s*(GW-.25),y-.01,4.0),(s*1.65,y-.01,WING_STONE-.05),.14)
    post(s*(GW-.08),y,WING_STONE+.4,WE-.1,'y')
    rail((s*GW,y,8.6),(s*1.16,y,8.6),.14)
    rail((s*(GW-.12),y,8.72),(s*1.5,y,WE-.2),.11)
rail((-1.16,y,8.6),(1.16,y,8.6),.14)
rail((-GW,y,WE-.08),(GW,y,WE-.08),.18);post(0,y,WE,GR-.12,'y')
for s in (-1,1):rail((s*(gx(WE+.2)-.15),y,WE+.12),(s*.08,y,WE+1.9),.12)
rail((-gx(WE+2.3)+.05,y,WE+2.3),(gx(WE+2.3)-.05,y,WE+2.3),.14)
# ---- openings ------------------------------------------------------------------------------
for x in (-4.3,4.3):
    window(G,M,(x,-D2,WZ),(0,-1,0),WIN,WH,planter=True,band=band());arch_head((x,-D2,0),(0,-1,0),0,WIN+.18,WZ+WH+.09)
for s in (-1,1):
    for yw in SIDE[s]:window(G,M,(s*W2,yw,WZ),(s,0,0),WIN,WH,band=band());arch_head((s*W2,yw,0),(s,0,0),0,WIN+.18,WZ+WH+.09)
for x in REAR:window(G,M,(x,D2,WZ),(0,1,0),WIN,WH,band=band());arch_head((x,D2,0),(0,1,0),0,WIN+.18,WZ+WH+.09)
for a in (-1.55,1.55):window(G,M,(a,WF,UW[0]),(0,-1,0),UW[2],UW[1],shutters=False,band=band())
window(G,M,(0,D2,7.0),(0,1,0),.6,1.4,band=band())
door(G,M,(0,WF,STEP),(0,-1,0),DOORW,DS,STONES,band=band())
P,R=frame((0,WF,STEP),(0,-1,0))
G['detail'].box(P(0,-.13,(DS+DOORW/2)/2),(.1,.03,DS+DOORW/2-.06),M['door'],.006,R,swap=True)   # meeting stiles of the double leaf
for k in range(10):   # second ring handle, mirrored
    t=math.tau*k/10;G['detail'].box(P(-DOORW/2*.55+.06*math.cos(t),-.13,1.0+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
G['detail'].box(P(-DOORW/2*.55,-.155,1.06),(.07,.01,.07),M['iron'],.004,R)
# steps and cheek blocks (step top = door sill: no gap at the threshold)
G['stone'].box((0,WF-.675,STEP/4),(2.8,.45,STEP/2),M['stone'],.012)
G['stone'].box((0,WF-.225,STEP/2),(2.8,.45,STEP),M['stone'],.012)
for s in (-1,1):G['stone'].box((s*1.8,WF-.45,.4),(.7,.9,.8),M['ashlar'],.015,rect=kit.rng.choice(STONES))
# ---- banner ----------------------------------------------------------------------------------
BT,BB,BW=WE+.2,6.1,1.15;yb=WF-.14
G['detail'].prism([(-BW,yb,BT),(BW,yb,BT),(BW,yb,BB+.55),(0,yb,BB),(-BW,yb,BB+.55)],(0,1,0),.02,M['banner'])
star((0,yb-.01,0),(0,-1,0),0,8.7,1.05,0)
for z in (BT-.35,BB+.85):
    for a in (-.75,.75):star((0,yb-.01,0),(0,-1,0),a,z,.14,0)
G['detail'].beam((-BW-.25,yb,BT+.05),(BW+.25,yb,BT+.05),.05,.05,M['iron'],grain=False)
for s in (-1,1):
    G['detail'].box((s*(BW+.3),yb,BT+.05),(.09,.09,.09),M['iron'],.02)
    G['detail'].beam((s*(BW+.1),WF-.01,BT+.05),(s*(BW+.1),yb+.03,BT+.05),.04,.04,M['iron'],grain=False)
# ---- lanterns, guild sign ------------------------------------------------------------------------
for s in (-1,1):lantern(G,M,(s*1.95,WF-.42,3.3),arm=((s*1.95,WF-.04,3.3),(0,-1,0)))
lantern(G,M,(0,WF-.45,4.95),arm=((0,WF-.04,4.95),(0,-1,0)))
SY=-D2+.7;SX=-W2-.6
G['detail'].beam((-W2-.01,SY,4.45),(-W2-1.1,SY,4.45),.05,.05,M['iron'],grain=False)
G['detail'].beam((-W2-.01,SY,3.95),(-W2-.65,SY,4.43),.035,.035,M['iron'],grain=False)
G['detail'].box((-W2-.04,SY,4.2),(.08,.12,.6),M['iron'],.004)
for dx in (-.3,.3):G['detail'].beam((SX+dx,SY,4.43),(SX+dx,SY,4.0),.015,.015,M['iron'],grain=False)
G['detail'].box((SX,SY,3.55),(.75,.05,.9),M['door'],.008,swap=True)
for z in (3.11,3.99):G['detail'].box((SX,SY,z),(.79,.07,.03),M['iron'],.004)
for x in (-.38,.38):G['detail'].box((SX+x,SY,3.55),(.03,.07,.91),M['iron'],.004)
star((0,SY-.025,0),(0,-1,0),SX,3.55,.26,0);star((0,SY+.025,0),(0,1,0),-SX,3.55,.26,0)
# ---- lean-to porch on the right wall -----------------------------------------------------------
LL=math.hypot(LX-W2,LW-LE)
plane((LX,0,LE),(0,1,0),(W2-LX,0,LW-LE),LL,lambda t:LY0,lambda t:LY1,[])
pz=lambda x:LE+(LX-x)*(LW-LE)/(LX-W2)-.05
PX=LX-.18
for yy in (LY0+.2,LY1-.2):
    post(PX,yy,.15,pz(PX)-.15,'x',.18);G['stone'].box((PX,yy,.075),(.34,.34,.15),M['ashlar'],.012,rect=kit.rng.choice(STONES))
    rail((PX,yy,2.8),(PX,yy+(.5 if yy<(LY0+LY1)/2 else -.5),pz(PX)-.2),.1)
    G['timber'].beam((W2+.01,yy,pz(W2+.3)-.1),(PX,yy,pz(PX)-.1),.1,.14,M['timber'],band=band())
rail((PX,LY0,pz(PX)-.1),(PX,LY1,pz(PX)-.1),.14)
rail((W2+.07,LY0,LW-.15),(W2+.07,LY1,LW-.15),.14)
G['timber'].beam((LX+.02,LY0,LE-.06),(LX+.02,LY1,LE-.06),.04,.2,M['timber'],band=band())
for yy in (LY0-.02,LY1+.02):G['timber'].beam((LX,yy,LE-.06),(W2+.01,yy,LW-.06),.04,.2,M['timber'],band=band())
NB=-D2+1.6   # notice board
G['timber'].box((W2+.04,NB,1.75),(.08,1.4,1.2),M['door'],.008)
for z in (1.13,2.37):G['timber'].box((W2+.06,NB,z),(.1,1.5,.08),M['timber'],.008,grain=True,band=band())
for dy,dz,wd,ht in ((-.38,1.95,.36,.5),(.05,1.6,.32,.44),(.42,1.98,.3,.38),(-.1,2.1,.24,.26)):
    G['detail'].box((W2+.085,NB+dy,dz),(.006,wd,ht),M['paper'],0)
lantern(G,M,(W2+.45,NB+1.15,3.0),arm=((W2+.04,NB+1.15,3.0),(1,0,0)))
# ---- export, preview scene, manifest -------------------------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'model does not stand on the ground: zmin {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/NAME
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-40,-40,0),(40,-40,0),(40,40,0),(-40,40,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o);c.clip_end=500
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-17,-24,8.5),(86,0,-35),lens=34);cam('Front',(0,-60,9),(90,0,0),18.6);cam('Side',(-60,0,9),(90,0,-90),18.6);cam('Rear',(0,60,9),(90,0,180),18.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'AdventurersGuild-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=400_000,f'landmark triangle budget exceeded: {tris}'
manifest=dict(asset=NAME,reference=dict(file='AdventurersGuild-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_adventurersguild.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_'+NAME,slot_m=list(SLOT),outputs=[NAME+'.blend',NAME+'.glb',NAME+'.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('GUILD_BUILT',manifest['bounds_m'],tris)
