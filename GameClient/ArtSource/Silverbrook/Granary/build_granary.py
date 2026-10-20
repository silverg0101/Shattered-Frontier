"""Silverbrook Granary from docs/Game-Design/concept-art/Silverbrook/Granary.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_granary.py
Original procedural geometry interpreting the elevation (9 m to the gable apex): steep-hipped
slate roof crossed by a taller half-timbered loading gable with a louvred vent and a hoist,
two louvred dormers, a half-timbered loft storey with great double loft doors over a timber
gallery reached by an open stair, and a rubble-stone undercroft with an arched double door
between pale ashlar piers. The 12 m concept is narrowed to the 8 x 8 x 10 m town slot;
storey heights are kept at full size.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,door,lantern
import bpy,numpy as np
from mathutils import Matrix
scene=kit.reset(4120)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Granary.png'
asset=bpy.data.collections.new('SilverbrookGranary');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Ashlar','stone_wall_04'),
    rubble=kit.scan('SB_Granary_Rubble','japanese_stone_wall',dark=(.30,.33,.40),aged=.8),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    rope=kit.solid('SB_Granary_Rope',(.30,.22,.13),.9))
M['ashlar']=M['stone']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('stone_wall_04',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBGranary_Masonry',asset),plaster=Geo('SM_SBGranary_Plaster',asset),timber=Geo('SM_SBGranary_Timber',asset),roof=Geo('SM_SBGranary_Roof',asset),detail=Geo('SM_SBGranary_Details',asset))
# ---- measured layout (metres) -------------------------------------------------
SLOT=(8,8,10)                  # town-layout slot w x d x h (concept README 'Fits')
W2,D2,TH=3.4,3.0,.3            # outer wall half-extents, wall thickness
LOFT,DECK=2.5,2.62             # undercroft top, gallery / loft floor
EAVE,RIDGE=5.3,8.4             # main hip roof
EX,EY,RX=3.75,3.35,2.95        # eave half-extents, ridge half-length
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
GH,GW,GR=1.95,1.65,9.0         # loading gable (front to back): roof half, face half, apex
KG=(GR-EAVE)/GH;GY=-D2-.45
DH,DW,DE,DR=.48,.38,7.0,7.55   # dormers at x = +-XD
XDS=(-2.55,2.55);KD=(DR-DE)/DH
YD=-(RIDGE-6.35)/KF            # dormer face where the main roof is 6.35 m up
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return GR-abs(x)*KG
def dormer_z(x,XD):return DR-abs(x-XD)*KD
in_gable=lambda p:abs(p.x)<GH and p.z<gable_z(p.x)-.005
def in_dormer(XD):return lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x,XD)-.005
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/3) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def spike(geo,base,half,height,mat):
    b=Vector(base);c=[b+Vector(v) for v in ((-half,-half,0),(half,-half,0),(half,half,0),(-half,half,0))];apex=b+Vector((0,0,height))
    for i in range(4):geo.poly([c[i],c[(i+1)%4],apex],mat)
    geo.poly(c[::-1],mat)
def finial(x,y,z0,post_h,spike_h,s=.16):
    """Oak post with collar under a forged-iron spike (the concept's dark pinnacles)."""
    G['timber'].box((x,y,z0+post_h/2),(s,s,post_h),M['timber'],.015,grain=True,band=band())
    G['timber'].box((x,y,z0+post_h-.03),(s+.06,s+.06,.06),M['timber'],.012,grain=True,band=band())
    spike(G['detail'],(x,y,z0+post_h),s/2+.01,spike_h,M['iron'])
def post(x,y,z0,z1,along_x=True,s=.2):G['timber'].box((x,y,(z0+z1)/2),(s if along_x else .12,.12 if along_x else s,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
def louvre(c,o,w,h,wall_th=TH,t=.07):
    """Louvred vent: oak frame and sill, raked boards shedding outward, boarded back.
    c: bottom centre of the louvred area on the outer wall face. Its wall hole is hole(a,w,z,h)."""
    P,R=kit.frame(c,o)
    def B(a,d,z,size,mat=M['timber'],geo='timber',rot=None):G[geo].box(P(a,d,z),size,mat,.006,R@rot if rot else R,**({'grain':True,'band':band()} if mat is M['timber'] else {}))
    for a in (-(w/2+t/2),w/2+t/2):B(a,-wall_th/2+.02,h/2,(t,wall_th+.04,h+2*t))
    B(0,-wall_th/2+.02,h+t/2,(w,wall_th+.04,t));B(0,-wall_th/2+.03,-t/2,(w+2*t+.1,wall_th+.1,t))
    B(0,-wall_th+.01,h/2,(w,.02,h),M['door'],'detail')
    n=max(3,round(h/.09));sd=min(.13,wall_th*.8)
    for i in range(n):B(0,-.02-sd*.38,(i+.6)*h/n,(w-.005,sd,.016),rot=Matrix.Rotation(math.radians(-40),3,'X'))
def hole(a,w,z,h,t=.07):return (a,w+2*t,z-t,z+h+t)
# ---- main hip roof --------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,*map(in_dormer,XDS)])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
X0=(GR-RIDGE)/KG               # main ridge dies into the loading gable here
for sx in (-1,1):
    capped((sx*X0,0,RIDGE+.05),(sx*RX,0,RIDGE+.05),6)
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.05),(sx*RX,0,RIDGE+.05),14)
    finial(sx*RX,0,RIDGE+.03,.42,.32)
for sy in (-1,1):
    for a,b in ((-EX,-GH),(GH,EX)):G['timber'].beam((a,sy*(EY+.02),EAVE-.06),(b,sy*(EY+.02),EAVE-.06),.04,.2,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.06),(sx*(EX+.02),EY,EAVE-.06),.04,.2,M['timber'],band=band())
# eaves gutters and downpipes (front corners, swan-necked back to the side walls)
for sy in (-1,1):
    for s in (-1,1):G['detail'].beam((s*(GH+.12),sy*(EY+.08),EAVE-.1),(s*EX,sy*(EY+.08),EAVE-.1),.13,.1,M['iron'],grain=False)
for s in (-1,1):
    top=Vector((s*(EX-.1),-EY-.08,EAVE-.14));knee=Vector((s*(W2+.06),-D2+.15,EAVE-.75))
    G['detail'].beam(top,top-Vector((0,0,.2)),.08,.08,M['iron'],grain=False)
    G['detail'].beam(top-Vector((0,0,.2)),knee,.08,.08,M['iron'],grain=False)
    G['detail'].beam(knee,(knee.x,knee.y,.2),.08,.08,M['iron'],grain=False)
    G['detail'].beam((knee.x,knee.y,.24),(knee.x+s*.16,knee.y,.06),.08,.08,M['iron'],grain=False)
    for z in (1.0,2.6,4.2):G['detail'].box((knee.x-s*.04,knee.y,z),(.1,.12,.03),M['iron'],.004)
# ---- loading gable roof (runs front to back), bargeboards and finials --------------------
LG=math.hypot(GH,GR-EAVE)
plane((-GH,0,EAVE),(0,-1,0),(GH,0,GR-EAVE),LG,lambda t:GY,lambda t:-GY,[under_main])
plane((GH,0,EAVE),(0,1,0),(-GH,0,GR-EAVE),LG,lambda t:GY,lambda t:-GY,[under_main])
capped((0,GY,GR+.05),(0,-GY,GR+.05),26)
GC=gable_z(GW)-.08;yq=(RIDGE-GC)/KF
for sy in (-1,1):
    for s in (-1,1):G['timber'].beam((s*(GH+.05),sy*(-GY+.04),EAVE-.08),(-s*.16,sy*(-GY+.04),GR+.16*KG+.03),.08,.24,M['timber'],band=band())
    for s in (-1,1):G['plaster'].prism([(s*(GW-.06),sy*D2,main_z(0,D2)-.04),(s*(GW-.06),sy*D2,GC),(s*(GW-.06),sy*yq,GC)],(1,0,0),.1,M['plaster'])
    finial(0,sy*(-GY+.06),GR-.3,.72,.32,.18)
# ---- dormers (oak-boarded, gabled, louvred, finials) --------------------------------------
LD=math.hypot(DH,DR-DE);DB=RIDGE-abs(YD)*KF-.1;YB=(RIDGE-DR)/KF
for XD in XDS:
    plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:YB-.1,lambda t:-YD+.16,[under_main])
    plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:-YB+.1,[under_main])
    capped((XD,YD-.16,DR+.04),(XD,-YB-.05,DR+.04),5)
    for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
    finial(XD,YD-.2,DR-.05,.28,.24,.1)
    top=dormer_z(XD+DW,XD)-.04
    wall(G['timber'],M['timber'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[hole(0,.32,.22,.39)],th=.12)
    G['timber'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,top),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,top)],(0,1,0),.12,M['timber'])
    ymc=-(RIDGE-top)/KF
    for s in (-1,1):G['timber'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,top),(XD+s*DW,ymc,top)],(1,0,0),.08,M['timber'])
    for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.03,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
    G['timber'].box((XD,YD-.04,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
    louvre((XD,YD,DB+.22),(0,-1,0),.32,.39,.12)
# ---- walls: rubble undercroft, plaster loft storey ------------------------------------
PLATE=main_z(0,-D2)-.04;UH=PLATE-LOFT
FRONT=(0,-D2,0),(0,-1,0)
DOOR_W,DOOR_S=1.6,1.25                       # undercroft arched double door
LD_H,LD_T=1.23,4.95                          # loft doors: half width, head
WO,WN,WZ,WH=.42,.2,3.66,.7                   # loft louvres: outer/inner width, sill, height
wall(G['stone'],M['rubble'],*FRONT,W2,LOFT,[(0,DOOR_W,0,DOOR_S+DOOR_W/2)],arch=(0,DOOR_W,DOOR_S))
wall(G['stone'],M['rubble'],(0,D2,0),(0,1,0),W2,LOFT)
for s in (-1,1):wall(G['stone'],M['rubble'],(s*W2,0,0),(s,0,0),D2-TH,LOFT,th=TH)
wall(G['plaster'],M['plaster'],(0,-D2,LOFT),(0,-1,0),W2,UH,[(0,2*LD_H,0,LD_T-LOFT)]+[hole(s*a,w,WZ-LOFT,WH) for s in (-1,1) for a,w in ((2.8,WO),(1.95,WN))])
wall(G['plaster'],M['plaster'],(0,D2,LOFT),(0,1,0),W2,UH,[hole(a,WO,WZ-LOFT,WH) for a in (-1.8,1.8)])
for s in (-1,1):wall(G['plaster'],M['plaster'],(s*W2,0,LOFT),(s,0,0),D2-TH,UH,[hole(0,WO,WZ-LOFT,WH)],th=TH)
# loading-gable faces above the wall plate, front and rear, each round a big louvred vent
LW,LH,LZ=1.0,1.15,6.2;OH,OB,OT=LW/2+.07,LZ-.07,LZ+LH+.07
def gx(z):return (GR-.08-z)/KG
for sy in (-1,1):
    for piece in ([(-GW,PLATE),(GW,PLATE),(GW,GC),(gx(OB),OB),(-gx(OB),OB),(-GW,GC)],[(-gx(OB),OB),(-OH,OB),(-OH,OT),(-gx(OT),OT)],
                  [(OH,OB),(gx(OB),OB),(gx(OT),OT),(OH,OT)],[(-gx(OT),OT),(gx(OT),OT),(0,GR-.08)]):
        G['plaster'].prism([(x,sy*(D2-TH/2),z) for x,z in piece],(0,1,0),TH,M['plaster'])
    louvre((0,sy*D2,LZ),(0,sy,0),LW,LH)
# ---- undercroft masonry: corner quoins, pale ashlar piers -------------------------------
for sx in (-1,1):
    for sy in (-1,1):
        z=0;i=0
        while z<LOFT-.05:
            h=min(.31,LOFT-z);a,b=(.46,.3) if i%2 else (.3,.46)
            G['stone'].box((sx*(W2+.02-a/2),sy*(D2+.02-b/2),z+h/2),(a,b,h-.012),M['ashlar'],.012,rect=kit.rng.choice(STONES));z+=h;i+=1
PIERS=(-1.5,1.5,2.5);PZ=1.3
for x in PIERS:
    for i in range(4):
        w=.5 if i%2 else .44;G['stone'].box((x,-D2-.06,PZ*(i+.5)/4),(w,.16,PZ/4-.012),M['ashlar'],.012,rect=kit.rng.choice(STONES))
door(G,M,(0,-D2,0),(0,-1,0),DOOR_W,DOOR_S,STONES)
P,R=kit.frame(*FRONT)
G['detail'].box(P(0,-.155,(DOOR_S+DOOR_W/2)/2),(.05,.02,DOOR_S+DOOR_W/2-.02),M['door'],.004,R)   # meeting stile of the pair
for k in range(10):
    t=math.tau*k/10;G['detail'].box(P(-DOOR_W/4*.55*2+.06*math.cos(t),-.13,1.0+.06*math.sin(t)),(.04,.014,.014),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
G['detail'].box(P(-DOOR_W/4*.55*2,-.155,1.06),(.07,.01,.07),M['iron'],.004,R)
# ---- gallery: posts on the piers, knee braces, joists and boards -------------------------
SX0,NR=-3.95,12;RISE=DECK/NR;GO=(-1.6-SX0)/(NR-1);SXD=SX0+(NR-1)*GO   # open stair to the gallery
GD=.9;GF=-D2-GD
for x in PIERS:
    G['timber'].box((x,-D2-.1,(PZ+2.37)/2),(.2,.18,2.37-PZ),M['timber'],.012,grain=True,band=band())
    rail((x,-D2-.12,1.72),(x,GF+.2,2.4),.12)
for x in (SXD+.07,-.75,0,.75,1.5,2.5,3.0,W2-.07):G['timber'].box((x,-D2-GD/2,2.47),(.14,GD,.2),M['timber'],.012,grain=True,band=band())
G['timber'].box(((SXD+W2)/2,GF+.07,2.47),(W2-SXD,.14,.22),M['timber'],.012,grain=True,band=band())
for i in range(6):G['timber'].box(((SXD+W2)/2,-D2-GD*(i+.5)/6,DECK-.027),(W2-SXD,GD/6-.008,.055),M['door'],.004,grain=True)
# ---- open stair --------------------------------------------------------------------
for y in (-D2-.05,GF+.04):G['timber'].beam((SX0+.12,y,.2),(SXD+.02,y,DECK-.15),.06,.26,M['timber'],band=band())
for k in range(1,NR):G['timber'].box((SX0+(k-.5)*GO+.01,-D2-GD/2,k*RISE-.025),(GO+.03,GD-.06,.05),M['door'],.004,grain=True)
HR=.95;yr=GF-.02
def rail_z(x):return HR+(x-SX0)*RISE/GO+(.12 if x<SX0+.1 else 0)
for x in (SX0+.06,SX0+.6,SX0+1.2,SX0+1.8,SXD):
    zb=max(0,math.floor((x-SX0)/GO)*RISE) if x>SX0+.1 else 0
    G['timber'].box((x,yr,(zb+rail_z(x))/2+.02),(.1,.1,rail_z(x)-zb+.04),M['timber'],.01,grain=True,band=band())
rail((SX0+.06,yr,rail_z(SX0+.06)-.12),(SXD,yr,rail_z(SXD)),.1);rail((SX0+.06,yr,rail_z(SX0+.06)-.6),(SXD,yr,rail_z(SXD)-.5),.08)
G['timber'].box((SX0+.06,yr,1.12),(.14,.14,.06),M['timber'],.01,grain=True,band=band())
# ---- loft storey timber frame -----------------------------------------------------------
y=-D2-.03
for s in (-1,1):
    for x in (W2-.1,2.3,LD_H+.1):post(s*x,y,DECK+.18 if x<3 else LOFT,PLATE-.1)
    rail((s*W2,y,DECK+.09),(s*(LD_H+.2),y,DECK+.09),.18);rail((s*W2,y,WZ-.15),(s*(LD_H+.2),y,WZ-.15),.12);rail((s*W2,y,WZ+WH+.15),(s*(LD_H+.2),y,WZ+WH+.15),.12)
    rail((s*(W2-.2),y,WZ+WH+.24),(s*2.42,y,PLATE-.2),.1);rail((s*2.18,y,WZ+WH+.24),(s*(LD_H+.22),y,PLATE-.2),.1)
    rail((s*(W2-.2),y,DECK+.2),(s*2.42,y,WZ-.24),.1)
rail((-W2,y,PLATE-.08),(W2,y,PLATE-.08),.18)
G['timber'].box((0,y-.02,LD_T+.1),(2*LD_H+.4,.2,.2),M['timber'],.012,grain=True,band=band())     # loft-door head
G['timber'].box((0,-D2+TH/2,(LOFT+DECK)/2),(2*LD_H,TH,DECK-LOFT),M['timber'],.008,grain=True,band=band())  # threshold
# loft doors: ledged and braced pair with long strap hinges and ring pulls
for s in (-1,1):
    lc=s*LD_H/2;hl=LD_T-DECK
    G['detail'].box(P(lc,-.15,DECK+hl/2),(LD_H-.01,.07,hl),M['door'],.004,R,grain=True)
    for z in (DECK+.3,DECK+hl/2,LD_T-.3):G['timber'].box(P(lc,-.09,z),(LD_H-.06,.05,.16),M['timber'],.008,R,grain=True,band=band())
    for z0,z1 in ((DECK+.38,DECK+hl/2-.08),(DECK+hl/2+.08,LD_T-.38)):G['timber'].beam(P(lc+s*(LD_H/2-.12),-.09,z0),P(lc-s*(LD_H/2-.12),-.09,z1),.05,.12,M['timber'],band=band())
    for z in (DECK+.3,DECK+hl/2,LD_T-.3):G['detail'].box(P(s*(LD_H-.42),-.059,z),(.8,.012,.06),M['iron'],.003,R)
    for k in range(10):
        t=math.tau*k/10;G['detail'].box(P(s*.15+.07*math.cos(t),-.05,3.6+.07*math.sin(t)),(.045,.016,.016),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
    G['detail'].box(P(s*.15,-.06,3.67),(.07,.012,.07),M['iron'],.004,R)
for s in (-1,1):
    for x,w in ((2.8,WO),(1.95,WN)):louvre((s*x,-D2,WZ),(0,-1,0),w,WH)
    lantern(G,M,(s*1.62,-D2-.45,4.25),arm=((s*1.62,-D2-.06,4.25),(0,-1,0)))
# side and rear framing
for s in (-1,1):
    x=s*(W2+.03)
    for yy in (-1.15,1.15):post(x,yy,LOFT,PLATE-.1,False)
    rail((x,-D2,LOFT+.09),(x,D2,LOFT+.09),.18)
    rail((x,-D2,PLATE-.08),(x,D2,PLATE-.08),.18);rail((x,-D2,WZ-.15),(x,D2,WZ-.15),.12)
    for d in (-1,1):rail((x,d*1.25,WZ-.05),(x,d*2.3,PLATE-.2),.1)
    louvre((s*W2,0,WZ),(s,0,0),WO,WH)
y=D2+.03
for x in (-W2+.1,-1.15,1.15,W2-.1,-2.6,2.6):post(x,y,LOFT,PLATE-.1)
for z,sz in ((LOFT+.09,.18),(WZ-.15,.12),(PLATE-.08,.18)):rail((-W2,y,z),(W2,y,z),sz)
for x in (-1.8,1.8):louvre((x,D2,WZ),(0,1,0),WO,WH)
# loading-gable framing front and rear: posts, braces, collar, king post
for sy in (-1,1):
    y=sy*(D2+.03)
    for s in (-1,1):
        post(s*(GW-.1),y,PLATE+.01,gable_z(GW-.1)-.1)
        rail((s*(GW-.2),y,PLATE+.06),(s*(OH+.06),y,OB+.45),.1)
        post(s*(OH+.06),y,PLATE+.01,OB-.02)
    G['timber'].box((0,y+sy*.02,OB-.04),(2*OH+.3,.2,.1),M['timber'],.01,grain=True,band=band())
    zc=OT+.1;rail((-gx(zc)+.06,y,zc),(gx(zc)-.06,y,zc),.14);post(0,y,zc+.07,GR-.14)
# hoist: beam out of the king post, pulley block, rope and hook (front only)
G['timber'].box((0,-D2-.38,7.78),(.16,1.06,.18),M['timber'],.012,grain=True,band=band())
G['timber'].box((0,-D2-.12,7.5),(.12,.12,.4),M['timber'],.01,grain=True,band=band())
G['timber'].beam((0,-D2-.08,7.3),(0,-D2-.62,7.72),.08,.08,M['timber'],band=band())
pul=[(0,-D2-.78+.11*math.cos(t),7.5+.11*math.sin(t)) for t in np.linspace(0,math.tau,13)[:-1]]
G['detail'].prism(pul,(1,0,0),.05,M['iron'])
for s in (-1,1):G['detail'].box((s*.04,-D2-.78,7.6),(.012,.03,.25),M['iron'],0)
G['detail'].beam((0,-D2-.89,7.5),(0,-D2-.89,6.15),.026,.026,M['rope'],grain=False)
G['detail'].beam((0,-D2-.67,7.5),(0,-D2-.67,6.9),.026,.026,M['rope'],grain=False)
G['detail'].box((0,-D2-.89,6.1),(.03,.03,.1),M['iron'],.004)
G['detail'].prism([(0,-D2-.89+.07*math.cos(t),5.98+.07*math.sin(t)) for t in np.linspace(math.pi*.15,math.pi*1.25,8)],(1,0,0),.025,M['iron'])
# ---- checks, export, preview scene, manifest ------------------------------------------
# no wall top may rise through the hip roof (loading-gable faces and cheeks are meant to)
for k in ('stone','plaster'):
    for p in G[k].v:
        if abs(p.x)<=GW+.06 and p.z<gable_z(p.x):continue
        assert p.z<=main_z(p.x,p.y)+1e-3,f'{k} vertex {tuple(round(c,3) for c in p)} pokes through the roof ({main_z(p.x,p.y):.3f})'
for p in G['plaster'].v:assert abs(p.x)>GH or p.z<=gable_z(p.x)+1e-3,f'gable face vertex {tuple(round(c,3) for c in p)} pokes through the gable roof'
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert lo[2]>-1e-3,f'geometry below ground: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookGranary'
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
cam('Beauty',(-11.5,-14.7,4.6),(84,0,-38),lens=36);cam('Front',(0,-30,4.9),(90,0,0),10.6);cam('Side',(-30,0,4.9),(90,0,-90),10.6);cam('Rear',(0,30,4.9),(90,0,180),10.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Granary-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'{tris} triangles exceeds the 120k house budget'
manifest=dict(asset='SilverbrookGranary',asset_id='BLD-GreenlandsGranary-01',reference=dict(file='Granary-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_granary.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookGranary',slot_m=list(SLOT),outputs=['SilverbrookGranary.blend','SilverbrookGranary.glb','SilverbrookGranary.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('GRANARY_BUILT',manifest['bounds_m'],tris)
