"""Silverbrook Mill from docs/Game-Design/concept-art/Silverbrook/Mill.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_mill.py
Original procedural geometry interpreting the elevation (slot 12 x 10 x 9 m): hipped slate
roof crossed by a tall half-timbered cross wing whose front gable carries an open loft
hoist door and a spire finial, jettied timber-framed upper storey with leaded windows,
cream sandstone ground storey with a pointed-arch door under a bracketed porch, an open
pent-roofed work bay, right dormer and stack, a left wall jib hoist and, on the right,
a lean-to wheel house over a stone pier carrying a static breastshot water wheel.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,window,lantern,frame
from mathutils import Matrix
import numpy as np
import bpy
scene=kit.reset(4131)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Mill.png'
asset=bpy.data.collections.new('SilverbrookMill');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    stone=kit.scan('SB_Mill_Ashlar','sandstone_blocks_05',dark=(.45,.40,.33)),
    shutter=kit.scan('SB_Mill_Shutter','old_planks_02',dark=(.32,.30,.28),damp=False,aged=.35),
    rope=kit.scan('SB_Mill_Rope','hessian_380',dark=(.45,.36,.22),damp=False,aged=.35),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75))
M['ashlar']=M['stone'];M['planks']=M['timber']   # concept shutters/loft doors are bare oak boards (M['shutter'])
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('sandstone_blocks_05',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.0)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBMill_Masonry',asset),plaster=Geo('SM_SBMill_Plaster',asset),timber=Geo('SM_SBMill_Timber',asset),roof=Geo('SM_SBMill_Roof',asset),detail=Geo('SM_SBMill_Details',asset),wheel=Geo('SM_SBMill_Wheel',asset))
# ---- measured layout (metres; recentred on the slot at the end) ------------------------
SLOT=(12,10,9)                 # town-layout slot w x d x h (concept README 'Fits')
X0,X1=-5.0,2.2                 # mill house side walls
Y0,Y1,YJ=-2.6,2.6,-2.95        # ground front, rear, jettied upper front face
GS=2.9                         # ground storey (stone) top = upper floor
EAVE,RIDGE=4.95,7.25           # main hip roof
YC=(YJ+Y1)/2;EY=(Y1-YJ)/2+.3   # roof centre line (the jetty pushes it forward), eave half-depth
EXL,EXR,R0,R1=X0-.35,X1+.35,-4.6,1.7   # eaves and ridge ends along x
KF=(RIDGE-EAVE)/EY;KS0=(RIDGE-EAVE)/(R0-EXL);KS1=(RIDGE-EAVE)/(EXR-R1)
XG,GH,GE,GR=-2.4,2.3,EAVE,8.1  # cross wing: centre, roof half, eave, ridge (above the main ridge)
KG=(GR-GE)/GH;GYF,GYB=YJ-.2,Y1+.3
XD,DH,DW,DE,DR=1.1,.55,.42,6.55,7.05   # dormer
YD=YC-(RIDGE-5.85)/KF;KD=(DR-DE)/DH
XC,YS=1.8,.45                 # stack on the right hip
XDOOR,STEP=-2.35,.15           # pointed-arch door
BX0,BY=-.45,-1.4               # open work bay: left return, back wall
WX,AZ,WR=4.45,1.7,1.6          # water wheel centre x, axle height, radius
PX0,PX1,PZ=X1,3.95,AZ-.12      # wheel-house stone pier (axle rests on it)
def main_z(x,y):return min(RIDGE-abs(y-YC)*KF,RIDGE-max(0,R0-x)*KS0,RIDGE-max(0,x-R1)*KS1)
def gable_z(x):return GR-abs(x-XG)*KG
def dormer_z(x):return DR-abs(x-XD)*KD
PLATE=main_z(-1,YJ)-.06        # upper wall plate stays under the front slope's deck
in_cross=lambda p:abs(p.x-XG)<GH+.02 and GYF-.05<p.y<GYB+.05 and p.z<gable_z(p.x)-.005
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<YC and p.z<dormer_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.42 and abs(p.y-YS)<.37
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/2) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
def post(x,y,z0,z1,o):G['timber'].box((x,y,(z0+z1)/2),(.2 if o=='y' else .12,.12 if o=='y' else .2,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
# ---- main hip roof -------------------------------------------------------------------
LF=math.hypot(EY,RIDGE-EAVE);LL=math.hypot(R0-EXL,RIDGE-EAVE);LR=math.hypot(EXR-R1,RIDGE-EAVE)
plane((0,YC-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(EXL,R0,LF),lerp(EXR,R1,LF),[in_cross,in_dormer,in_stack])
plane((0,YC+EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EXR,-R1,LF),lerp(-EXL,-R0,LF),[in_cross,in_stack])
plane((EXR,YC,EAVE),(0,1,0),(R1-EXR,0,RIDGE-EAVE),LR,lerp(-EY,0,LR),lerp(EY,0,LR),[in_stack])
plane((EXL,YC,EAVE),(0,-1,0),(R0-EXL,0,RIDGE-EAVE),LL,lerp(-EY,0,LL),lerp(EY,0,LL),[in_cross])
XK=(GR-RIDGE)/KG   # main ridge disappears under the cross wing within XG+-XK
capped((XG+XK,YC,RIDGE+.05),(R1,YC,RIDGE+.05),8);capped((R0,YC,RIDGE+.05),(XG-XK,YC,RIDGE+.05),3)
for sx,ex,rx in ((-1,EXL,R0),(1,EXR,R1)):
    for sy in (-1,1):capped((ex,YC+sy*EY,EAVE+.05),(rx,YC,RIDGE+.05),9)
for a,b,y in ((XG+GH,EXR,YC-EY-.02),(EXL,XG-GH,YC-EY-.02),(XG+GH,EXR,YC+EY+.02),(EXL,XG-GH,YC+EY+.02)):G['timber'].beam((a,y,EAVE-.06),(b,y,EAVE-.06),.04,.2,M['timber'],band=band())
for x in (EXL-.02,EXR+.02):G['timber'].beam((x,YC-EY,EAVE-.06),(x,YC+EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- cross wing: through roof above the main ridge, gables front and rear ---------------
LG=math.hypot(GH,GR-GE)
plane((XG-GH,0,GE),(0,-1,0),(GH,0,GR-GE),LG,lambda t:-GYB,lambda t:-GYF,[under_main])
plane((XG+GH,0,GE),(0,1,0),(-GH,0,GR-GE),LG,lambda t:GYF,lambda t:GYB,[under_main])
capped((XG,GYF,GR+.05),(XG,GYB,GR+.05),20)
for y in (GYF-.04,GYB+.04):
    for s in (-1,1):G['timber'].beam((XG+s*(GH+.05),y,GE-.08),(XG-s*.16,y,GR+.16*KG+.03),.08,.24,M['timber'],band=band())
# spire finial on a king post (concept: oak post, forged iron spike)
G['timber'].box((XG,GYF-.06,GR-.05),(.16,.16,.5),M['timber'],.02,grain=True,band=band())
G['timber'].box((XG,GYF-.06,GR+.22),(.22,.22,.05),M['timber'],.02,grain=True,band=band())
G['detail'].vessel((XG,GYF-.06,GR+.245),[(.05,0),(.05,.06),(.08,.1),(.05,.15),(.018,.54),(0,.58)],M['iron'],12)
GW=(GR-.05-(PLATE+.05))/KG     # gable face half-width where it meets the plate
def gx(z):return (GR-.05-z)/KG
# front gable: plaster with the loft hoist opening
OH,OT=.75,6.65
for piece in ([(-GW,PLATE),(-OH,PLATE),(-OH,OT),(-gx(OT),OT),(-GW,PLATE+.05)],[(OH,PLATE),(GW,PLATE),(GW,PLATE+.05),(gx(OT),OT),(OH,OT)],[(-gx(OT),OT),(gx(OT),OT),(0,GR-.05)]):
    G['plaster'].prism([(XG+x,YJ+.15,z) for x,z in piece],(0,1,0),.3,M['plaster'])
G['plaster'].prism([(XG+x,Y1-.15,z) for x,z in [(-GW,PLATE),(GW,PLATE),(GW,PLATE+.05),(0,GR-.05),(-GW,PLATE+.05)]],(0,1,0),.3,M['plaster'])
# loft recess (shallow, closed by a boarded back) with hoist beam, pulley, rope and hook
RD=.6
G['detail'].box((XG,YJ+RD+.025,(PLATE+OT)/2),(2*OH+.1,.05,OT-PLATE),M['door'],.004,swap=True)
for s in (-1,1):G['detail'].box((XG+s*(OH-.025),YJ+RD/2,(PLATE+OT)/2),(.05,RD,OT-PLATE),M['door'],.004)
G['detail'].box((XG,YJ+RD/2,OT-.025),(2*OH,RD,.05),M['door'],.004)
G['detail'].box((XG,YJ+RD/2+.02,PLATE+.02),(2*OH,RD+.04,.04),M['door'],.004)
G['timber'].beam((XG-OH,YJ+.35,OT-.12),(XG+OH,YJ+.35,OT-.12),.12,.14,M['timber'],band=band())
G['detail'].box((XG,YJ+.35,OT-.3),(.06,.16,.22),M['iron'],.006)
G['detail'].beam((XG,YJ+.35,OT-.41),(XG,YJ+.35,5.75),.03,.03,M['rope'])
G['detail'].box((XG,YJ+.35,5.72),(.05,.05,.08),M['iron'],.006)
# loft door frame and the two board leaves swung open to 90 degrees
for s in (-1,1):G['timber'].box((XG+s*(OH+.07),YJ-.04,(PLATE+OT)/2),(.14,.12,OT-PLATE+.1),M['timber'],.012,grain=True,band=band())
G['timber'].box((XG,YJ-.04,OT+.08),(2*OH+.3,.14,.16),M['timber'],.012,grain=True,band=band())
LH=OT-PLATE-.1
for s in (-1,1):
    xl=XG+s*(OH+.17);yl=YJ-.1-OH/2
    G['detail'].box((xl,yl,PLATE+.05+LH/2),(.05,OH,LH),M['shutter'],.004)
    for z in (.18,.82):G['detail'].box((xl-s*.035,yl,PLATE+.05+z*LH),(.025,OH-.06,.1),M['shutter'],.004)
    G['detail'].beam((xl-s*.035,yl-OH/2+.06,PLATE+.05+.22*LH),(xl-s*.035,yl+OH/2-.06,PLATE+.05+.78*LH),.022,.07,M['shutter'],grain=False)
    for z in (.18,.82):G['detail'].box((xl+s*.028,YJ-.12-OH*.3,PLATE+.05+z*LH),(.008,OH*.6,.035),M['iron'],.002)
# gable half-timbering: corner posts, collar, king post, struts
y=YJ-.03
zc=OT+.3;rail((XG-gx(zc)+.05,y,zc),(XG+gx(zc)-.05,y,zc),.14);post(XG,y,zc+.07,GR-.1,'y')
for s in (-1,1):rail((XG+s*(GW-.2),y,PLATE+.25),(XG+s*(OH+.25),y,OT-.2),.1);rail((XG+s*.1,y,zc+.1),(XG+s*(gx(zc+.6)-.1),y,zc+.6),.1)
y=Y1+.03
rail((XG-gx(6.6)+.05,y,6.6),(XG+gx(6.6)-.05,y,6.6),.14);post(XG,y,PLATE,GR-.1,'y')
# ---- dormer --------------------------------------------------------------------------------
LD=math.hypot(DH,DR-DE);YB=YC-(RIDGE-DR)/KF+.15
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:-YB,lambda t:-YD+.16,[under_main])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:YB,[under_main])
capped((XD,YD-.16,DR+.04),(XD,YB,DR+.04),4)
for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
DB=main_z(XD,YD)-.1
wall(G['plaster'],M['plaster'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.56,DB+.11-DB,DB+.69-DB)],th=.12)
G['plaster'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,dormer_z(XD+DW)-.04),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,dormer_z(XD+DW)-.04)],(0,1,0),.12,M['plaster'])
ym=YC-(RIDGE-(dormer_z(XD+DW)-.04))/KF
for s in (-1,1):G['plaster'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,dormer_z(XD+DW)-.04),(XD+s*DW,ym,dormer_z(XD+DW)-.04)],(1,0,0),.08,M['plaster'])
for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
window(G,M,(XD,YD,DB+.2),(0,-1,0),.38,.4,shutters=False,wall_th=.12,band=band())
# ---- stack on the right hip ------------------------------------------------------------------
G['stone'].box((XC,YS,6.45),(.76,.66,2.5),M['stone'],.01)
G['stone'].box((XC,YS,7.75),(.84,.74,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YS,7.86),(.78,.68,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.17,.17):G['detail'].vessel((XC+dx,YS,7.92),[(.1,0),(.095,.26),(.105,.33),(.12,.37),(.09,.37),(.075,.31),(0,.31)],M['pot'])
# ---- walls ---------------------------------------------------------------------------------
XM=(X0+X1)/2
WZ,WH=3.5,.85;WO=lambda a,w:(a,w+.18,WZ-.09-GS,WZ+WH+.09-GS)    # upper-storey windows (z relative to the wall base GS)
GZ,GWH=1.0,.85;GO=lambda a,w:(a,w+.18,GZ-.09,GZ+GWH+.09)       # ground windows (inferred sides/rear)
UPF=[(-4.3,.5),(-2.73,.42),(-2.0,.42),(-1.27,.42),(-.25,.42),(1.1,.45)]   # front upper: single, triple, single, shuttered
wall(G['plaster'],M['plaster'],(XM,YJ,GS),(0,-1,0),(X1-X0)/2,PLATE-GS,[WO(x-XM,w) for x,w in UPF])
UPB=[(-3.4,.6),(.6,.6)]
wall(G['plaster'],M['plaster'],(XM,Y1,GS),(0,1,0),(X1-X0)/2,PLATE-GS,[WO(-(x-XM),w) for x,w in UPB])
YM=(YJ+Y1)/2;SH=(Y1-YJ)/2-.3
wall(G['plaster'],M['plaster'],(X0,YM,GS),(-1,0,0),SH,PLATE-GS,[WO(-(.3-YM),.6)])
wall(G['plaster'],M['plaster'],(X1,YM,GS),(1,0,0),SH,PLATE-GS)
# ground storey: cream sandstone, pointed door, open bay on the right
FX=(X0+BX0)/2;DOOR_W,DS,DE_=1.2,1.4,.2;DRR=DOOR_W/2+DE_;DH_=math.sqrt(DRR**2-DE_**2)
wall(G['stone'],M['stone'],(FX,Y0,0),(0,-1,0),(BX0-X0)/2,GS,[(XDOOR-FX,DOOR_W,STEP,STEP+DS+DH_)])
wall(G['stone'],M['stone'],(XM,Y1,0),(0,1,0),(X1-X0)/2,GS,[GO(-(x-XM),.6) for x in (-3.4,.6)])
wall(G['stone'],M['stone'],(X0,0,0),(-1,0,0),Y1-.3,GS,[GO(-.5,.6)])
wall(G['stone'],M['stone'],(X1,0,0),(1,0,0),Y1-.3,GS)
G['stone'].box((BX0+.15,(Y0+BY)/2,GS/2),(.3,BY-Y0,GS),M['stone'],0)               # bay return wall
G['detail'].box(((BX0+.3+X1-.3)/2,BY+.06,GS/2),(X1-.3-BX0-.3,.12,GS),M['door'],.004,swap=True)   # boarded bay back
G['detail'].box(((BX0+X1)/2,(YJ+.3+BY)/2,GS-.04),(X1-BX0,BY-YJ-.3,.08),M['door'],.004)          # bay ceiling boards
G['stone'].box(((BX0+X1)/2,(-3.98+BY)/2,.06),(X1-BX0,BY+3.98,.12),M['stone'],.01)               # bay floor slab
# pointed (two-centred) arch: spandrels, jamb quoins, voussoirs, door leaf
P,Rw=frame((XDOOR,Y0,STEP),(0,-1,0));o=Vector((0,-1,0))
def arc(side,n=9):return [(side*(DRR*math.cos(t)-DE_),DS+DRR*math.sin(t)) for t in np.linspace(0,math.acos(DE_/DRR),n)]
for side in (-1,1):
    G['stone'].prism([P(a,-.15,z) for a,z in arc(side)+[(side*DOOR_W/2,DS+DH_)]],o,.3,M['stone'])
G['detail'].prism([P(a,-.2,z) for a,z in [(-DOOR_W/2,0),(DOOR_W/2,0)]+arc(1,13)+arc(-1,13)[::-1][1:]],o,.07,M['door'])
for z in (.4,DS-.15):
    G['detail'].box(P(-.1,-.16,z),(DOOR_W*.75,.012,.055),M['iron'],.004,Rw)
    G['detail'].box(P(-.1-DOOR_W*.375,-.16,z),(.1,.012,.1),M['iron'],.004,Rw)
for k in range(10):
    t=math.tau*k/10;G['detail'].box(P(.33+.06*math.cos(t),-.148,1.0+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,Rw@Matrix.Rotation(t,3,'Y'))
G['detail'].box(P(.33,-.163,1.06),(.07,.01,.07),M['iron'],.004,Rw)
z=0;i=0
while z<DS-.05:
    hgt=min(.3,DS-z);qw=.28 if i%2 else .42
    for side in (-1,1):G['stone'].box(P(side*(DOOR_W/2-.012+qw/2),-.125,z+hgt/2),(qw,.35,hgt-.012),M['ashlar'],.012,Rw,rect=kit.rng.choice(STONES))
    z+=hgt;i+=1
depth=.22;n=5;ta=math.acos(DE_/DRR)
for side in (-1,1):
    for k in range(n):
        t=ta*(k+.5)/n;rm=DRR-.012+depth/2;radial=Vector((side*math.cos(t),0,math.sin(t)));tang=Vector((-math.sin(t),0,side*math.cos(t)))
        G['stone'].box(P(side*(rm*math.cos(t)-DE_),-.125,DS+rm*math.sin(t)),(DRR*ta/n-.014,.35,depth),M['ashlar'],.012,Matrix((Rw@tang,o,Rw@radial)).transposed(),rect=kit.rng.choice(STONES))
G['stone'].box(P(0,-.125,DS+DH_+depth/2-.03),(.16,.35,depth+.06),M['ashlar'],.012,Rw,rect=kit.rng.choice(STONES))   # keystone
G['stone'].box((XDOOR,Y0-.225,STEP/2),(1.7,.45,STEP),M['stone'],.012)                            # door step
# ---- jetty, upper-storey framing -------------------------------------------------------------
G['timber'].box((XM,YJ-.06,2.93),(X1-X0+.12,.2,.2),M['timber'],.015,grain=True,band=band())       # bressumer
G['timber'].box((XM,(YJ+Y0+.05)/2,2.88),(X1-X0,Y0+.05-YJ,.04),M['door'],.004)   # jetty soffit boards
for x in np.arange(X0+.3,X1-.1,.62):G['timber'].box((x,(Y0+YJ)/2-.02,2.8),(.12,Y0-YJ+.1,.12),M['timber'],.01,grain=True,band=band())   # joist ends
for x in (-4.75,-3.65,-1.05):
    G['timber'].beam((x,Y0,1.95),(x,YJ+.02,2.8),.12,.12,M['timber'],band=band())                  # jetty brackets
    G['timber'].box((x,Y0-.06,2.0),(.18,.12,.3),M['timber'],.01,grain=True,band=band())
y=YJ-.03
for x in (X0+.06,-3.75,-.85,.3,X1-.06):post(x,y,3.03,PLATE,'y')
for a,b in ((X0,X1),):rail((a,y,3.12),(b,y,3.12),.16);rail((a,y,PLATE-.08),(b,y,PLATE-.08),.18)
rail((X0,y,4.66),(X1,y,4.66),.14)
rail((-3.65,y,3.25),(-3.15,y,4.55),.1);rail((1.85,y,3.25),(2.06,y,4.55),.1)
for s,x in ((-1,X0-.03),(1,X1+.03)):
    for yy in (YJ+.1,Y1-.1):post(x,yy,GS,PLATE,'x')
    rail((x,YJ,3.0),(x,Y1,3.0));rail((x,YJ,PLATE-.08),(x,Y1,PLATE-.08),.18)
    rail((x,YJ+.2,3.1),(x,YJ+.7,4.6),.12);rail((x,Y1-.2,3.1),(x,Y1-.7,4.6),.12)
post(X0-.03,-.95,GS,PLATE,'x');post(X0-.03,1.25,GS,PLATE,'x')
y=Y1+.03
for x in (X0+.06,-2.4,-1.2,X1-.06):post(x,y,GS,PLATE,'y')
rail((X0,y,3.0),(X1,y,3.0));rail((X0,y,PLATE-.08),(X1,y,PLATE-.08),.18)
# ---- openings ------------------------------------------------------------------------------------
for x,w in UPF:window(G,M,(x,YJ,WZ),(0,-1,0),w,WH,shutters=x>1,band=band())
B=dict(x=-2.0,w=2.25)    # one long planter box under the triple window
G['timber'].box((B['x'],YJ-.15,WZ-.09-.2),(B['w'],.24,.2),M['planks'],.006)
G['detail'].box((B['x'],YJ-.15,WZ-.09-.095),(B['w']-.06,.19,.02),M['soil'],0)
for x in (-2.8,-1.2):G['timber'].beam((x,YJ-.1,3.0),(x,YJ-.25,WZ-.3),.06,.06,M['timber'],band=band())
for x,w in UPB:window(G,M,(x,Y1,WZ),(0,1,0),w,WH,band=band())
window(G,M,(X0,.3,WZ),(-1,0,0),.6,WH,band=band())
for x in (-3.4,.6):window(G,M,(x,Y1,GZ),(0,1,0),.6,GWH,band=band())
window(G,M,(X0,.5,GZ),(-1,0,0),.6,GWH,band=band())
lantern(G,M,(XDOOR-1.05,Y0-.4,2.0),arm=((XDOOR-1.05,Y0-.04,2.0),(0,-1,0)))
lantern(G,M,(.9,(Y0+BY)/2,GS-.16))
# ---- porch over the door: bracketed slate canopy with a king-post truss -------------------------
PH,PE,PR,PYF=1.0,2.15,2.72,-3.45;LP=math.hypot(PH,PR-PE);kp=(PR-PE)/PH
plane((XDOOR-PH,0,PE),(0,-1,0),(PH,0,PR-PE),LP,lambda t:-Y0,lambda t:-PYF,[])
plane((XDOOR+PH,0,PE),(0,1,0),(-PH,0,PR-PE),LP,lambda t:PYF,lambda t:Y0,[])
capped((XDOOR,PYF,PR+.04),(XDOOR,YJ-.1,PR+.04),3)
for s in (-1,1):
    G['timber'].beam((XDOOR+s*(PH+.04),PYF-.03,PE-.07),(XDOOR-s*.1,PYF-.03,PR+.1*kp+.02),.07,.18,M['timber'],band=band())
    rail((XDOOR+s*(PH-.08),Y0,PE-.08),(XDOOR+s*(PH-.08),PYF+.1,PE-.08),.14)
    G['timber'].beam((XDOOR+s*(PH-.08),Y0,1.45),(XDOOR+s*(PH-.08),PYF+.35,PE-.15),.1,.1,M['timber'],band=band())
    G['timber'].box((XDOOR+s*(PH-.08),Y0-.06,1.55),(.14,.12,.5),M['timber'],.01,grain=True,band=band())
    rail((XDOOR+s*(PH-.15),PYF+.1,PE),(XDOOR+s*.08,PYF+.1,PR-.12),.09)
rail((XDOOR-PH,PYF+.1,PE),(XDOOR+PH,PYF+.1,PE),.14);post(XDOOR,PYF+.1,PE+.07,PR-.08,'y')
# ---- open work bay: pent roof on posts ------------------------------------------------------------
PYP,PEZ=-3.8,2.3
for x in (BX0+.08,.95,X1-.02):
    post(x,PYP,.12,PEZ-.08,'y')
    for s in (-1,1):
        if BX0<x+s*.5<X1+.2:G['timber'].beam((x,PYP,1.7),(x+s*.5,PYP,PEZ-.1),.1,.1,M['timber'],band=band())
rail((BX0-.05,PYP,PEZ),(X1+.1,PYP,PEZ),.16)
LPN=math.hypot(-3.12-(PYP-.18),2.78-(PEZ+.04))
plane((0,PYP-.18,PEZ+.04),(1,0,0),(0,-3.12-(PYP-.18),2.78-(PEZ+.04)),LPN,lambda t:BX0-.15,lambda t:X1+.1,[])
# ---- wheel house: stone pier, lean-to on posts ---------------------------------------------------
G['stone'].box(((PX0+PX1)/2,0,PZ/2),(PX1-PX0,4.0,PZ),M['stone'],.01)
G['stone'].box((5.25,0,PZ/2),(.6,2.0,PZ),M['stone'],.01)
LT,LX,LEZ=4.6,4.3,3.78;LN=math.hypot(LX-X1,LT-LEZ)
plane((LX,0,LEZ),(0,1,0),(X1-LX,0,LT-LEZ),LN,lambda t:-2.15,lambda t:2.15,[])
for yy in (-1.9,0,1.9):
    post(3.85,yy,PZ,3.78,'x')
    rail((3.85,yy,3.15),(3.85,yy+(.45 if yy<1 else -.45),3.75),.1)
rail((3.85,-2.1,3.85),(3.85,2.1,3.85),.14);rail((X1+.06,-2.1,4.45),(X1+.06,2.1,4.45),.14)
for yy in (-1.95,1.95):rail((X1,yy,2.6),(3.85,yy,2.6),.14);rail((X1,yy,3.4),(3.85,yy,3.4),.14);rail((X1,yy,4.0),(3.75,yy,3.4),.1)
for x in (3.75,5.25):G['timber'].box((x,0,AZ),(.34,.42,.3),M['timber'],.015,grain=True,band=band())   # axle bearings
# ---- water wheel (static; a separate mesh object so it can be split out to rotate later) ----------
W=G['wheel'];X=Vector((1,0,0))
def at(r,t,x=0):return Vector((WX+x,r*math.cos(t),AZ+r*math.sin(t)))
def wrot(t):return Matrix((Vector((0,-math.sin(t),math.cos(t))),X,Vector((0,math.cos(t),math.sin(t))))).transposed()
NB=24
for k in range(NB):
    t=math.tau*(k+.5)/NB
    for s in (-1,1):W.box(at(WR-.08,t,s*.32),(math.tau*WR/NB+.01,.07,.16),M['timber'],.008,wrot(t),grain=True,band=band())   # rims
    W.box(at(1.22,t),(math.tau*1.22/NB+.01,.6,.04),M['timber'],.006,wrot(t),grain=True,band=band())                         # sole boards
    tb=math.tau*k/NB;W.box(at(1.42,tb),(.04,.6,.38),M['timber'],.006,wrot(tb)@Matrix.Rotation(-.35,3,'Y'),grain=True,band=band())   # buckets
for k in range(8):
    t=math.tau*k/8
    for s in (-1,1):W.beam(at(.2,t,s*.3),at(WR-.1,t,s*.3),.1,.08,M['timber'],band=band())                              # spokes
W.beam((WX-.38,0,AZ),(WX+.38,0,AZ),.5,.5,M['timber'],band=band())                                                        # hub
for s in (-1,1):W.beam((WX+s*.28,0,AZ),(WX+s*.36,0,AZ),.54,.54,M['iron'],grain=False)
W.beam((X1,0,AZ),(5.55,0,AZ),.24,.24,M['timber'],band=band())                                                            # axle
# ---- left wall jib hoist, downpipe -----------------------------------------------------------------
JY=-1.6
G['timber'].beam((X0+.1,JY,4.35),(X0-.95,JY,4.35),.14,.18,M['timber'],band=band())
G['timber'].beam((X0,JY,3.45),(X0-.62,JY,4.28),.12,.12,M['timber'],band=band())
G['detail'].box((X0-.85,JY,4.17),(.1,.16,.18),M['iron'],.006)
G['detail'].beam((X0-.85,JY,4.08),(X0-.85,JY,2.35),.03,.03,M['rope'])
G['detail'].box((X0-.85,JY,2.32),(.05,.05,.08),M['iron'],.006)
G['detail'].beam((X0-.04,-2.45,.05),(X0-.04,-2.45,4.6),.09,.09,M['iron'],grain=False)
G['detail'].beam((X0-.04,-2.45,4.6),(X0-.28,-2.45,4.82),.08,.08,M['iron'],grain=False)
G['detail'].box((X0-.28,-2.45,4.88),(.16,.16,.14),M['iron'],.006)
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'model must stand on z=0, min z {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookMill'
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
cam('Beauty',(-12,-17,5.5),(86,0,-35),lens=40);cam('Front',(0,-30,4.5),(90,0,0),12.6);cam('Side',(30,0,4.5),(90,0,90),12.6);cam('Rear',(0,30,4.5),(90,0,180),12.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Mill-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'{tris} triangles exceeds the 120k house budget'
manifest=dict(asset='SilverbrookMill',reference=dict(file='Mill-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_mill.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookMill',slot_m=list(SLOT),outputs=['SilverbrookMill.blend','SilverbrookMill.glb','SilverbrookMill.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('MILL_BUILT',manifest['bounds_m'],tris)
