"""Silverbrook Golden Stag Inn from docs/Game-Design/concept-art/Silverbrook/The Golden Stag Inn.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_goldenstaginn.py
Original procedural geometry interpreting the elevation (14 m wide incl. the hanging sign
and side lean-to, 14 m to the gable finial): steep-hipped slate roof crossed by a taller
half-timbered gable wing, jettied front storey on braced posts, arched stone doorway,
right dormer, tall stone stack, stag banner and bracket sign.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,window,door,lantern
import bpy
import numpy as np
scene=kit.reset(4114)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/The Golden Stag Inn.png'
asset=bpy.data.collections.new('SilverbrookGoldenStagInn');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Ashlar','stone_wall_04'),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    sign=kit.scan('SB_GoldenStagInn_SignBoard','old_planks_02',dark=(.10,.10,.11),damp=False,aged=.8),
    banner=kit.scan('SB_GoldenStagInn_Banner','rough_linen',dark=(.02,.03,.12),damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_GoldenStagInn_Gilt',(.80,.55,.18),.35,1.0))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('stone_wall_04',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBGoldenStagInn_Masonry',asset),plaster=Geo('SM_SBGoldenStagInn_Plaster',asset),timber=Geo('SM_SBGoldenStagInn_Timber',asset),roof=Geo('SM_SBGoldenStagInn_Roof',asset),detail=Geo('SM_SBGoldenStagInn_Details',asset))
# ---- measured layout (metres; elevation read at ~64.6 px/m across, 53 px/m up) --------
SLOT=(14,12,14)                # town-layout slot w x d x h (concept README 'Fits')
W2,D2=5.6,4.0                  # ground-storey outer wall half-extents
JZ,JET=4.0,.45                 # jetty (first floor) height and front projection
YF=-D2-JET                     # jettied upper front wall face
EAVE,RIDGE=5.3,11.0            # main hip roof
EX,EY,RX=6.05,4.8,4.85         # eave half-extents, ridge half-length (steep hips, as drawn)
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
XG,GW,GH,GE,GR=-.85,2.25,2.95,7.6,12.2   # cross wing: centre x, wall half, roof half, eave, ridge (above the main ridge)
KG=(GR-GE)/GH;GY=YF-.6;GB=D2+.45          # cross roof front / rear edges
XD,DH,DW,DE,DR=3.85,.95,.7,8.0,9.9        # dormer
YD=-(RIDGE-6.6)/KF;KD=(DR-DE)/DH
XC,YC=2.05,-.7                 # stack
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return GR-abs(x-XG)*KG
def dormer_z(x):return DR-abs(x-XD)*KD
in_gable=lambda p:abs(p.x-XG)<GW+.04 and p.z<gable_z(p.x)-.005
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.52 and abs(p.y-YC)<.47
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/3) else 0
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
# ---- main hip roof ----------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,in_dormer,in_stack])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
capped((-RX,0,RIDGE+.05),(XG-.7,0,RIDGE+.05),12);capped((XG+.7,0,RIDGE+.05),(RX,0,RIDGE+.05),9)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.05),(sx*RX,0,RIDGE+.05),18)
    G['timber'].box((sx*RX,0,RIDGE+.35),(.16,.16,.6),M['timber'],.02,grain=True,band=band())   # ridge-end finials
    G['timber'].box((sx*RX,0,RIDGE+.67),(.22,.22,.05),M['timber'],.02,grain=True,band=band())
for sy in (-1,1):
    for a,b in ((-EX,XG-GW),(XG+GW,EX)):G['timber'].beam((a,sy*(EY+.02),EAVE-.06),(b,sy*(EY+.02),EAVE-.06),.04,.2,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.06),(sx*(EX+.02),EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- cross wing roof: runs front to back, taller than the main ridge -------------------
LG=math.hypot(GH,GR-GE)
plane((XG-GH,0,GE),(0,-1,0),(GH,0,GR-GE),LG,lambda t:-GB,lambda t:-GY,[under_main,in_stack])
plane((XG+GH,0,GE),(0,1,0),(-GH,0,GR-GE),LG,lambda t:GY,lambda t:GB,[under_main,in_stack])
capped((XG,GY,GR+.05),(XG,GB,GR+.05),30)
for y in (GY,GB):
    yb=y-.04 if y<0 else y+.04
    for s in (-1,1):G['timber'].beam((XG+s*(GH+.05),yb,GE-.08),(XG-s*.16,yb,GR+.16*KG+.03),.08,.26,M['timber'],band=band())
G['timber'].box((XG,GY-.06,GR+.4),(.16,.16,1.2),M['timber'],.02,grain=True,band=band())       # front finial
G['timber'].box((XG,GY-.06,GR+1.03),(.24,.24,.06),M['timber'],.02,grain=True,band=band())
G['detail'].beam((XG,GY-.06,GR+1.06),(XG,GY-.06,GR+1.58),.06,.06,M['iron'],grain=False)
G['detail'].box((XG,GY-.06,GR+1.25),(.12,.12,.1),M['iron'],.01)
# cheek walls close each gable face down to the main slope
GC=gable_z(XG+GW)-.05
for sy,y0 in ((-1,YF+.02),(1,D2-.02)):
    yq=sy*(RIDGE-GC)/KF
    for s in (-1,1):
        x=XG+s*GW;G['plaster'].prism([(x,y0,main_z(x,y0)-.04),(x,y0,GC),(x,yq,GC)],(1,0,0),.1,M['plaster'])
# ---- dormer ----------------------------------------------------------------------
LD=math.hypot(DH,DR-DE)
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:.6,lambda t:-YD+.16,[under_main])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:-.6,[under_main])
capped((XD,YD-.16,DR+.04),(XD,-.8,DR+.04),6)
for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
G['timber'].box((XD,YD-.2,DR+.3),(.1,.1,.6),M['timber'],.015,grain=True,band=band())
DB=main_z(XD,YD)-.1
wall(G['plaster'],M['plaster'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.93,.16,1.24)],th=.12)
G['plaster'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,dormer_z(XD+DW)-.04),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,dormer_z(XD+DW)-.04)],(0,1,0),.12,M['plaster'])
ym=-(RIDGE-(dormer_z(XD+DW)-.04))/KF
for s in (-1,1):G['plaster'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,dormer_z(XD+DW)-.04),(XD+s*DW,ym,dormer_z(XD+DW)-.04)],(1,0,0),.08,M['plaster'])
for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
window(G,M,(XD,YD,DB+.25),(0,-1,0),.75,.9,shutters=False,wall_th=.12,band=band())
# ---- stack -----------------------------------------------------------------------
G['stone'].box((XC,YC,10.95),(.96,.86,4.0),M['stone'],.01)
G['stone'].box((XC,YC,12.6),(1.02,.92,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,12.98),(1.08,.98,.14),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.22,.22):G['detail'].vessel((XC+dx,YC,13.05),[(.12,0),(.11,.55),(.125,.66),(.14,.72),(.11,.72),(.095,.64),(0,.64)],M['pot'])
# ---- ground storey (ashlar) ------------------------------------------------------------
STEP,DS,DOOR=.45,2.1,1.3       # door sill = top step; arch spring above it; door width
WZ,WH=1.2,1.5
def hole(a,w):return (a,w+.18,WZ-.09,WZ+WH+.09)
FW=[(-4.25,1.5),(2.3,.9),(4.6,.9)]
wall(G['stone'],M['stone'],(0,-D2,0),(0,-1,0),W2,JZ,[hole(x,w) for x,w in FW]+[(XG,DOOR,STEP,STEP+DS+DOOR/2)],arch=(XG,DOOR,STEP+DS))
RW=[(-3.6,.9),(2.8,.9)]
wall(G['stone'],M['stone'],(0,D2,0),(0,1,0),W2,JZ,[hole(-x,w) for x,w in RW])
SD=-2.6                        # side door under the lean-to (+X)
wall(G['stone'],M['stone'],(W2,0,0),(1,0,0),D2-.3,JZ,[hole(1.6,.9),(SD,1.0,.15,.15+1.8+.5)],arch=(SD,1.0,.15+1.8))
wall(G['stone'],M['stone'],(-W2,0,0),(-1,0,0),D2-.3,JZ,[hole(1.5,.9),hole(-1.8,.9)])
# plinth course (open at the steps and side door)
for a0,a1 in ((-W2-.04,XG-1.1),(XG+1.1,W2+.04)):G['stone'].box(((a0+a1)/2,-D2-.02,.22),(a1-a0,.1,.44),M['stone'],.01)
G['stone'].box((0,D2+.02,.22),(2*W2+.08,.1,.44),M['stone'],.01)
G['stone'].box((-W2-.02,0,.22),(.1,2*D2,.44),M['stone'],.01)
for b0,b1 in ((-D2,SD-.62),(SD+.62,D2)):G['stone'].box((W2+.02,(b0+b1)/2,.22),(.1,b1-b0,.44),M['stone'],.01)
for k in range(3):G['stone'].box((XG,-D2-.175-.35*k,(3-k)*.075),(2.1,.35,(3-k)*.15),M['stone'],.015)
G['stone'].box((W2+.2,SD,.075),(.4,1.4,.15),M['stone'],.015)
# ---- jettied upper storey and cross-wing faces (plaster) ------------------------------
PLATE=main_z(0,YF)-.04;PLATE_R=main_z(0,D2)-.04
for a0,a1 in ((-W2,XG-GW),(XG+GW,W2)):wall(G['plaster'],M['plaster'],((a0+a1)/2,YF,JZ),(0,-1,0),(a1-a0)/2,PLATE-JZ)
GWZ,GWH,GWX=6.4,1.3,1.35       # wing windows flank the banner
wall(G['plaster'],M['plaster'],(XG,YF,JZ),(0,-1,0),GW,GC-JZ,[(s*GWX,.88,GWZ-.09-JZ,GWZ+GWH+.09-JZ) for s in (-1,1)])
G['plaster'].prism([(XG-GW,YF+.15,GC),(XG+GW,YF+.15,GC),(XG,YF+.15,GR-.05)],(0,1,0),.3,M['plaster'])
RB=[(-W2,XG-GW,-4.35),(XG+GW,W2,3.5)]
for a0,a1,x in RB:wall(G['plaster'],M['plaster'],((a0+a1)/2,D2,JZ),(0,1,0),(a1-a0)/2,PLATE_R-JZ,[(-(x-(a0+a1)/2),.88,.45,1.43)])
wall(G['plaster'],M['plaster'],(XG,D2,JZ),(0,1,0),GW,GC-JZ,[(0,.78,2.6,3.68)])
G['plaster'].prism([(XG-GW,D2-.15,GC),(XG+GW,D2-.15,GC),(XG,D2-.15,GR-.05)],(0,1,0),.3,M['plaster'])
YS0,YS1=YF+.3,D2-.3;ZS=main_z(W2,YS0)-.05     # side upper walls: rectangle, then the roof profile
for s in (-1,1):
    wall(G['plaster'],M['plaster'],(s*W2,(YS0+YS1)/2,JZ),(s,0,0),(YS1-YS0)/2,ZS-JZ,[(0,.88,.51,1.59)])
    top=[(y,main_z(W2,y)-.08) for y in np.linspace(YS0,YS1,41)]
    G['plaster'].prism([(s*(W2-.15),y,z) for y,z in [(YS1,ZS),(YS0,ZS)]+top],(1,0,0),.3,M['plaster'])
# ---- timber framing ------------------------------------------------------------------
PY=-D2-.33                     # jetty posts on stone pads, braced to the bressumer
G['timber'].box((0,-D2-.235,3.99),(2*W2+.1,.57,.22),M['timber'],.015,grain=True,band=band())
for x in np.linspace(-W2+.3,W2-.3,17):G['timber'].box((x,-D2-.5,3.82),(.12,.2,.12),M['timber'],.01,grain=True,band=band())
for x in (-5.45,-3.0,1.3,3.3,5.45):
    G['stone'].box((x,PY,.25),(.44,.44,.5),M['ashlar'],.015,rect=kit.rng.choice(STONES))
    post(x,PY,.5,3.88,'y',.24)
    for s in (-1,1):
        if abs(x+s*.62)<W2:rail((x,PY,3.0),(x+s*.62,PY,3.87),.12)
y=YF-.03
for a0,a1 in ((-W2,XG-GW),(XG+GW,W2)):
    for x in np.linspace(a0+.1,a1-.1,max(2,round((a1-a0)/.9)+1)):post(x,y,4.1,PLATE,'y',.14)
    rail((a0,y,4.85),(a1,y,4.85),.12);rail((a0,y,PLATE-.08),(a1,y,PLATE-.08),.18)
for s in (-1,1):post(XG+s*(GW-.08),y,4.1,gable_z(XG+GW-.08)-.05,'y')
for z,sz in ((PLATE-.08,.18),(6.05,.14),(7.95,.2)):rail((XG-GW,y,z),(XG+GW,y,z),sz)
for s in (-1,1):post(XG+s*.72,y,4.1,7.85,'y',.14)
zc=9.6;hx=(GR-.05-zc)/KG;rail((XG-hx+.05,y,zc),(XG+hx-.05,y,zc),.16);post(XG,y,8.05,GR-.08,'y')
for s in (-1,1):rail((XG+s*(GW-.15),y,8.05),(XG+s*.15,y,zc-.08),.12);rail((XG+s*.1,y,zc+.1),(XG+s*.8,y,GR-.05-.8*KG-.05),.1)
for s in (-1,1):               # side framing
    x=s*(W2+.03)
    for yy in (-D2+.1,0,D2-.1):post(x,yy,.44,JZ-.1,'x')
    rail((x,-D2,.5),(x,D2,.5));rail((x,YF-.05,JZ),(x,D2,JZ),.2)
    for yy in (YS0+.05,-1.5,1.5,YS1-.05):post(x,yy,JZ+.1,main_z(W2,yy)-.12,'x',.14)
y=D2+.03                       # rear framing
for x in (-W2+.1,-1.5,1.5,W2-.1):
    if abs(x-XG)>GW:post(x,y,.44,JZ-.1,'y')
rail((-W2,y,.5),(W2,y,.5));rail((-W2,y,JZ),(W2,y,JZ),.2)
for a0,a1,_ in RB:rail((a0,y,PLATE_R-.08),(a1,y,PLATE_R-.08),.18)
for s in (-1,1):post(XG+s*(GW-.08),y,JZ+.1,gable_z(XG+GW-.08)-.05,'y')
rail((XG-GW,y,7.95),(XG+GW,y,7.95),.2);post(XG,y,8.05,GR-.08,'y')
# ---- openings ---------------------------------------------------------------------
for x,w in FW:window(G,M,(x,-D2,WZ),(0,-1,0),w,WH,shutters=False,planter=True,band=band())
for x,w in RW:window(G,M,(x,D2,WZ),(0,1,0),w,WH,shutters=False,band=band())
window(G,M,(W2,1.6,WZ),(1,0,0),.9,WH,shutters=False,band=band())
for yy in (-1.5,1.8):window(G,M,(-W2,yy,WZ),(-1,0,0),.9,WH,shutters=False,band=band())
for s in (-1,1):window(G,M,(XG+s*GWX,YF,GWZ),(0,-1,0),.7,GWH,shutters=False,planter=True,band=band())
for a0,a1,x in RB:window(G,M,(x,D2,JZ+.54),(0,1,0),.7,.8,shutters=False,band=band())
window(G,M,(XG,D2,JZ+2.69),(0,1,0),.6,.9,shutters=False,band=band())
for s in (-1,1):window(G,M,(s*W2,(YS0+YS1)/2,JZ+.6),(s,0,0),.7,.9,shutters=False,band=band())
door(G,M,(XG,-D2,STEP),(0,-1,0),DOOR,DS,STONES,band=band())
door(G,M,(W2,SD,.15),(1,0,0),1.0,1.8,STONES,band=band())
# ---- the stag: gilded cut-out (head, ears, branching antlers) ----------------------------
def stag(geo,origin,right,up,normal,s):
    o=Vector(origin)+Vector(normal)*.008;R,U,N=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+R*(a*s)+U*(z*s)
    geo.prism([p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,M['gilt'])
    for sx in (-1,1):
        geo.prism([p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
def trim(geo,pts,normal,w=.045):
    n=Vector(normal)*.008
    for a,b in zip(pts,pts[1:]+pts[:1]):geo.beam(Vector(a)+n,Vector(b)+n,.02,w,M['gilt'],grain=False)
# banner on the wing face, hung from an iron rod
yb=YF-.06;bx=[(XG-.6,8.05),(XG+.6,8.05),(XG+.6,6.5),(XG,6.05),(XG-.6,6.5)]
G['detail'].prism([(x,yb,z) for x,z in bx],(0,1,0),.02,M['banner'])
trim(G['detail'],[(x,yb-.01,z) for x,z in bx],(0,-1,0))
stag(G['detail'],(XG,yb-.01,7.0),(1,0,0),(0,0,1),(0,-1,0),1.0)
G['detail'].beam((XG-.75,yb,8.1),(XG+.75,yb,8.1),.035,.035,M['iron'],grain=False)
for s in (-1,1):G['detail'].beam((XG+s*.7,YF+.05,8.1),(XG+s*.7,yb,8.1),.03,.03,M['iron'],grain=False)
# bracket sign on the left wall
AY,AZ,AX=-3.4,3.75,-W2-1.45
G['detail'].beam((-W2+.05,AY,AZ),(AX,AY,AZ),.05,.05,M['iron'],grain=False)
G['detail'].beam((-W2+.05,AY,AZ-.85),(-W2-.95,AY,AZ-.02),.035,.035,M['iron'],grain=False)
G['detail'].box((-W2-.03,AY,AZ-.4),(.06,.1,1.0),M['iron'],.004)
for k in range(8):             # scroll under the arm
    t0,t1=math.tau*k/8,math.tau*(k+1)/8;c=Vector((-W2-.5,AY,AZ-.22));r=.17
    G['detail'].beam(c+Vector((r*math.cos(t0),0,r*math.sin(t0))),c+Vector((r*math.cos(t1),0,r*math.sin(t1))),.02,.02,M['iron'],grain=False)
sx_=-W2-.8;sb=[(sx_-.4,3.45),(sx_+.4,3.45),(sx_+.4,2.65),(sx_,2.3),(sx_-.4,2.65)]
G['detail'].prism([(x,AY,z) for x,z in sb],(0,1,0),.05,M['sign'])
for x in (sx_-.32,sx_+.32):G['detail'].beam((x,AY,AZ-.03),(x,AY,3.44),.02,.02,M['iron'],grain=False)
for n in (-1,1):
    trim(G['detail'],[(x,AY+n*.025,z) for x,z in sb],(0,n,0),.04)
    stag(G['detail'],(sx_,AY+n*.025,2.95),(-n,0,0),(0,0,1),(0,n,0),.75)
# ---- right lean-to over the side door --------------------------------------------------
LX,LE,LW=.95,3.05,3.6
LL=math.hypot(LX,LW-LE)
plane((W2+LX,-3.9,LE),(0,1,0),(-LX,0,LW-LE),LL,lambda t:0,lambda t:2.6,[])
G['timber'].beam((W2+.06,-3.95,LW-.12),(W2+.06,-1.25,LW-.12),.12,.16,M['timber'],band=band())
G['timber'].beam((W2+LX-.12,-3.95,LE-.1),(W2+LX-.12,-1.25,LE-.1),.14,.16,M['timber'],band=band())
for yy in (-3.8,-1.4):
    post(W2+LX-.12,yy,0,LE-.18,'x',.16)
    rail((W2+LX-.12,yy,2.3),(W2+.12,yy,LW-.2),.1)
# ---- lanterns ----------------------------------------------------------------------------
for x in (XG-1.15,XG+1.15):lantern(G,M,(x,-D2-.42,2.75),arm=((x,-D2-.09,2.75),(0,-1,0)))
lantern(G,M,(3.9,-D2-.3,3.8))
lantern(G,M,(AX+.05,AY,AZ-.08))
lantern(G,M,(W2+.5,-1.9,LW-.25))
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
from mathutils import Matrix
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'not grounded: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookGoldenStagInn'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-40,-40,0),(40,-40,0),(40,40,0),(-40,40,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-17.5,-23,7.5),(80,0,-37),lens=38);cam('Front',(0,-40,7.0),(90,0,0),15);cam('Side',(-40,0,7.0),(90,0,-90),15);cam('Rear',(0,40,7.0),(90,0,180),15)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'GoldenStagInn-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=400_000,f'landmark triangle budget: {tris}'
manifest=dict(asset='SilverbrookGoldenStagInn',reference=dict(file='GoldenStagInn-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_goldenstaginn.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookGoldenStagInn',slot_m=list(SLOT),outputs=['SilverbrookGoldenStagInn.blend','SilverbrookGoldenStagInn.glb','SilverbrookGoldenStagInn.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('GOLDENSTAGINN_BUILT',manifest['bounds_m'],tris)
