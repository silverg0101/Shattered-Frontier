"""Silverbrook Blacksmith from docs/Game-Design/concept-art/Silverbrook/Blacksmith.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_blacksmith.py
Original procedural geometry interpreting the elevation (slot 14 x 12 x 14 m): steep
hipped slate roof, jettied half-timbered front gable over an open forge bay (brick
hearth and hood, anvil on a stump), ashlar ground storey with an arched door and a
shuttered window, a timber/plaster band under the eaves, a right dormer, a ridge
stack with two pots, a forge flue stack with an iron stovepipe and a right lean-to.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,window,door,lantern
import bpy
scene=kit.reset(4114)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Blacksmith.png'
asset=bpy.data.collections.new('SilverbrookBlacksmith');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Ashlar','stone_wall_04'),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    brick=kit.scan('SB_Blacksmith_ForgeBrick','medieval_red_brick',dark=(.10,.08,.07),aged=.35),
    banner=kit.scan('SB_Blacksmith_Banner','denim_fabric_03',dark=(.12,.14,.30),damp=False,aged=.75),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    fire=kit.solid('SB_Blacksmith_ForgeFire',(.25,.08,.02),.9,0,(1.0,.32,.05),9),
    gilt=kit.solid('SB_Blacksmith_Gilt',(.62,.45,.18),.4,.85))
M['ashlar']=M['stone'];M['planks']=M['timber'];M['shutter']=M['door']   # concept shutters are bare oak boards
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('stone_wall_04',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBBlacksmith_Masonry',asset),plaster=Geo('SM_SBBlacksmith_Plaster',asset),timber=Geo('SM_SBBlacksmith_Timber',asset),roof=Geo('SM_SBBlacksmith_Roof',asset),detail=Geo('SM_SBBlacksmith_Details',asset))
# ---- measured layout (metres) -------------------------------------------------
SLOT=(14,12,14)                # town-layout slot w x d x h (concept README 'Fits')
W2,D2=5.2,3.8                  # outer wall half-extents
EAVE,RIDGE=5.4,11.6            # main hip roof
EX,EY,RX=5.6,4.2,3.6           # eave half-extents, ridge half-length
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
STONE_TOP=4.0                  # ashlar ground storey; timber/plaster band above it
GX,GW,GH,GE=-1.6,2.2,2.6,7.4   # front gable: centre, wall half, roof half, eave
KG=(RIDGE-GE)/GH
GF=-D2-.5;GY=GF-.25            # jettied gable wall face, gable roof front
JZ=4.97                        # jetty floor top = gable wall base
BX0,BX1,BY,CZ=-W2+.3,.9,-.8,4.8  # open forge bay: inner left, right end, back wall, ceiling
XD,DH,DW,DE,DR=2.9,.85,.68,8.0,9.0   # dormer
YD=-(RIDGE-6.6)/KF
KD=(DR-DE)/DH
XC,YC=3.1,.3                   # ridge stack
XF,YF=-4.0,-1.0                # forge flue stack
TILE=dict(size=(.42,.58,.028),exposure=.27)
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return RIDGE-abs(x-GX)*KG
def dormer_z(x):return DR-abs(x-XD)*KD
in_gable=lambda p:abs(p.x-GX)<GW+.04 and p.y<0 and p.z<gable_z(p.x)-.005
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.52 and abs(p.y-YC)<.47
in_flue=lambda p:abs(p.x-XF)<.41 and abs(p.y-YF)<.41
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/3) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered,deck_cover=None):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8),**TILE)
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],deck_cover if deck_cover is not None else covered)
def frustum(geo,c0,c1,z0,z1,s0,s1,mat):
    """Closed tapered block (forge hood): rect s0 centred c0 at z0 to rect s1 centred c1 at z1."""
    def ring(c,s,z):return [Vector((c[0]+a*s[0]/2,c[1]+b*s[1]/2,z)) for a,b in ((-1,-1),(1,-1),(1,1),(-1,1))]
    b,t=ring(c0,s0,z0),ring(c1,s1,z1);geo.poly(b[::-1],mat);geo.poly(t,mat)
    for i in range(4):geo.poly([b[i],b[(i+1)%4],t[(i+1)%4],t[i]],mat)
# ---- main hip roof --------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,in_dormer,in_stack,in_flue])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_stack,in_flue])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_flue])
def cap(a,b,w=.32):G['roof'].beam(a,b,w,.06,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
capped((-RX,0,RIDGE+.06),(RX,0,RIDGE+.06),20)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.06),(sx*RX,0,RIDGE+.06),16)
for a,b in ((-EX,GX-GW),(GX+GW,EX)):G['timber'].beam((a,-EY-.02,EAVE-.07),(b,-EY-.02,EAVE-.07),.05,.24,M['timber'],band=band())
G['timber'].beam((-EX,EY+.02,EAVE-.07),(EX,EY+.02,EAVE-.07),.05,.24,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.07),(sx*(EX+.02),EY,EAVE-.07),.05,.24,M['timber'],band=band())
# ---- jettied front gable: roof, bargeboards, spire finial --------------------------
LG=math.hypot(GH,RIDGE-GE)
plane((GX-GH,0,GE),(0,-1,0),(GH,0,RIDGE-GE),LG,lambda t:0,lambda t:-GY,[under_main])
plane((GX+GH,0,GE),(0,1,0),(-GH,0,RIDGE-GE),LG,lambda t:GY,lambda t:0,[under_main])
capped((GX,GY,RIDGE+.06),(GX,-.5,RIDGE+.06),14)
for s in (-1,1):G['timber'].beam((GX+s*(GH+.06),GY-.05,GE-.1),(GX-s*.2,GY-.05,RIDGE+.2*KG+.04),.1,.3,M['timber'],band=band())
G['timber'].box((GX,GY-.07,RIDGE+.4),(.22,.22,1.0),M['timber'],.02,grain=True,band=band())
G['timber'].box((GX,GY-.07,RIDGE+.93),(.3,.3,.06),M['timber'],.02,grain=True,band=band())
G['detail'].vessel((GX,GY-.07,RIDGE+.96),[(.1,0),(.1,.12),(.07,.18),(.015,.62),(0,.62)],M['iron'],12)
# gable wall: storey with the shuttered window, then the triangle up to the ridge
GC=gable_z(GX+GW)-.05;UWZ,UWW,UWH=5.55,1.0,1.1
wall(G['plaster'],M['plaster'],(GX,GF,JZ),(0,-1,0),GW,GC-JZ,[(0,UWW+.18,UWZ-JZ-.09,UWZ-JZ+UWH+.09)],th=.25)
G['plaster'].prism([(GX-GW,GF+.125,GC),(GX+GW,GF+.125,GC),(GX,GF+.125,RIDGE-.05)],(0,1,0),.25,M['plaster'])
# cheek walls close the jettied gable back to the main slope
yq=-(RIDGE-GC)/KF
for s in (-1,1):
    G['plaster'].prism([(GX+s*GW,y,z) for y,z in ((GF+.02,JZ),(GF+.02,GC),(-D2,GC),(-D2,JZ))],(1,0,0),.1,M['plaster'])
    G['plaster'].prism([(GX+s*GW,y,z) for y,z in ((-D2,GC),(yq,GC),(-D2,main_z(GX+s*GW,-D2)-.07))],(1,0,0),.1,M['plaster'])
# jetty floor, bressumer and brackets
G['timber'].box((GX,(GF-.03+(-D2+.1))/2,JZ-.075),(2*GW+.1,abs(GF-.03+D2-.1),.15),M['planks'],.01)
G['timber'].box((GX,GF+.02,JZ-.12),(2*GW+.3,.24,.26),M['timber'],.015,grain=True,band=band())
for k in range(7):G['timber'].box((GX+(k-3)*.7,(GF+(-D2))/2,JZ-.27),(.14,abs(GF+D2)+.1,.12),M['timber'],.01,grain=True,band=band())
# ---- stacks -------------------------------------------------------------------------
G['stone'].box((XC,YC,11.1),(1.0,.86,3.6),M['stone'],.01)
G['stone'].box((XC,YC,12.2),(1.06,.92,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,12.86),(1.12,.98,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.5,0))
for dx in (-.24,.24):G['detail'].vessel((XC+dx,YC,12.92),[(.12,0),(.115,.34),(.125,.43),(.14,.47),(.11,.47),(.095,.41),(0,.41)],M['pot'])
G['stone'].box((XF,YF,10.1),(.74,.74,2.6),M['stone'],.01,w=(.35,0))
G['stone'].box((XF,YF,11.45),(.82,.82,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.6,0))
G['detail'].vessel((XF,YF,11.5),[(.16,0),(.16,.95),(0,.95)],M['iron'],16)
for k in range(4):
    a=math.tau*k/4+.4;G['detail'].beam((XF+.13*math.cos(a),YF+.13*math.sin(a),12.4),(XF+.13*math.cos(a),YF+.13*math.sin(a),12.57),.02,.02,M['iron'],grain=False)
G['detail'].vessel((XF,YF,12.55),[(0,0),(.36,0),(.36,.03),(0,.28)],M['iron'],16)
# ---- dormer ---------------------------------------------------------------------------
LD=math.hypot(DH,DR-DE)
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:1.5,lambda t:-YD+.18,[under_main])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.18,lambda t:-1.5,[under_main])
capped((XD,YD-.18,DR+.05),(XD,-1.6,DR+.05),6)
for s in (-1,1):G['timber'].beam((XD+s*(DH+.04),YD-.2,DE-.06),(XD-s*.12,YD-.2,DR+.12*KD+.03),.07,.22,M['timber'],band=band())
DB=main_z(XD,YD)-.1
wall(G['plaster'],M['plaster'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.73,.16,.99)],th=.14)
G['plaster'].prism([(XD-DW,YD+.07,DE+.02),(XD+DW,YD+.07,DE+.02),(XD+DW,YD+.07,dormer_z(XD+DW)-.05),(XD,YD+.07,DR-.05),(XD-DW,YD+.07,dormer_z(XD+DW)-.05)],(0,1,0),.14,M['plaster'])
ym=-(RIDGE-(dormer_z(XD+DW)-.05))/KF
for s in (-1,1):G['plaster'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,dormer_z(XD+DW)-.05),(XD+s*DW,ym,dormer_z(XD+DW)-.05)],(1,0,0),.09,M['plaster'])
for s in (-1,1):G['timber'].box((XD+s*(DW-.06),YD-.03,(DB+DE)/2),(.14,.14,DE-DB),M['timber'],.01,grain=True,band=band())
G['timber'].box((XD,YD-.04,DE-.02),(2*DW+.1,.16,.15),M['timber'],.01,grain=True,band=band())
window(G,M,(XD,YD,DB+.25),(0,-1,0),.55,.65,shutters=False,wall_th=.14,band=band())
# ---- walls: ashlar ground storey, plaster band to the wall plate ---------------------
PLATE=main_z(0,-D2)-.04
STEP,DS,DWID=.2,2.2,1.1        # door sill = step top; arch spring above it
WIN,WH,WZ=.8,1.0,1.3;WO=(WIN+.18,WZ-.09,WZ+WH+.09)
XDOOR,XWIN=2.0,4.15;FC=(BX1+W2)/2   # front stone wall spans BX1..W2
wall(G['stone'],M['stone'],(FC,-D2,0),(0,-1,0),(W2-BX1)/2,STONE_TOP,[(XDOOR-FC,DWID,STEP,STEP+DS+DWID/2),(XWIN-FC,*WO)],arch=(XDOOR-FC,DWID,STEP+DS))
wall(G['plaster'],M['plaster'],(FC,-D2,STONE_TOP),(0,-1,0),(W2-BX1)/2,PLATE-STONE_TOP)
wall(G['plaster'],M['plaster'],((BX0+BX1)/2,-D2,CZ),(0,-1,0),(BX1-BX0)/2,PLATE-CZ)
wall(G['stone'],M['stone'],(0,D2,0),(0,1,0),W2,STONE_TOP,[(-2.6,*WO),(2.6,*WO)])
wall(G['plaster'],M['plaster'],(0,D2,STONE_TOP),(0,1,0),W2,PLATE-STONE_TOP)
SIDE_WIN={-1:1.2,1:1.6}
for s in (-1,1):
    yc,half=((-.15,D2-.15) if s<0 else (0,D2-.3))   # left wall runs to the front corner (open bay)
    wall(G['stone'],M['stone'],(s*W2,yc,0),(s,0,0),half,STONE_TOP,[(s*(SIDE_WIN[s]-yc),*WO)])
    wall(G['plaster'],M['plaster'],(s*W2,yc,STONE_TOP),(s,0,0),half,PLATE-STONE_TOP)
    top=[(y/10,main_z(W2,y/10)-.05) for y in range(-round(D2*10),round(D2*10)+1) if main_z(W2,y/10)-.05>PLATE+.01]
    G['plaster'].prism([(s*(W2-.15),y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),.3,M['plaster'])
# ---- open forge bay -------------------------------------------------------------------
G['stone'].box(((BX1+.3/2),(-D2+.3+BY)/2,CZ/2),(.3,abs(BY+D2-.3),CZ),M['stone'],0)          # right return wall
G['stone'].box(((-W2+BX1)/2,(-D2-.1+BY)/2,.05),(BX1+W2,abs(BY+D2+.1),.1),M['stone'],.01)     # flagged floor
G['detail'].box(((BX0+BX1)/2,BY+.075,(CZ+.1)/2),(BX1-BX0,.15,CZ-.1),M['door'],0,swap=True)   # boarded back wall
G['timber'].box(((BX0+BX1)/2,(-D2+.3+BY)/2,CZ+.07),(BX1-BX0,abs(BY+D2-.3),.14),M['planks'],0)  # ceiling boards
for x in (-4.0,-2.6,-1.2,.2):G['timber'].box((x,(-D2+BY)/2,CZ-.09),(.16,abs(BY+D2),.18),M['timber'],.01,grain=True,band=band())
G['timber'].box(((-W2+BX1)/2,-D2-.06,CZ-.16),(BX1+W2,.26,.3),M['timber'],.015,grain=True,band=band())  # bay bressumer
G['stone'].box((-W2+.27,-D2-.04,.55),(.54,.54,1.1),M['ashlar'],.012,rect=kit.rng.choice(STONES))       # corner pier
G['stone'].box((BX1-.1,-D2-.06,.45),(.5,.42,.9),M['ashlar'],.012,rect=kit.rng.choice(STONES))
G['timber'].box((-W2+.15,-D2-.06,(1.1+CZ-.31)/2),(.26,.26,CZ-.31-1.1),M['timber'],.015,grain=True,band=band())
G['timber'].box((BX1-.13,-D2-.06,(.9+CZ-.31)/2),(.26,.26,CZ-.31-.9),M['timber'],.015,grain=True,band=band())
G['timber'].beam((-W2+.2,-D2-.06,3.3),(-W2+1.1,-D2-.06,CZ-.32),.16,.16,M['timber'],band=band())
G['timber'].beam((BX1-.2,-D2-.06,3.3),(BX1-1.1,-D2-.06,CZ-.32),.16,.16,M['timber'],band=band())
# forge: brick hearth, cheeks, tapered hood and flue to the ceiling; fire bed glows
HX,HY=-2.3,BY-.55
G['stone'].box((HX,HY,.5),(1.9,1.1,.8),M['brick'],.01)
G['detail'].box((HX,HY-.05,.915),(1.1,.6,.03),M['fire'],0)
for s in (-1,1):G['stone'].box((HX+s*.78,HY+.05,1.4),(.34,1.0,1.0),M['brick'],.01)
G['stone'].box((HX,BY-.14,1.4),(1.22,.28,1.0),M['brick'],0)
frustum(G['stone'],(HX,HY),(HX,BY-.36),1.9,3.0,(1.94,1.14),(.8,.6),M['brick'])
G['stone'].box((HX,BY-.36,3.9),(.8,.6,1.8),M['brick'],.01)
G['timber'].box((HX,HY-.58,1.94),(2.0,.12,.1),M['timber'],.01,grain=True,band=band())
# anvil on an oak stump in front of the hearth
AX,AY=-2.3,-2.75
G['timber'].vessel((AX,AY,.1),[(.31,0),(.32,.55),(.3,.6),(0,.6)],M['timber'],18)
G['detail'].box((AX,AY,.75),(.38,.26,.1),M['iron'],.01)
G['detail'].box((AX,AY,.87),(.2,.15,.14),M['iron'],.01)
G['detail'].box((AX+.05,AY,.99),(.5,.17,.1),M['iron'],.01)
G['detail'].prism([(AX-.2,AY,.94),(AX-.2,AY,1.04),(AX-.46,AY,1.02)],(0,1,0),.12,M['iron'])
# ---- timber framing of the band and gable ----------------------------------------------
def post(x,y,z0,z1,along_x):G['timber'].box((x,y,(z0+z1)/2),(.2 if along_x else .13,.13 if along_x else .2,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.13,s,M['timber'],band=band())
def frame_band(a,b,o,z0,top,n):
    """Posts, sill rail and alternating braces on the band between a and b (outer face points)."""
    a,b,o=Vector((*a,0)),Vector((*b,0)),Vector((*o,0))*.03;d=(b-a).normalized();along_x=abs(d.x)>abs(d.y)
    pts=[a+d*.12+(b-a-d*.24)*(i/n) for i in range(n+1)]
    for p in pts:post(p.x+o.x,p.y+o.y,z0,top(p)-.04,along_x)
    rail((a.x+o.x,a.y+o.y,z0+.07),(b.x+o.x,b.y+o.y,z0+.07))
    for i in range(n):
        p,q=(pts[i],pts[i+1]) if i%2 else (pts[i+1],pts[i]);zt=min(top(p),top(q))-.2
        rail((p.x+o.x,p.y+o.y,z0+.16),(q.x+o.x,q.y+o.y,zt),.12)
flat=lambda p:PLATE-.13   # stay under the deck where the eave overhang starts
frame_band((BX1,-D2),(W2,-D2),(0,-1),STONE_TOP,flat,3)
frame_band((-W2,D2),(W2,D2),(0,1),STONE_TOP,flat,7)
for s in (-1,1):frame_band((s*W2,-D2),(s*W2,D2),(s,0),STONE_TOP,flat,5)
for a,b in ((-W2,GX-GW),(GX+GW,W2)):rail((a,-D2-.04,PLATE-.25),(b,-D2-.04,PLATE-.25),.16)
rail((-W2,D2+.04,PLATE-.25),(W2,D2+.04,PLATE-.25),.16)
post(-W2+.15,-D2-.06,CZ,PLATE-.17,True)
y=GF-.03
for x in (GX-GW+.05,GX+GW-.05):post(x,y,JZ,GC-.02,True)
rail((GX-GW,y,UWZ+UWH+.2),(GX+GW,y,UWZ+UWH+.2),.13);rail((GX-GW,y,JZ+.07),(GX+GW,y,JZ+.07),.13)
rail((GX-GW+.1,y,GC+.02),(GX+GW-.1,y,GC+.02),.16)
for s in (-1,1):
    rail((GX+s*(GW-.12),y,JZ+.2),(GX+s*1.35,y,UWZ+UWH+.12),.11)
    rail((GX+s*(GW-.25),y,GC+.12),(GX+s*.62,y,9.55),.11)
gx=lambda z:(RIDGE-.05-z)/KG
rail((GX-gx(10.1)+.06,y,10.1),(GX+gx(10.1)-.06,y,10.1),.14);post(GX,y,10.17,RIDGE-.12,True)
# ---- openings ---------------------------------------------------------------------------
window(G,M,(XWIN,-D2,WZ),(0,-1,0),WIN,WH,planter=True,band=band())
for x in (-2.6,2.6):window(G,M,(x,D2,WZ),(0,1,0),WIN,WH,band=band())
for s in (-1,1):window(G,M,(s*W2,SIDE_WIN[s],WZ),(s,0,0),WIN,WH,band=band())
window(G,M,(GX,GF,UWZ),(0,-1,0),UWW,UWH,wall_th=.25,band=band())
door(G,M,(XDOOR,-D2,STEP),(0,-1,0),DWID,DS,STONES,band=band())
G['stone'].box((XDOOR,-D2-.225,STEP/2),(1.7,.45,STEP),M['ashlar'],.015,rect=kit.rng.choice(STONES))
# ---- shop sign: navy banner with a gilt anvil on an iron rod, crossed iron straps ------
SY=GF-.09
G['detail'].beam((GX-.78,SY,9.15),(GX+.78,SY,9.15),.03,.03,M['iron'],grain=False)
for s in (-1,1):
    G['detail'].beam((GX+s*.7,GF+.02,9.15),(GX+s*.7,SY-.02,9.15),.025,.025,M['iron'],grain=False)
    G['detail'].vessel((GX+s*.8,SY,9.15),[(0,-.04),(.035,-.04),(.035,.0),(0,.05)],M['iron'],8)
    G['detail'].beam((GX-s*.6,GF-.03,9.28),(GX+s*.6,GF-.03,9.86),.04,.015,M['iron'],grain=False)
banner=[(GX-.6,9.15),(GX+.6,9.15),(GX+.6,7.55),(GX,7.12),(GX-.6,7.55)]
G['detail'].prism([(x,SY,z) for x,z in banner],(0,1,0),.02,M['banner'])
anvil=[(-.36,.12),(.26,.12),(.26,.02),(.1,-.04),(.07,-.2),(.2,-.28),(-.2,-.28),(-.07,-.2),(-.1,-.04),(-.22,-.01)]
G['detail'].prism([(GX+a*1.1,SY-.01,8.3+z*1.1) for a,z in anvil],(0,1,0),.016,M['gilt'])
# ---- right lean-to ----------------------------------------------------------------------
LX,LZ0,LZ1,LY0,LY1=6.45,3.2,4.0,-3.6,-.3
LL=math.hypot(LX-W2,LZ1-LZ0)
plane((LX,0,LZ0),(0,1,0),(W2-LX,0,LZ1-LZ0),LL,lambda t:LY0,lambda t:LY1,[])
for yy in (LY0+.15,LY1-.15):post(LX-.15,yy,0,LZ0,False)
rail((LX-.15,LY0,LZ0-.02),(LX-.15,LY1,LZ0-.02),.16);rail((W2+.06,LY0,LZ1-.1),(W2+.06,LY1,LZ1-.1),.14)
for yy in (LY0+.15,LY1-.15):rail((LX-.15,yy,2.4),(LX-.15,yy+(.5 if yy<-2 else -.5),LZ0-.15),.1)
# ---- lanterns ------------------------------------------------------------------------------
for x in (-4.0,-.4):lantern(G,M,(x,-2.9,CZ-.02))
lantern(G,M,(3.07,-D2-.42,2.95),arm=((3.07,-D2-.09,2.95),(0,-1,0)))
lantern(G,M,(-W2-.6,-D2-.06,4.3),arm=((-W2+.02,-D2-.06,4.3),(-1,0,0)))
lantern(G,M,(LX-.5,-1.95,LZ0+.25))
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
from mathutils import Matrix
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
assert abs(lo[2])<1e-3,f'model does not stand on the ground: min z {lo[2]}'
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookBlacksmith'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
# Textures stay in ../Textures (FBX references them relatively); embedding 2k scans makes a huge GLB.
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
cam('Beauty',(-13,-17.5,6.2),(84,0,-36),lens=34);cam('Front',(0,-40,7.0),(90,0,0),14.6);cam('Side',(-40,0,7.0),(90,0,-90),14.6);cam('Rear',(0,40,7.0),(90,0,180),14.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Blacksmith-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'{tris} triangles exceeds the 120k house budget'
manifest=dict(asset='SilverbrookBlacksmith',reference=dict(file='Blacksmith-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_blacksmith.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookBlacksmith',slot_m=list(SLOT),outputs=['SilverbrookBlacksmith.blend','SilverbrookBlacksmith.glb','SilverbrookBlacksmith.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('BLACKSMITH_BUILT',manifest['bounds_m'],tris)
