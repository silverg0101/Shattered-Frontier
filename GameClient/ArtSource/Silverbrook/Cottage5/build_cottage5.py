"""Silverbrook Cottage 5 from docs/Game-Design/concept-art/Silverbrook/Cottage-5.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_cottage5.py
Original procedural geometry interpreting the elevation (8 m wide, 6 m to the chimney
pots, heights scaled ~0.9 so the pots fit the slot): pale fieldstone walls on a grey
plinth with oak corner posts and braces, steep-hipped slate roof patched with boards
and rust-red tiles, an off-centre half-timbered front gable, gabled porch hood over a
round-headed door, right dormer, right-end stack and a lean-to woodshed.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,Matrix,tile_plane,deck,wall,window,door,lantern
import bpy,numpy as np
scene=kit.reset(4105)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Cottage-5.png'
asset=bpy.data.collections.new('SilverbrookCottage5');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Cottage5_Fieldstone','stone_wall'),
    plinth=kit.scan('SB_Cottage5_Plinth','rock_wall_08'),
    ashlar=kit.scan('SB_Ashlar','stone_wall_04'),
    rust=kit.scan('SB_Cottage5_RustTile','roof_tiles_14',dark=(.20,.22,.28),damp=False),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    shutter=kit.scan('SB_PaintedShutter','blue_painted_planks',dark=(.20,.22,.28),damp=False,aged=.85),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75))
M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
def reddest(asset,size,keep=20):
    """Element interiors of a scan ranked by warmth (R-G): the rust-red tiles, not the mossy ones."""
    img=kit._image(asset,'Diffuse');W,H=img.size;px=np.empty(len(img.pixels),np.float32);img.pixels.foreach_get(px);px=px.reshape(H,W,4)
    warm=lambda r:(lambda b:(b[...,0]-b[...,1]).mean())(px[int(r[1]*H):int((r[1]+r[3])*H),int(r[0]*W):int((r[0]+r[2])*W)])
    return sorted(kit.patches(asset,size,count=400),key=warm)[-keep:]
RUST=reddest('roof_tiles_14',(.03,.04))
STONES=kit.patches('stone_wall_04',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBCottage5_Masonry',asset),timber=Geo('SM_SBCottage5_Timber',asset),roof=Geo('SM_SBCottage5_Roof',asset),detail=Geo('SM_SBCottage5_Details',asset))
# ---- measured layout (metres; recentred on the slot at export) ---------------------
SLOT=(8,6,6)                   # town-layout slot w x d x h (concept README 'Fits')
W2,D2=3.2,2.2                  # outer wall half-extents
EAVE,RIDGE=2.45,5.0            # main hip roof
EX,EY,RX=3.5,2.5,2.9           # eave half-extents, ridge half-length (steep hips)
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
XG,GH,GW,GE=-.45,1.35,1.1,3.5  # front gable: centre (left of the door, as drawn), roof half, wall half, eave
KG=(RIDGE-GE)/GH;GY=-D2-.3
XD,DH,DW,DE,DR=2.0,.6,.48,3.85,4.25   # dormer
YD=-1.9;KD=(DR-DE)/DH;YDB=-(RIDGE-DR)/KF+.18   # dormer roof runs back until it is under the main slope
XC,YC=2.68,.15                 # right-end stack
PE,PR,PH,PY=2.05,2.85,1.1,-D2-.8  # porch eave, ridge, roof half, front
SX0,SX1,SY0,SY1,SZ0,SZ1=W2,4.05,-1.6,.5,2.25,1.75   # lean-to woodshed (right wall)
PLINTH=.85
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return RIDGE-abs(x-XG)*KG
def dormer_z(x):return DR-abs(x-XD)*KD
in_gable=lambda p:abs(p.x-XG)<GW+.04 and p.y<0 and p.z<gable_z(p.x)-.005
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.5 and abs(p.y-YC)<.42
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/2) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
# Rust-red replacement tiles (concept: right of the gable near the ridge, right of the dormer at the eave).
in_rust=lambda p:p.y<0 and ((.95<p.x<1.6 and p.z>4.05) or (2.65<p.x<3.4 and p.z<2.95))
class Patched:
    """tile_plane target that swaps the slates inside in_rust for single rust tiles."""
    def __init__(s,geo):s.geo=geo
    def box(s,centre,size,mat,*a,**kw):
        if in_rust(centre):mat=M['rust'];kw['rect']=kit.rng.choice(RUST);kw['w']=(kw['w'][0]*.6,0)
        s.geo.box(centre,size,mat,*a,**kw)
def plane(O,e,toward,L,lo,hi,covered,deck_cover=None,geo=None):
    u=Vector(toward).normalized();tile_plane(geo or G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],deck_cover if deck_cover is not None else covered)
def boards(O,e,toward,x0,x1,t0,t1,along_u):
    """Patch of replacement boards nailed over the slates; returns the cut for the slates under it."""
    O,e,u=Vector(O),Vector(e).normalized(),Vector(toward).normalized();n=e.cross(u);R=Matrix((e,u,n)).transposed()
    if along_u:
        k=max(1,round((x1-x0)/.15))
        for i in range(k):x=x0+(i+.5)*(x1-x0)/k;G['roof'].box(O+e*x+u*(t0+t1)/2+n*.06,((x1-x0)/k-.012,t1-t0+kit.rng.uniform(-.06,.06),.032),M['timber'],.006,R,grain=True,band=band(),w=(.4,0))
    else:
        k=max(1,round((t1-t0)/.15))
        for i in range(k):t=t0+(i+.5)*(t1-t0)/k;G['roof'].box(O+e*((x0+x1)/2+kit.rng.uniform(-.04,.04))+u*t+n*.06,(x1-x0,(t1-t0)/k-.012,.032),M['timber'],.006,R,grain=True,band=band(),w=(.4,0))
    return lambda p:x0+.04<(p-O).dot(e)<x1-.04 and t0+.04<(p-O).dot(u)<t1-.04
# ---- main hip roof -------------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
FO,FU=(0,-EY,EAVE),(0,EY,RIDGE-EAVE)
patch=[boards(FO,(1,0,0),FU,-2.95,-2.45,1.75,2.75,True),boards(FO,(1,0,0),FU,-2.85,-2.05,.3,.75,False),boards(FO,(1,0,0),FU,1.0,1.3,.15,.95,True)]
plane(FO,(1,0,0),FU,LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,in_dormer,in_stack]+patch,[in_gable,in_dormer,in_stack],geo=Patched(G['roof']))
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_stack])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
capped((-RX,0,RIDGE+.05),(RX,0,RIDGE+.05),18)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.05),(sx*RX,0,RIDGE+.05),11)
for a,b in ((-EX,XG-GW),(XG+GW,EX)):G['timber'].beam((a,-EY-.02,EAVE-.06),(b,-EY-.02,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((-EX,EY+.02,EAVE-.06),(EX,EY+.02,EAVE-.06),.04,.2,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.06),(sx*(EX+.02),EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- front gable roof, bargeboards and finial -------------------------------------------
LG=math.hypot(GH,RIDGE-GE)
plane((XG-GH,0,GE),(0,-1,0),(GH,0,RIDGE-GE),LG,lambda t:0,lambda t:-GY,[under_main])
plane((XG+GH,0,GE),(0,1,0),(-GH,0,RIDGE-GE),LG,lambda t:GY,lambda t:0,[under_main])
capped((XG,GY,RIDGE+.05),(XG,-.4,RIDGE+.05),9)
for s in (-1,1):G['timber'].beam((XG+s*(GH+.05),GY-.04,GE-.08),(XG-s*.16,GY-.04,RIDGE+.16*KG+.03),.08,.24,M['timber'],band=band())
yq=-(RIDGE-(gable_z(XG+GW)-.05))/KF    # cheek walls close the gable down to the main slope
for s in (-1,1):G['stone'].prism([(XG+s*GW,-D2,main_z(0,-D2)-.04),(XG+s*GW,-D2,gable_z(XG+GW)-.05),(XG+s*GW,yq,gable_z(XG+GW)-.05)],(1,0,0),.1,M['stone'])
G['timber'].box((XG,GY-.06,5.32),(.15,.15,.8),M['timber'],.02,grain=True,band=band())
G['timber'].box((XG,GY-.06,5.74),(.2,.2,.05),M['timber'],.02,grain=True,band=band())
# ---- dormer -----------------------------------------------------------------------
LD=math.hypot(DH,DR-DE)
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:-YDB,lambda t:-YD+.16,[under_main])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:YDB,[under_main])
capped((XD,YD-.16,DR+.04),(XD,-.72,DR+.04),4)
for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
DB=main_z(XD,YD)-.1
wall(G['stone'],M['stone'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.58,.13,.71)],th=.12)
G['stone'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,dormer_z(XD+DW)-.04),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,dormer_z(XD+DW)-.04)],(0,1,0),.12,M['stone'])
ym=-(RIDGE-(dormer_z(XD+DW)-.04))/KF
for s in (-1,1):G['stone'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,dormer_z(XD+DW)-.04),(XD+s*DW,ym,dormer_z(XD+DW)-.04)],(1,0,0),.08,M['stone'])
for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
window(G,M,(XD,YD,DB+.22),(0,-1,0),.4,.4,shutters=False,wall_th=.12,band=band())
# ---- right-end stack: fieldstone with a dressed cap ------------------------------------
G['stone'].box((XC,YC,4.17),(.96,.8,2.56),M['stone'],.01)
G['stone'].box((XC,YC,5.5),(1.08,.92,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.22,.22):G['detail'].vessel((XC+dx,YC,5.56),[(.11,0),(.105,.3),(.115,.38),(.13,.42),(.10,.42),(.085,.36),(0,.36)],M['pot'])
# ---- walls --------------------------------------------------------------------------
PLATE=main_z(0,-D2)-.04
STEP,DS,DWD=.3,1.2,.95          # door sill = landing top; arch spring above it; door width
WIN,WH,WZ=.72,.85,1.0;WO=(WIN+.18,WZ-.09,WZ+WH+.09)
XL,XR=-1.92,2.2                 # front windows (as drawn, not symmetric about the door)
wall(G['stone'],M['stone'],(0,-D2,0),(0,-1,0),W2,PLATE,[(XL,*WO),(0,DWD,STEP,STEP+DS+DWD/2),(XR,*WO)],arch=(0,DWD,STEP+DS))
wall(G['stone'],M['stone'],(0,D2,0),(0,1,0),W2,PLATE,[(-1.8,*WO),(1.8,*WO)])
SIDE={-1:-.6,1:1.3}             # side window along each wall's own axis (left: y=+0.6, right: y=+1.3)
for s in (-1,1):
    wall(G['stone'],M['stone'],(s*W2,0,0),(s,0,0),D2-.3,PLATE,[(SIDE[s],*WO)],th=.3)
    top=[(y/10,main_z(W2,y/10)-.05) for y in range(-round(D2*10),round(D2*10)+1) if main_z(W2,y/10)-.05>PLATE+.01]
    G['stone'].prism([(s*(W2-.15),y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),.3,M['stone'])
# gable face (fieldstone infill) above the wall plate, with a window aperture
GWZ,GWW,GWH=3.2,.55,.65;OB,OT,OH=GWZ-.09,GWZ+GWH+.09,(GWW+.18)/2;GC=gable_z(XG+GW)-.05
def gx(z):return (RIDGE-.05-z)/KG     # half-width of the gable face at height z
for piece in ([(-GW,PLATE),(GW,PLATE),(GW,OB),(-GW,OB)],
              [(-GW,OB),(-OH,OB),(-OH,OT),(-gx(OT),OT),(-GW,GC)],[(OH,OB),(GW,OB),(GW,GC),(gx(OT),OT),(OH,OT)],[(-gx(OT),OT),(gx(OT),OT),(0,RIDGE-.05)]):
    G['stone'].prism([(XG+x,-D2+.15,z) for x,z in piece],(0,1,0),.3,M['stone'])
# grey rubble plinth skin (breaks for the door surround)
for a0,a1 in ((-W2-.04,-.95),(.95,W2+.04)):G['stone'].box(((a0+a1)/2,-D2-.02,PLINTH/2),(a1-a0,.1,PLINTH),M['plinth'],.01)
G['stone'].box((0,D2+.02,PLINTH/2),(2*W2+.08,.1,PLINTH),M['plinth'],.01)
for s in (-1,1):G['stone'].box((s*(W2+.02),0,PLINTH/2),(.1,2*D2,PLINTH),M['plinth'],.01)
# ---- timber framing: corner posts, wall plates, corner braces, lintels -------------------
def post(x,y,z0,z1,o):G['timber'].box((x,y,(z0+z1)/2),(.2 if o=='y' else .12,.12 if o=='y' else .2,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
for sx in (-1,1):
    for sy in (-1,1):G['timber'].box((sx*(W2-.08),sy*(D2-.08),(PLINTH+PLATE)/2),(.22,.22,PLATE-PLINTH),M['timber'],.015,grain=True,band=band())
y=-D2-.03
for a,b in ((-W2,XG-GW),(XG+GW,W2)):rail((a,y,PLATE-.08),(b,y,PLATE-.08),.18)
for s in (-1,1):rail((s*3.1,y,1.95),(s*2.75,y,PLATE-.15),.12)
for x in (XL,XR):rail((x-.58,y-.02,WZ+WH+.17),(x+.58,y-.02,WZ+WH+.17),.14)
# half-timbered gable over a bressumer
G['timber'].box((XG,y-.04,PLATE+.1),(2*GW+.2,.2,.2),M['timber'],.015,grain=True,band=band())
for x in (XG-GW+.08,XG+GW-.08):post(x,y,PLATE+.2,gable_z(x)-.05,'y')
rail((XG-GW,y,GWZ-.17),(XG+GW,y,GWZ-.17),.12);zc=OT+.12;rail((XG-gx(zc)+.05,y,zc),(XG+gx(zc)-.05,y,zc),.14);post(XG,y,zc+.07,RIDGE-.08,'y')
for s in (-1,1):rail((XG+s*(GW-.1),y,PLATE+.22),(XG+s*.5,y,GWZ-.24),.1)
for s in (-1,1):
    x=s*(W2+.03);rail((x,-D2,PLATE-.08),(x,D2,PLATE-.08),.18)
    for sy in (-1,1):rail((x,sy*(D2-.1),1.95),(x,sy*(D2-.45),PLATE-.15),.12)
y=D2+.03;rail((-W2,y,PLATE-.08),(W2,y,PLATE-.08),.18)
for s in (-1,1):rail((s*3.1,y,1.95),(s*2.75,y,PLATE-.15),.12)
# ---- openings ------------------------------------------------------------------------
for x in (XL,XR):window(G,M,(x,-D2,WZ),(0,-1,0),WIN,WH,planter=True,band=band())
for x in (-1.8,1.8):window(G,M,(x,D2,WZ),(0,1,0),WIN,WH,band=band())
window(G,M,(-W2,.6,WZ),(-1,0,0),WIN,WH,band=band())
window(G,M,(W2,1.3,WZ),(1,0,0),WIN,WH,shutters=False,band=band())   # beside the woodshed
window(G,M,(XG,-D2-.02,GWZ),(0,-1,0),GWW,GWH,band=band())
door(G,M,(0,-D2,STEP),(0,-1,0),DWD,DS,STONES,band=band())
# ---- porch hood -----------------------------------------------------------------------
LP=math.hypot(PH,PR-PE);kp=(PR-PE)/PH
plane((-PH,0,PE),(0,-1,0),(PH,0,PR-PE),LP,lambda t:D2,lambda t:-PY,[])
plane((PH,0,PE),(0,1,0),(-PH,0,PR-PE),LP,lambda t:PY,lambda t:-D2,[])
capped((0,PY,PR+.04),(0,-D2,PR+.04),5)
for s in (-1,1):G['timber'].beam((s*(PH+.04),PY-.03,PE-.07),(-s*.1,PY-.03,PR+.1*kp+.02),.07,.18,M['timber'],band=band())
for s in (-1,1):
    post(s*.85,PY+.15,STEP,PE-.02,'y');rail((s*.85,PY+.15,PE),(s*.85,-D2,PE),.16)
    rail((s*.85,PY+.15,1.55),(s*.55,PY+.15,PE-.03),.1);rail((s*.85,PY+.15,1.55),(s*.85,PY+.5,PE-.03),.1)
rail((-.97,PY+.15,PE+.02),(.97,PY+.15,PE+.02),.18);post(0,PY+.12,PE+.1,PR-.06,'y')
G['stone'].box((0,(PY+.05-D2)/2,STEP/2),(2.1,abs(PY+.05+D2),STEP),M['ashlar'],.015)
G['stone'].box((0,PY-.08,STEP/4),(1.6,.3,STEP/2),M['ashlar'],.015)
lantern(G,M,(0,PY+.55,PR-.12))
lantern(G,M,(-W2-.42,-D2+.32,2.15),arm=((-W2-.035,-D2+.32,2.15),(-1,0,0)))
# ---- lean-to woodshed on the right wall ---------------------------------------------------
LW=math.hypot(SX1-SX0,SZ0-SZ1);ks=(SZ0-SZ1)/(SX1-SX0)
def shed_z(x):return SZ1+(SX1-x)*ks
plane((SX1,0,SZ1),(0,1,0),(SX0-SX1,0,SZ0-SZ1),LW,lambda t:SY0-.1,lambda t:SY1+.1,[])
G['timber'].beam((SX1+.02,SY0-.1,SZ1-.05),(SX1+.02,SY1+.1,SZ1-.05),.04,.18,M['timber'],band=band())
G['timber'].beam((SX0+.06,SY0-.1,SZ0-.06),(SX0+.06,SY1+.1,SZ0-.06),.12,.12,M['timber'],band=band())
XP=SX1-.12
G['timber'].beam((SX0+.02,SY0-.12,SZ0+.03),(SX1+.04,SY0-.12,SZ1+.03),.04,.18,M['timber'],band=band())   # front verge board
for yy in (SY0+.07,SY1-.07):post(XP,yy,0,shed_z(XP)-.1,'x')
G['timber'].beam((XP,SY0,shed_z(XP)-.1),(XP,SY1,shed_z(XP)-.1),.14,.14,M['timber'],band=band())
for yy in (SY0+.07,SY1-.07):rail((XP,yy,1.2),(SX0+.04,yy,shed_z(SX0+.3)-.12),.1)
n=round((SY1-SY0-.14)/.16)
for i in range(n):                                  # outer side boarding
    yy=SY0+.07+(i+.5)*(SY1-SY0-.14)/n;G['timber'].box((XP+.09,yy,(shed_z(XP)-.12)/2),(.03,(SY1-SY0-.14)/n-.012,shed_z(XP)-.12),M['timber'],.004,grain=True,band=band())
n=round((XP-SX0-.07)/.16)
for i in range(n):                                  # rear boarding
    xx=SX0+(i+.5)*(XP-SX0-.07)/n;h=shed_z(xx+.08)-.1;G['timber'].box((xx,SY1-.02,h/2),((XP-SX0-.07)/n-.012,.03,h),M['timber'],.004,grain=True,band=band())
# ---- export, preview scene, manifest -----------------------------------------------------
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert lo[2]>-1e-3 and all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookCottage5'
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
cam('Beauty',(-9.5,-12.5,4.2),(82,0,-37),lens=38);cam('Front',(0,-30,3.15),(90,0,0),8.6);cam('Side',(-30,0,3.15),(90,0,-90),8.6);cam('Rear',(0,30,3.15),(90,0,180),8.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Cottage5-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'{tris} triangles exceeds the house budget'
manifest=dict(asset='SilverbrookCottage5',reference=dict(file='Cottage5-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_cottage5.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookCottage5',slot_m=list(SLOT),outputs=['SilverbrookCottage5.blend','SilverbrookCottage5.glb','SilverbrookCottage5.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('COTTAGE5_BUILT',manifest['bounds_m'],tris)
