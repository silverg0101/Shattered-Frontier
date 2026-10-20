"""Silverbrook Stables from docs/Game-Design/concept-art/Silverbrook/Stables.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_stables.py
Original procedural geometry interpreting the elevation (7 m to the gable finial):
steep-hipped slate roof with two gabled dormers and a ridge stack, a central
half-timbered hay-loft gable with a hoist, and a stone ground storey with a
segmental-arched double carriage door between two arched stall bays with half gates,
all under an oak post-and-plate frame with knee braces and bracket lanterns.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,window,lantern
import bpy,numpy as np
from mathutils import Matrix
scene=kit.reset(4112)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Stables.png'
asset=bpy.data.collections.new('SilverbrookStables');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Ashlar','stone_wall_04'),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('stone_wall_04',(.05,.035),dark=.7,frac=.01,std=.22,bright=1.05)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBStables_Masonry',asset),plaster=Geo('SM_SBStables_Plaster',asset),timber=Geo('SM_SBStables_Timber',asset),roof=Geo('SM_SBStables_Roof',asset),detail=Geo('SM_SBStables_Details',asset))
# ---- measured layout (metres) -------------------------------------------------
SLOT=(12,8,7)                  # town-layout slot w x d x h (concept README 'Fits')
W2,D2=5.0,3.2                  # outer wall half-extents
EAVE,RIDGE=2.95,6.2            # main hip roof
EX,EY,RX=5.35,3.55,4.35        # eave half-extents, ridge half-length
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
GH,GW,GE=1.45,1.1,4.5          # hay-loft gable: roof half, wall half, eave on the main slope
KG=(RIDGE-GE)/GH;GY=-D2-.3
DH,DW,DE,DR=.62,.5,4.3,4.95    # dormers at x = +-XD
XDS=(-2.75,2.75);KD=(DR-DE)/DH
YD=-(RIDGE-3.6)/KF             # dormer face where the main roof is 3.6 m up
XC,YC=3.65,-.9                 # stack on the front slope, right of the right dormer
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return RIDGE-abs(x)*KG
def dormer_z(x,XD):return DR-abs(x-XD)*KD
in_gable=lambda p:abs(p.x)<GW+.04 and p.y<0 and p.z<gable_z(p.x)-.005
def in_dormer(XD):return lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x,XD)-.005
in_stack=lambda p:abs(p.x-XC)<.47 and abs(p.y-YC)<.42
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/2.5) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered,deck_cover=None):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],deck_cover if deck_cover is not None else covered)
def spike(geo,base,half,height,mat):
    """Square pyramid finial cap, outward winding."""
    b=Vector(base);c=[b+Vector(v) for v in ((-half,-half,0),(half,-half,0),(half,half,0),(-half,half,0))];apex=b+Vector((0,0,height))
    for i in range(4):geo.poly([c[i],c[(i+1)%4],apex],mat)
    geo.poly(c[::-1],mat)
def finial(x,y,z0,post_h,spike_h,s=.16):
    G['timber'].box((x,y,z0+post_h/2),(s,s,post_h),M['timber'],.015,grain=True,band=band())
    G['timber'].box((x,y,z0+post_h-.03),(s+.06,s+.06,.06),M['timber'],.012,grain=True,band=band())
    spike(G['timber'],(x,y,z0+post_h),s/2+.01,spike_h,M['timber'])
# ---- main hip roof --------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,in_stack,*map(in_dormer,XDS)])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
capped((-RX,0,RIDGE+.05),(RX,0,RIDGE+.05),28)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.05),(sx*RX,0,RIDGE+.05),13)
    finial(sx*RX,0,RIDGE+.03,.42,.32)
for a,b in ((-EX,-GW),(GW,EX)):G['timber'].beam((a,-EY-.02,EAVE-.06),(b,-EY-.02,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((-EX,EY+.02,EAVE-.06),(EX,EY+.02,EAVE-.06),.04,.2,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.06),(sx*(EX+.02),EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- hay-loft gable roof, bargeboards and finial --------------------------------------
LG=math.hypot(GH,RIDGE-GE)
plane((-GH,0,GE),(0,-1,0),(GH,0,RIDGE-GE),LG,lambda t:0,lambda t:-GY,[under_main])
plane((GH,0,GE),(0,1,0),(-GH,0,RIDGE-GE),LG,lambda t:GY,lambda t:0,[under_main])
capped((0,GY,RIDGE+.05),(0,-.4,RIDGE+.05),12)
for s in (-1,1):G['timber'].beam((s*(GH+.05),GY-.04,GE-.08),(-s*.16,GY-.04,RIDGE+.16*KG+.03),.08,.24,M['timber'],band=band())
GC=gable_z(GW)-.05;yq=-(RIDGE-GC)/KF
for s in (-1,1):G["plaster"].prism([(s*(GW-.01),-D2,main_z(0,-D2)-.04),(s*(GW-.01),-D2,GC),(s*(GW-.01),yq,GC)],(1,0,0),.1,M['plaster'])
finial(0,GY-.06,RIDGE-.3,.75,.3,.18)
# ---- dormers (boarded, gabled, finials) -----------------------------------------------
LD=math.hypot(DH,DR-DE);DB=RIDGE-abs(YD)*KF-.1
def cross_window(c,o,w,h,wall_th):
    """Four-light lit window: oak frame, mullion and transom (dormers)."""
    P,R=kit.frame(c,o);t=.07
    def B(a,d,z,size,mat,geo='timber'):G[geo].box(P(a,d,z),size,mat,.006,R,**({'grain':True,'band':band()} if mat is M['timber'] else {}))
    B(0,-.07,h/2,(w,.01,h),M['glass'],'detail')
    for a in (-(w/2+t/2),w/2+t/2):B(a,-wall_th/2+.02,h/2,(t,wall_th+.04,h+2*t),M['timber'])
    for z in (h+t/2,-t/2):B(0,-wall_th/2+.02,z,(w,wall_th+.04,t),M['timber'])
    B(0,-.04,h/2,(.045,.06,h),M['timber']);B(0,-.04,h/2,(w,.06,.045),M['timber'])
    B(0,.04,-t-.02,(w+2*t+.1,.12,.04),M['timber'])
for XD in XDS:
    plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:1.3,lambda t:-YD+.16,[under_main])
    plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:-1.3,[under_main])
    capped((XD,YD-.16,DR+.04),(XD,-(RIDGE-DR)/KF-.05,DR+.04),5)
    for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
    finial(XD,YD-.2,DR-.05,.28,.22,.1)
    top=dormer_z(XD+DW,XD)-.04
    wall(G['timber'],M['timber'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.58,.15,.73)],th=.12)
    G['timber'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,top),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,top)],(0,1,0),.12,M['timber'])
    ymc=-(RIDGE-top)/KF
    for s in (-1,1):G['timber'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,top),(XD+s*DW,ymc,top)],(1,0,0),.08,M['timber'])
    for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.03,(DB+DE)/2),(.12,.12,DE-DB),M['timber'],.01,grain=True,band=band())
    G['timber'].box((XD,YD-.04,DE-.02),(2*DW+.08,.14,.13),M['timber'],.01,grain=True,band=band())
    cross_window((XD,YD,DB+.22),(0,-1,0),.44,.44,.12)
# ---- ridge stack ------------------------------------------------------------------
G['stone'].box((XC,YC,5.45),(.86,.76,2.1),M['stone'],.01)
G['stone'].box((XC,YC,6.47),(.92,.82,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,6.65),(.98,.88,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.2,.2):G['detail'].vessel((XC+dx,YC,6.71),[(.1,0),(.095,.17),(.105,.22),(.12,.25),(.09,.25),(.075,.21),(0,.21)],M['pot'])
# ---- walls ------------------------------------------------------------------------
PLATE=main_z(0,-D2)-.04;TH=.3
STALLS=(-3.25,3.25);SH,SS,SC=.95,1.6,2.25        # stall bays: half width, spring, crown
CH,CS,CC=1.3,1.7,2.25                           # carriage door
def seg(h,spring,crown):
    R=(h*h+(crown-spring)**2)/(2*(crown-spring));z0=crown-R;phi=math.asin((spring-z0)/R);return R,z0,phi
def arch(c,o,a,h,spring,crown,n):
    """Segmental stone head: spandrels close the opening above the curve, voussoir ring proud of the face."""
    P,R=kit.frame(c,o);Rad,z0,phi=seg(h,spring,crown);ov=Vector(o).normalized()
    for side in (-1,1):
        arc=[P(a+side*Rad*math.cos(t),0,z0+Rad*math.sin(t)) for t in np.linspace(phi,math.pi/2,10)]
        G['stone'].prism([p-ov*TH/2 for p in arc+[P(a+side*h,0,crown)]],o,TH,M['stone'])
    depth=.3
    for k in range(n):
        t=phi+(math.pi-2*phi)*(k+.5)/n;rm=Rad-.012+depth/2;radial=R@Vector((math.cos(t),0,math.sin(t)));tang=R@Vector((-math.sin(t),0,math.cos(t)))
        G['stone'].box(P(a+rm*math.cos(t),-TH/2+.025,z0+rm*math.sin(t)),((math.pi-2*phi)*rm/n-.014,TH+.05,depth),M['ashlar'],.012,Matrix((tang,ov,radial)).transposed(),rect=kit.rng.choice(STONES))
WIN,WH,WZ=.6,.75,1.15;WO=(WIN+.18,WZ-.09,WZ+WH+.09)
FRONT=(0,-D2,0),(0,-1,0)
wall(G['stone'],M['stone'],*FRONT,W2,PLATE,[(a,2*SH,0,SC) for a in STALLS]+[(0,2*CH,0,CC)])
for a in STALLS:arch(*FRONT,a,SH,SS,SC,9)
arch(*FRONT,0,CH,CS,CC,11)
wall(G['stone'],M['stone'],(0,D2,0),(0,1,0),W2,PLATE,[(-3.0,WO[0],WO[1],WO[2]),(3.0,WO[0],WO[1],WO[2])])
for s in (-1,1):
    wall(G['stone'],M['stone'],(s*W2,0,0),(s,0,0),D2-TH,PLATE,[(1.4,WO[0],WO[1],WO[2])],th=TH)
    top=[(y/10,main_z(W2,y/10)-.05) for y in range(-round(D2*10),round(D2*10)+1) if main_z(W2,y/10)-.05>PLATE+.01]
    G['stone'].prism([(s*(W2-TH/2),y,z) for y,z in [(top[-1][0],PLATE),(top[0][0],PLATE)]+top],(1,0,0),TH,M['stone'])
# hay-loft gable face (plaster infill) with the loft door aperture
OB,OT,OH=3.35,4.45,.42
def gx(z):return (RIDGE-.05-z)/KG
for piece in ([(-GW,PLATE),(GW,PLATE),(GW,OB),(-GW,OB)],[(-GW,OB),(-OH,OB),(-OH,OT),(-GW,OT)],[(OH,OB),(GW,OB),(GW,OT),(OH,OT)],
              [(-GW,OT),(GW,OT),(GW,GC),(0,RIDGE-.05),(-GW,GC)]):
    G['plaster'].prism([(x,-D2+.15,z) for x,z in piece],(0,1,0),.3,M['plaster'])
# ---- stall bays: shallow boarded niches (no interior) with half gates ---------------
NB=-D2+1.0
for a in STALLS:
    G['detail'].box((a,NB+.025,1.3),(2*SH+.6,.05,2.6),M['door'],.004)
    for s in (-1,1):G['detail'].box((a+s*(SH+.3),(NB-D2+TH)/2,1.3),(.05,NB-(-D2+TH)+.01,2.6),M['door'],.004)
    G['detail'].box((a,(NB-D2+TH)/2,2.575),(2*SH+.6,NB-(-D2+TH)+.01,.05),M['door'],.004)
    G['timber'].box((a,NB-.04,1.25),(2*SH+.55,.08,.12),M['timber'],.01,grain=True,band=band())   # manger rail
    lantern(G,M,(a,NB-.35,2.47))
    P,R=kit.frame(*FRONT);GT=1.15;lw=SH+.02
    for side in (-1,1):
        lc=a+side*lw/2
        G['detail'].box(P(lc,-.12,GT/2),(lw-.01,.06,GT),M['door'],.004,R)
        for z in (.1,GT-.07):G['timber'].box(P(lc,-.07,z),(lw-.02,.05,.12),M['timber'],.008,R,grain=True,band=band())
        for d in (-1,1):G['timber'].beam(P(lc-d*(lw/2-.1),-.075,.18),P(lc+d*(lw/2-.1),-.075,GT-.15),.08,.04,M['timber'],band=band())
        for z in (.32,.82):G['detail'].box(P(lc-side*.05,-.085,z),(lw-.1,.012,.045),M['iron'],.003,R)
        G['detail'].box(P(a+side*.04,-.08,.62),(.04,.02,.12),M['iron'],.003,R)   # latch
# ---- carriage doors -----------------------------------------------------------------
P,R=kit.frame(*FRONT);Rad,z0,phi=seg(CH,CS,CC)
for side in (-1,1):
    arc=[P(side*(Rad+.02)*math.cos(t),-.1,z0+(Rad+.02)*math.sin(t)) for t in np.linspace(math.pi/2,phi,10)]
    G['detail'].prism([P(side*(CH+.02),-.1,0),P(side*.005,-.1,0),P(side*.005,-.1,CC+.02)]+arc[1:]+[P(side*(CH+.02),-.1,CS)],(0,1,0),.08,M['door'],w=(0,0))
for side in (-1,1):
    for z in (.3,1.05,1.75):G['detail'].box(P(side*(CH-.55),-.055,z),(1.05,.014,.07),M['iron'],.004,R)
    for k in range(10):
        t=math.tau*k/10;G['detail'].box(P(side*.14+.07*math.cos(t),-.03,1.05+.07*math.sin(t)),(.045,.016,.016),M['iron'],0,R@Matrix.Rotation(t,3,'Y'))
    G['detail'].box(P(side*.14,-.05,1.12),(.07,.012,.07),M['iron'],.004,R)
# ---- timber frame -------------------------------------------------------------------
def post(x,y,z0,z1,along_x):G['timber'].box((x,y,(z0+z1)/2),(.22 if along_x else .16,.16 if along_x else .22,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.14,s,M['timber'],band=band())
def base(x,y,along_x):G['stone'].box((x,y,.275),(.38 if along_x else .3,.3 if along_x else .38,.55),M['ashlar'],.015,rect=kit.rng.choice(STONES))
PB=2.88   # plate beam centre
for y,o in ((-D2,-1),(D2,1)):
    yy=y+o*.06
    for x in (-(W2-.11),-1.7,1.7,W2-.11):
        base(x,y+o*.08,True);post(x,yy,.55,PB-.1,True)
        for d in (-1,1):
            if abs(x+d*.55)<W2:rail((x,yy-o*.01,2.2),(x+d*.55,yy-o*.01,PB-.1),.12)
    G['timber'].box((0,y+o*.1,PB),(2*W2+.2,.25,.22),M['timber'],.015,grain=True,band=band())
for s in (-1,1):
    x=s*(W2+.06)
    for yy in (-(D2-.11),0,D2-.11):
        if abs(yy)>1:continue      # corners carry the front/rear posts
        base(s*(W2+.08),yy,False);post(x,yy,.55,PB-.1,False)
        for d in (-1,1):rail((x-s*.01,yy,2.2),(x-s*.01,yy+d*.55,PB-.1),.12)
    G['timber'].box((s*(W2+.1),0,PB),(.25,2*D2+.2,.22),M['timber'],.015,grain=True,band=band())
# gable frame: sill, posts, loft-door frame, braces, collar and king post
y=-D2-.03
G['timber'].box((0,y-.04,3.1),(2*GW+.08,.22,.2),M['timber'],.015,grain=True,band=band())
for x in (-GW+.08,GW-.08):post(x,y,3.2,gable_z(GW-.08)-.05,True)
for x in (-OH-.06,OH+.06):post(x,y,3.2,OT+.06,True)
rail((-GW,y,OB-.06),(-OH,y,OB-.06),.12);rail((OH,y,OB-.06),(GW,y,OB-.06),.12)
G['timber'].box((0,y-.02,OB-.04),(2*OH+.24,.26,.1),M['timber'],.01,grain=True,band=band())   # loft sill
zc=OT+.12;rail((-GW+.02,y,zc),(GW-.02,y,zc),.16);post(0,y,zc+.08,RIDGE-.08,True)
for s in (-1,1):
    rail((s*(GW-.1),y,3.25),(s*(OH+.14),y,OT-.05),.1)
    rail((s*(min(GW,gx(zc+.1))-.15),y,zc+.1),(s*.09,y,zc+.75),.1)
G['detail'].box((0,-D2+.33,(OB+OT)/2),(2*OH+.12,.05,OT-OB+.12),M['door'],.004)   # loft back boards
for s in (-1,1):
    for z in (3.55,4.25):G['detail'].box((s*(OH+.06),y-.09,z),(.3,.014,.05),M['iron'],.004)   # loft door strap hinges
# hoist beam, pulley and chain
G['timber'].box((0,-D2-.33,4.95),(.16,1.0,.18),M['timber'],.012,grain=True,band=band())
pul=[(0,-D2-.68+.11*math.cos(t),4.72+.11*math.sin(t)) for t in np.linspace(0,math.tau,13)[:-1]]
G['detail'].prism(pul,(1,0,0),.04,M['iron'])
for s in (-1,1):G['detail'].box((s*.04,-D2-.68,4.8),(.012,.03,.2),M['iron'],0)
G['detail'].beam((0,-D2-.79,4.72),(0,-D2-.79,3.95),.018,.018,M['iron'],grain=False)
G['detail'].box((0,-D2-.79,3.9),(.03,.03,.1),M['iron'],.004)
# ---- openings and lanterns ------------------------------------------------------------
for x in (-3.0,3.0):window(G,M,(x,D2,WZ),(0,1,0),WIN,WH,shutters=False,band=band())
for s in (-1,1):window(G,M,(s*W2,s*1.4,WZ),(s,0,0),WIN,WH,shutters=False,band=band())
for x in (-4.5,4.5):lantern(G,M,(x,-D2-.42,2.05),arm=((x,-D2-.05,2.05),(0,-1,0)))
for x in (-1.7,1.7):lantern(G,M,(x,-D2-.5,2.05),arm=((x,-D2-.14,2.05),(0,-1,0)))
lantern(G,M,(0,-D2-.18,2.72))
# ---- export, preview scene, manifest ---------------------------------------------
# no wall top may rise through the hip roof (gable face, cheeks and stack are meant to)
for k in ('stone','plaster'):
    for p in G[k].v:
        if abs(p.x)>EX or abs(p.y)>EY or (abs(p.x-XC)<.5 and abs(p.y-YC)<.45) or (abs(p.x)<=GW+.06 and p.y<0 and p.z<gable_z(p.x)):continue
        assert p.z<=main_z(p.x,p.y)+1e-3,f'{k} vertex {tuple(round(c,3) for c in p)} pokes through the roof ({main_z(p.x,p.y):.3f})'
objs=[g.object() for g in G.values()]
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert lo[2]>-1e-3,f'geometry below ground: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookStables'
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
cam('Beauty',(-12.5,-15.5,5.0),(80,0,-38),lens=38);cam('Front',(0,-30,3.6),(90,0,0),11.6);cam('Side',(-30,0,3.6),(90,0,-90),11.6);cam('Rear',(0,30,3.6),(90,0,180),11.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Stables-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120000,f'{tris} triangles exceeds the 120k house budget'
manifest=dict(asset='SilverbrookStables',reference=dict(file='Stables-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_stables.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookStables',slot_m=list(SLOT),outputs=['SilverbrookStables.blend','SilverbrookStables.glb','SilverbrookStables.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('STABLES_BUILT',manifest['bounds_m'],tris)
