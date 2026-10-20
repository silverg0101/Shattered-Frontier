"""Silverbrook Fisherman's Wharf from docs/Game-Design/concept-art/Silverbrook/Fishermans Wharf.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_fishermanswharf.py
Original procedural geometry interpreting the elevation (20 m wide, 8 m from deck to chimney top):
a plank deck on timber piles with mooring posts and ladders, a stone-and-board gabled fish shop with
an open shop front, a dormered side wing over an open porch, an open slate-roofed work shed, a
lean-to drying pavilion, a jib crane with a hanging basket and a canvas awning over a drying rack.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,window,door,lantern,frame,UP
from mathutils import Matrix
import bpy
scene=kit.reset(4120)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Fishermans Wharf.png'
asset=bpy.data.collections.new('SilverbrookFishermansWharf');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Ashlar','stone_wall_04'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    boards=kit.scan('SB_FishermansWharf_Boards','weathered_brown_planks',dark=(.30,.29,.28),aged=.4),
    decking=kit.scan('SB_FishermansWharf_Deck','wood_planks_grey',dark=(.30,.29,.27),aged=.25),
    canvas=kit.scan('SB_FishermansWharf_Canvas','cotton_jersey',dark=(.85,.82,.62),normal=.6,damp=False,aged=.5),
    banner=kit.scan('SB_FishermansWharf_Banner','denim_fabric_03',dark=(.10,.12,.28),damp=False,aged=.8),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_FishermansWharf_Gilt',(.80,.55,.18),.35,1.0))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('stone_wall_04',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)
N='SM_SBFishermansWharf_'
G=dict(stone=Geo(N+'Masonry',asset),boards=Geo(N+'Boards',asset),timber=Geo(N+'Timber',asset),deck=Geo(N+'Deck',asset),roof=Geo(N+'Roof',asset),detail=Geo(N+'Details',asset))
T,D=G['timber'],G['detail']
# ---- levels ------------------------------------------------------------------------
# z=0 is the riverbank where the wharf meets the land (+Y). heightfield.py puts the bank 1.5 m above the
# water (WATER_BANK) and the channel bed 2 m below the water (CHANNEL_DEPTH), as for StoneBridge.
# The deck top is flush with the bank (z=0), so it is 1.5 m above the water and you walk on with no step.
SLOT=(20,8,11.5)               # layout gives 20 x 8 x 1 (deck only); h = 3.5 m of piles below the bank + 8 m above the deck (concept)
WATER,BED=-1.5,-3.5
FY=-3.8                        # deck edge on the water (front face of the fascia)
DY0,DY1=-3.7,3.95              # deck boards (land edge at +Y)
# ---- shared helpers -------------------------------------------------------------------
def post(x,y,z0,z1,s=.22):T.box((x,y,(z0+z1)/2),(s,s,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,w=.12,d=.16):T.beam(a,b,w,d,M['timber'],band=band())
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-3.5)/2) else 0
def plane(O,e,toward,L,lo,hi,covered,deck_cover=None):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],deck_cover if deck_cover is not None else covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
def fill(geo,pts,axis,th,mat):
    """Flat slab around a planar outline, caps fanned from pts[0] (Townhouse1)."""
    pts=[Vector(p) for p in pts];axis=Vector(axis).normalized()
    if sum((a.cross(pts[(i+1)%len(pts)]) for i,a in enumerate(pts)),Vector()).dot(axis)<0:pts.reverse()
    o=axis*th/2;n=len(pts)
    for i in range(1,n-1):geo.poly([pts[0]+o,pts[i]+o,pts[i+1]+o],mat);geo.poly([pts[0]-o,pts[i+1]-o,pts[i]-o],mat)
    for i,a in enumerate(pts):b=pts[(i+1)%n];geo.poly([a-o,b-o,b+o,a+o],mat)
def stag(geo,origin,right,up,normal,s):
    """Gilt stag head with antlers (Townhouse1)."""
    o=Vector(origin)+Vector(normal)*.008;Rv,U,Nn=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+Rv*(a*s)+U*(z*s)
    fill(geo,[p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],Nn,.02,M['gilt'])
    for sx in (-1,1):
        fill(geo,[p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],Nn,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
def banner(c,o,ztop,zbot,w,d=.06):
    """Navy banner with a pointed foot, gilt edging, rod on iron brackets and a gilt stag (StoneBridge)."""
    P,R=frame(c,o);pts=[(-w/2,ztop),(w/2,ztop),(w/2,zbot+.35),(0,zbot),(-w/2,zbot+.35)]
    D.prism([P(x,d,z) for x,z in pts],o,.02,M['banner'])
    for (x0,z0),(x1,z1) in zip(pts,pts[1:]+pts[:1]):D.beam(P(x0,d+.012,z0),P(x1,d+.012,z1),.03,.012,M['gilt'],grain=False)
    D.beam(P(-w/2-.08,d,ztop+.05),P(w/2+.08,d,ztop+.05),.035,.035,M['gilt'],grain=False)
    for s in (-1,1):D.box(P(s*(w/2+.04),d/2,ztop+.05),(.04,d+.03,.04),M['iron'],.004,R)
    stag(D,P(0,d+.011,(ztop+zbot)/2+.15),R@Vector((1,0,0)),UP,Vector(o).normalized(),w*1.05)
def wall_lantern(x,y,z):lantern(G,M,(x,y-.33,z),arm=((x,y,z),(0,-1,0)))
# ---- deck, piles, mooring posts, ladders ----------------------------------------------
NB=80;BW=19.9/NB
for i in range(NB):G['deck'].box((-9.95+(i+.5)*BW,(DY0+DY1)/2,-.03),(BW,DY1-DY0,.06),M['decking'],.006,grain=True,w=(kit.rng.uniform(0,.3),0))
T.box((0,FY+.05,-.15),(19.9,.1,.3),M['timber'],.01,grain=True,band=band())          # fascia; its top is part of the deck
ROWS=(-3.58,-1.25,1.25,3.8)
for y in ROWS:T.box((0,y,-.17),(19.9,.22,.22),M['timber'],.012,grain=True,band=band())  # bearers under the boards
XS=[-9.85+i*19.7/8 for i in range(9)]
def pile(x,y,top,s=.3):T.box((x,y,(BED+top)/2),(s,s,top-BED),M['timber'],.06,grain=True,band=band())
for y in ROWS[1:]:
    for x in XS:pile(x,y,-.28)
MOOR=[-9.85,-8.2,-5.5,-3.8,-1.0,2.9,5.0,7.4,9.85]          # front piles rise through the deck edge as mooring posts
for x in MOOR:pile(x,-3.85,kit.rng.uniform(.85,1.1))
for a,b in zip(MOOR,MOOR[1:]):                            # cross bracing behind the front piles
    k=MOOR.index(a)%2;rail((a,-3.64,-1.6 if k else -.45),(b,-3.64,-.45 if k else -1.6),.12,.08)
for x in (-9.65,9.65):
    for ya,yb in zip(ROWS[1:],ROWS[2:]):rail((x,ya,-1.6),(x,yb,-.45),.08,.12)
for xl in (1.6,8.45):                                     # ladders down to the water, rails as grab handles
    for s in (-1,1):T.box((xl+s*.28,FY-.04,(-2.1+.9)/2),(.07,.08,3.0),M['timber'],.008,grain=True,band=band())
    z=-1.95
    while z<.75:T.beam((xl-.28,FY-.04,z),(xl+.28,FY-.04,z),.045,.045,M['timber'],band=band());z+=.3
# ---- main shop: stone ground storey, boarded gable, slate roof ------------------------
XM,HW,YF,YR=-3.3,3.0,-2.4,2.6
EAVE,RIDGE,EH=3.7,7.6,3.7;KM=(RIDGE-EAVE)/EH;MY0,MY1=YF-.35,YR+.35
def main_z(x):return RIDGE-abs(x-XM)*KM
WALLTOP=main_z(XM+HW)-.06;PLATE=3.2
WO=(.88,.96,1.99)
wall(G['stone'],M['stone'],(XM,YF,0),(0,-1,0),HW,PLATE,[(0,3.8,0,PLATE)])
wall(G['stone'],M['stone'],(XM,YR,0),(0,1,0),HW,PLATE,[(0,1.0,0,2.2)],arch=(0,1.0,1.7))
wall(G['stone'],M['stone'],(XM-HW,.1,0),(-1,0,0),2.2,WALLTOP,[(-.9,*WO)])
wall(G['stone'],M['stone'],(XM+HW,.1,0),(1,0,0),2.2,WALLTOP)
for y in (YF+.1,YR-.1):
    G['boards'].prism([(XM+x,y,z) for x,z in [(-HW,PLATE),(HW,PLATE),(HW,WALLTOP),(0,RIDGE-.05),(-HW,WALLTOP)]],(0,1,0),.3,M['boards'],swap=True)
T.box((XM,YF-.1,3.15),(2*HW+.3,.22,.3),M['timber'],.015,grain=True,band=band())   # bressumer / shop lintel
T.box((XM,YR+.1,3.15),(2*HW+.3,.22,.3),M['timber'],.015,grain=True,band=band())
for s in (-1,1):
    for y in (YF-.06,YR+.06):post(XM+s*(HW-.1),y,PLATE+.1,WALLTOP-.05,.16)
rail((XM-2.9,YF-.07,4.0),(XM+2.9,YF-.07,4.0),.12,.14)
# shop front: a 1.1 m deep open recess with a fish counter (no interior)
G['boards'].box((XM,YF+1.05,1.5),(3.8,.1,3.0),M['boards'],0,swap=True)
for s in (-1,1):G['stone'].box((XM+s*2.05,YF+.7,1.5),(.3,.8,3.0),M['stone'],0)
G['boards'].box((XM,YF+.55,3.1),(3.8,1.1,.2),M['boards'],0)
T.box((XM,YF+.75,.45),(3.0,.5,.9),M['timber'],.01,grain=True,band=band())
T.box((XM,YF+.72,.92),(3.1,.62,.05),M['timber'],.01,grain=True,band=band())
for x in (-1.2,0,1.2):rail((XM+x,YF+.3,2.75),(XM+x,YF+1.0,2.75),.1,.1)    # ceiling joists for the hanging hooks
door(G,M,(XM,YR,0),(0,1,0),1.0,1.7,STONES,band=band())
window(G,M,(XM-HW,1.0,1.05),(-1,0,0),.7,.85,shutters=False,band=band())
banner((XM,YF-.05,0),(0,-1,0),6.0,4.15,.95)
wall_lantern(XM-2.55,YF-.04,2.6);wall_lantern(XM+2.55,YF-.04,2.6)
# chimney stack on the left slope
XC,YC=-5.1,.8
G['stone'].box((XC,YC,5.175),(.9,.85,4.35),M['stone'],.01)
G['stone'].box((XC,YC,6.7),(.96,.91,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,7.38),(.98,.93,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.2,.2):D.vessel((XC+dx,YC,7.44),[(.11,0),(.105,.3),(.115,.38),(.13,.42),(.10,.42),(.085,.36),(0,.36)],M['pot'])
in_stack=lambda p:abs(p.x-XC)<.5 and abs(p.y-YC)<.47
# ---- side wing over an open porch, with a dormer ------------------------------------------
WR,WE,WY=6.0,3.8,2.95;KW=(WR-WE)/WY;LW=math.hypot(WY,WR-WE)
def wing_z(y):return WR-abs(y)*KW
under_main=lambda p:MY0<p.y<MY1 and p.x<XM+EH and p.z<main_z(p.x)-.01
under_wing=lambda p:p.x>-1.9 and abs(p.y)<WY and p.z<wing_z(p.y)-.01
def valley(t):return XM+(RIDGE-(WE+t*(WR-WE)/LW))/KM-.12     # wing slope meets the main slope here
XD,DH,DW,DE,DR=.85,.62,.45,5.45,5.85
YD=-(WR-4.6)/KW;KD=(DR-DE)/DH;LD=math.hypot(DH,DR-DE)
def dormer_z(x):return DR-abs(x-XD)*KD
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
plane((0,-WY,WE),(1,0,0),(0,WY,WR-WE),LW,valley,lambda t:2.75,[under_main,in_dormer],[])
plane((0,WY,WE),(-1,0,0),(0,-WY,WR-WE),LW,lambda t:-2.75,lambda t:-valley(t),[under_main],[])
capped((valley(LW)+.1,0,WR+.05),(2.75,0,WR+.05),9)
for s in (-1,1):T.beam((2.79,s*(WY+.05),WE-.08),(2.79,-s*.16,WR+.16*KW+.03),.08,.24,M['timber'],band=band())
# enclosed body x -0.3..2.6, y -0.2..2.6; open porch in front of it
wall(G['boards'],M['boards'],(1.15,-.2,0),(0,-1,0),1.45,wing_z(-.2)-.06,[(-.75,.78,.91,1.89),(.6,1.0,0,2.1)],th=.15,swap=True)
wall(G['boards'],M['boards'],(1.15,2.6,0),(0,1,0),1.45,wing_z(2.6)-.06,[(0,.78,.91,1.89)],th=.15,swap=True)
G['boards'].prism([(2.525,y,z) for y,z in [(-.2,0),(2.6,0),(2.6,wing_z(2.6)-.06),(0,WR-.06),(-.2,wing_z(-.2)-.06)]],(1,0,0),.15,M['boards'],swap=True)
G['boards'].prism([(2.525,y,z) for y,z in [(-2.75,3.6),(-.2,3.6),(-.2,wing_z(-.2)-.06),(-2.75,wing_z(-2.75)-.06)]],(1,0,0),.15,M['boards'],swap=True)
G['boards'].prism([(-.3,-.125,main_z(-.3)-.1),(-.3,-.125,wing_z(-.2)-.06),(XM+(RIDGE-(wing_z(-.2)-.06))/KM,-.125,wing_z(-.2)-.06)],(0,1,0),.15,M['boards'],swap=True)  # closes the gap above the main slope
window(G,M,(.4,-.2,1.0),(0,-1,0),.6,.8,shutters=False,wall_th=.15,band=band())
window(G,M,(1.15,2.6,1.0),(0,1,0),.6,.8,shutters=False,wall_th=.15,band=band())
D.box((1.75,-.12,1.05),(1.0,.06,2.1),M['door'],.004,swap=True)
for z in (.4,1.7):D.box((1.75,-.16,z),(.8,.012,.05),M['iron'],.004)
T.box((1.75,-.24,2.17),(1.2,.1,.14),M['timber'],.01,grain=True,band=band())
for x in (.25,2.45):
    post(x,-2.7,0,3.7);rail((x,-2.7,3.6),(x,-.2,3.6),.12,.16)
    for s in (-1,1):rail((x+s*.45,-2.7,3.7),(x,-2.7,3.1),.1,.1)
T.box((1.15,-2.7,3.8),(2.9,.18,.2),M['timber'],.012,grain=True,band=band())
wall_lantern(2.45,-2.81,2.7)
# dormer (Cottage1's construction, on the wing's front slope)
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:.1,lambda t:-YD+.16,[under_wing])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:-.1,[under_wing])
capped((XD,YD-.16,DR+.04),(XD,-.3,DR+.04),4)
for s in (-1,1):T.beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
DB=wing_z(YD)-.1
wall(G['boards'],M['boards'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.58,.13,.71)],th=.12,swap=True)
G['boards'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,dormer_z(XD+DW)-.04),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,dormer_z(XD+DW)-.04)],(0,1,0),.12,M['boards'],swap=True)
ym=-(WR-(dormer_z(XD+DW)-.04))/KW
for s in (-1,1):G['boards'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,dormer_z(XD+DW)-.04),(XD+s*DW,ym,dormer_z(XD+DW)-.04)],(1,0,0),.08,M['boards'],swap=True)
for s in (-1,1):T.box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
T.box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
window(G,M,(XD,YD,DB+.22),(0,-1,0),.4,.4,shutters=False,wall_th=.12,band=band())
# main roof (after the wing so its slates skip the wing's interior)
LM=math.hypot(EH,RIDGE-EAVE)
plane((XM-EH,0,EAVE),(0,-1,0),(EH,0,RIDGE-EAVE),LM,lambda t:-MY1,lambda t:-MY0,[in_stack])
plane((XM+EH,0,EAVE),(0,1,0),(-EH,0,RIDGE-EAVE),LM,lambda t:MY0,lambda t:MY1,[under_wing],[])
capped((XM,MY0,RIDGE+.05),(XM,MY1,RIDGE+.05),22)
for y in (MY0-.04,MY1+.04):
    for s in (-1,1):T.beam((XM+s*(EH+.05),y,EAVE-.08),(XM-s*.16,y,RIDGE+.16*KM+.03),.08,.24,M['timber'],band=band())
T.box((XM,MY0-.06,RIDGE+.1),(.14,.14,.6),M['timber'],.02,grain=True,band=band())
# ---- open work shed between the wing and the pavilion -----------------------------------
SR,SE,SY=4.3,3.25,2.95;KS=(SR-SE)/SY;LS=math.hypot(SY,SR-SE)
plane((0,-SY,SE),(1,0,0),(0,SY,SR-SE),LS,lambda t:2.6,lambda t:6.05,[])
plane((0,SY,SE),(-1,0,0),(0,-SY,SR-SE),LS,lambda t:-6.05,lambda t:-2.6,[])
capped((2.6,0,SR+.05),(6.05,0,SR+.05),6)
for y in (-2.7,2.7):
    T.box((4.325,y,3.19),(3.45,.18,.18),M['timber'],.012,grain=True,band=band())
    for x in (4.3,5.85):
        post(x,y,0,3.1)
        for s in (-1,1):rail((x+s*.45,y,3.1),(x,y,2.55),.1,.1)
for x in (4.3,5.85):rail((x,-2.7,3.2),(x,2.7,3.2),.12,.16);post(x,0,3.28,SR-.1,.16)
T.box((4.325,0,SR-.17),(3.45,.16,.14),M['timber'],.012,grain=True,band=band())
G['boards'].prism([(6.02,y,z) for y,z in [(-3.1,4.82),(3.1,4.82),(3.1,3.2),(0,SR-.04),(-3.1,3.2)]],(1,0,0),.1,M['boards'],swap=True)
wall_lantern(5.85,-2.81,2.7)
# ---- lean-to drying pavilion -------------------------------------------------------------
PX0,PX1,PH,PE,PY=6.07,9.95,4.9,3.1,3.1;KP=(PH-PE)/(PX1-PX0);LP=math.hypot(PX1-PX0,PH-PE)
def pav_z(x):return PH-(x-PX0)*KP
plane((PX1,0,PE),(0,1,0),(PX0-PX1,0,PH-PE),LP,lambda t:-PY,lambda t:PY,[])
capped((PX0,-PY,PH+.04),(PX0,PY,PH+.04),12)
for x in (6.25,9.6):
    top=pav_z(x)-.24;T.box((x,0,top+.1),(.16,2*PY-.1,.2),M['timber'],.012,grain=True,band=band())
    for y in (-2.8,0,2.8):
        post(x,y,0,top)
        for s in (-1,1):
            if abs(y+s*.45)<3:rail((x,y+s*.45,top),(x,y,top-.5),.1,.1)
for y in (-2.8,0,2.8):rail((PX0,y,pav_z(PX0)-.135),(PX1,y,pav_z(PX1)-.135),.1,.16)
for y in (-2.8,2.8):rail((6.25,y,2.4),(9.6,y,2.4),.08,.08)                   # drying rails
wall_lantern(6.25,-2.91,2.6);wall_lantern(9.6,-2.91,2.6)
# ---- jib crane with hanging basket, awning over a drying rack --------------------------------
CX,CY=-7.0,-3.25
post(CX,CY,0,6.9,.28)
T.beam((-8.6,CY,6.75),(-5.9,CY,6.75),.2,.22,band=band(),mat=M['timber'])
rail((CX,CY,5.6),(CX-1.1,CY,6.66),.12,.12);rail((CX,CY,5.9),(CX+.8,CY,6.66),.12,.12)
for s in (-1,1):rail((CX,CY,1.1),(CX+s*.75,CY,0),.12,.12);rail((CX,CY,1.1),(CX,CY+s*.6,0),.12,.12)
D.box((-8.4,CY,6.55),(.14,.12,.2),M['iron'],.01)
D.beam((-8.4,CY,6.45),(-8.4,CY,5.0),.03,.03,M['iron'],grain=False)
BZ=3.95
for dx,dy in ((-1,-1),(1,-1),(1,1),(-1,1)):D.beam((-8.4,CY,5.01),(-8.4+dx*.31,CY+dy*.31,BZ+.5),.02,.02,M['iron'],grain=False)
T.box((-8.4,CY,BZ+.03),(.7,.7,.06),M['timber'],.008,grain=True,band=band())
for s in (-1,1):
    T.box((-8.4+s*.33,CY,BZ+.28),(.04,.7,.5),M['timber'],.006,grain=True,band=band())
    T.box((-8.4,CY+s*.33,BZ+.28),(.62,.04,.5),M['timber'],.006,grain=True,band=band())
for y in (-3.25,-.25):post(-9.6,y,0,2.7,.16)
rail((-9.6,-3.25,2.75),(-9.6,-.25,2.75),.1,.1)
rail((-6.36,-3.05,3.33),(-6.36,-.15,3.33),.12,.12)
T.beam((-9.6,-3.25,2.2),(CX,CY,2.2),.08,.08,mat=M['timber'],band=band())     # fish rail
D.beam((-6.42,-1.6,3.38),(-9.6,-1.6,2.81),2.9,.02,M['canvas'],grain=False)
# ---- export, checks, preview scene, manifest ---------------------------------------------
DAMP=set(kit.DAMP);kit.DAMP.clear()   # kit damps from z=0 (here the deck); damp from the water instead
objs=[g.object() for g in G.values()]
def rewet(o):
    """Wet band 0.65 m above the water (and everything below it) on the damp materials."""
    me=o.data;col=me.color_attributes['Weathering']
    for poly in me.polygons:
        if me.materials[poly.material_index].name not in DAMP:continue
        for li in poly.loop_indices:
            p=me.vertices[me.loops[li].vertex_index].co;damp=max(0,1-(p.z-WATER)/.65)*(.30+.15*math.sin(p.x*7.3+p.y*5.1))
            c=col.data[li].color;col.data[li].color=(min(.8,max(c[0],damp)),c[1],0,1)
for o in objs:rewet(o)
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
assert abs(lo[2]-BED)<1e-6,f'piles must stand on the bed: {lo[2]}'
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120_000,f'triangle budget: {tris}'
# Walkable deck: cast down from 0.5 m on a 0.2 m grid over the whole deck (inside buildings too);
# every hit must be the deck top or the foot of something standing on it, at z=0 (no gaps or steps).
bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get();sx,sy=-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2;bad=[];n=0
for i in range(99):
    for j in range(38):
        p=Vector((-9.8+i*.2+sx,DY0+.05+j*.2+sy,.5));n+=1
        while True:   # pass through anything standing on the deck (counter, braces) down to the walking surface
            hit,loc,*_=scene.ray_cast(dg,p,Vector((0,0,-1)))
            if not hit or loc.z<=.012:break
            p=loc-Vector((0,0,1e-4))
        if not hit or abs(loc.z)>.012:bad.append((round(p.x,2),round(p.y,2),round(loc.z,3) if hit else None))
assert not bad,f'deck not walkable at {len(bad)} of {n} points, e.g. {bad[:6]}'
OUT=HERE/'SilverbrookFishermansWharf'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
# Textures stay in ../Textures (FBX references them relatively); embedding 2k scans makes a huge GLB.
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
def preview(name,verts,faces,mat):
    me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);ob=bpy.data.objects.new(name,me);pre.objects.link(ob);me.materials.append(mat)
# Bank as heightfield.py carves it: land at z=0 behind the wharf (+Y), sloping to the water, bed 2 m down.
prof=[(60,0),(4.2,0),(1.5,WATER+.3),(-6,BED),(-60,BED)]
preview('Ground',[(x,y,z) for y,z in prof for x in (-40,40)],[(2*i,2*i+1,2*i+3,2*i+2) for i in range(len(prof)-1)],kit.solid('Preview_Ground',(.11,.12,.07),.95))
preview('Water',[(-40,-60,WATER),(40,-60,WATER),(40,2,WATER),(-40,2,WATER)],[(0,1,2,3)],kit.solid('Preview_Water',(.03,.05,.06),.05))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-15,-24,7.5),(78,0,-32),lens=30);cam('Front',(0,-60,2.4),(90,0,0),21.5);cam('Side',(-60,0,2.4),(90,0,-90),21.5);cam('Rear',(0,60,2.4),(90,0,180),21.5)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1600;scene.render.resolution_y=900;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'FishermansWharf-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
manifest=dict(asset='SilverbrookFishermansWharf',reference=dict(file='FishermansWharf-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_fishermanswharf.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y (water side), origin at ground centre = deck centre at riverbank level',bounds_m=[round(v,3) for v in maxs-mins],
    z_range_m=[round(mins.z,3),round(maxs.z,3)],
    deck=dict(top_z_m=0.0,bank_z_m=0.0,water_z_m=WATER,bed_z_m=BED,height_above_water_m=-WATER,land_edge='+Y (meets the bank flush, no step)',water_edge='-Y (fascia, mooring posts, two ladders to the water)',
              walkable_m=[19.9,round(DY1-DY0+.1,2)],ray_checks=n,note='continuous plank deck; buildings stand on it; checked by downward ray casts on a 0.2 m grid'),
    slot_height_note='layout wharf slot is 20 x 8 x 1 (deck only); the concept has an 8 m shop, crane and chimney above the deck, and the piles reach the bed 3.5 m below the bank, so the slot height is 11.5 m',
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookFishermansWharf',slot_m=list(SLOT),
    asset_id='BLD-GreenlandsFishermansWharf-01',outputs=['SilverbrookFishermansWharf.blend','SilverbrookFishermansWharf.glb','SilverbrookFishermansWharf.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('FISHERMANSWHARF_BUILT',manifest['bounds_m'],manifest['z_range_m'],tris)
