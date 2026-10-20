"""Silverbrook Townhouse 1 from docs/Game-Design/concept-art/Silverbrook/Townhouse-1.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhouse1.py
Original procedural geometry interpreting the elevation (6 m wide walls, drawn 10 m tall,
compressed to the 9 m slot): pale squared-stone ground storey with an arched door up two
steps and a slated canted bay, jettied half-timbered upper storey on carved corbels, a
steep front cross-gable with finial, steep hipped slate ranges with a dormer and a stack,
a bracket banner with a gilded stag, lanterns, gutters and downpipes.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,door,lantern,frame
import bpy
import numpy as np
scene=kit.reset(4121)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Townhouse-1.png'
asset=bpy.data.collections.new('SilverbrookTownhouse1');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Townhouse1_Ashlar','sandstone_blocks_05',dark=(.45,.40,.33)),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_Townhouse1_Banner','denim_fabric_03',dark=(.10,.12,.28),damp=False,aged=.5),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_Townhouse1_Gilt',(.80,.55,.18),.35,1.0))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('sandstone_blocks_05',(.05,.035),dark=.7,frac=.01,std=.22)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

G=dict(stone=Geo('SM_SBTownhouse1_Masonry',asset),plaster=Geo('SM_SBTownhouse1_Plaster',asset),timber=Geo('SM_SBTownhouse1_Timber',asset),roof=Geo('SM_SBTownhouse1_Roof',asset),detail=Geo('SM_SBTownhouse1_Details',asset))
# ---- measured layout (metres; elevation read at ~100 px/m across, storeys compressed to fit 9 m) ----
SLOT=(8,7,9)                   # town-layout slot w x d x h (concept README 'Fits')
W2,D2=2.85,2.9                 # ground-storey outer wall half-extents
JZ,FL,JET=3.3,3.55,.4          # jetty bressumer underside, upper floor level, front projection
YF=-D2-JET                     # jettied upper front face
EAVE,RIDGE=5.75,8.3            # every eave and both ridges
RANGES=(-1.9,1.5);RH=1.7       # double-pile hipped ranges right of the wing: ridge y, half depth (eave to ridge)
KF=(RIDGE-EAVE)/RH;EX,KS=3.15,2.5;RX=EX-(RIDGE-EAVE)/KS   # steep right hips
XG,GW,GH=-.875,1.975,2.2       # front cross wing (full depth): centre x, wall half, roof half
KG=(RIDGE-EAVE)/GH;GY,GB=YF-.25,D2+.3
XD,DH,DW,DE,DR=1.75,.45,.36,7.42,8.0   # dormer on the front range
KD=(DR-DE)/DH;YD=RANGES[0]-(RIDGE-6.7)/KF
XC,YC=2.2,1.5                  # stack on the rear ridge
def range_z(x,y):
    z=-9
    for yc in RANGES:
        if abs(y-yc)<=RH+1e-6 and x>XG:z=max(z,min(RIDGE-abs(y-yc)*KF,RIDGE-max(0,x-RX)*KS))
    return z
def wing_z(x):return RIDGE-abs(x-XG)*KG
def dormer_z(x):return DR-abs(x-XD)*KD
in_wing=lambda p:abs(p.x-XG)<GH and GY-.05<p.y<GB+.05 and p.z<wing_z(p.x)-.005
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<RANGES[0] and p.z<dormer_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.45 and abs(p.y-YC)<.4
under_range=lambda p:p.z<range_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.4+.3*max(0,1-(p.z-EAVE)/2) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered,geo='roof'):
    u=Vector(toward).normalized();tile_plane(G[geo],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8))
    deck(G[geo],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
def post(x,y,z0,z1,o,s=.16):G['timber'].box((x,y,(z0+z1)/2),(s if o=='y' else .12,.12 if o=='y' else s,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.15):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
def fill(geo,pts,axis,th,mat):
    """Flat slab of thickness th around a planar outline. Newell normal orients it; the caps are
    fanned from pts[0], so a concave outline needs pts[0] to see every other point (spandrels)."""
    pts=[Vector(p) for p in pts];axis=Vector(axis).normalized()
    if sum((a.cross(pts[(i+1)%len(pts)]) for i,a in enumerate(pts)),Vector()).dot(axis)<0:pts.reverse()
    o=axis*th/2;n=len(pts)
    for i in range(1,n-1):geo.poly([pts[0]+o,pts[i]+o,pts[i+1]+o],mat);geo.poly([pts[0]-o,pts[i+1]-o,pts[i]-o],mat)
    for i,a in enumerate(pts):b=pts[(i+1)%n];geo.poly([a-o,b-o,b+o,a+o],mat)
def glazing(c,o,w,h,lights=2,th=.3,sill=True,planter=False):
    """Leaded lights in an oak frame (kit.window without shutters, any number of lights).
    c: bottom centre of the glazed area on the outer face; th: depth of the frame's wall."""
    P,R=frame(c,o);t=.09;fd=th+.04;gd=-min(.14,th/2-.01)
    def B(geo,a,d,z,size,mat,bevel=.008,**kw):geo.box(P(a,d,z),size,mat,bevel,R,**kw)
    for a in (-(w/2+t/2),w/2+t/2):B(G['timber'],a,-th/2+.02,h/2,(t,fd,h+2*t),M['timber'],grain=True,band=band())
    for z in (h+t/2,-t/2):B(G['timber'],0,-th/2+.02,z,(w,fd,t),M['timber'],grain=True,band=band())
    pw=(w-.06*(lights-1))/lights
    for i in range(lights):
        ac=-w/2+pw/2+i*(pw+.06)
        if i:B(G['timber'],ac-pw/2-.03,gd+.05,h/2,(.06,.1,h),M['timber'],grain=True,band=band())
        B(G['detail'],ac,gd,h/2,(pw,.01,h),M['glass'],0)
        lo,hi=ac-pw/2,ac+pw/2;slope=1.7;step=.10*slope
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
        B(G['timber'],0,.17,-t-.25,(w+2*t+.1,.24,.2),M['planks'],.006)
        B(G['detail'],0,.17,-t-.145,(w+2*t+.04,.19,.02),M['soil'],0)
        for a in (-w/2,w/2):G['timber'].beam(P(a,.03,-t-.6),P(a,.25,-t-.35),.06,.06,M['timber'],band=band())
def hole(a,w,z0,h):return (a,w+.18,z0-.09,z0+h+.09)
# ---- front cross wing roof (full depth) ------------------------------------------------
LG=math.hypot(GH,RIDGE-EAVE)
plane((XG-GH,0,EAVE),(0,-1,0),(GH,0,RIDGE-EAVE),LG,lambda t:-GB,lambda t:-GY,[in_stack])
plane((XG+GH,0,EAVE),(0,1,0),(-GH,0,RIDGE-EAVE),LG,lambda t:GY,lambda t:GB,[under_range,in_stack])
capped((XG,GY,RIDGE+.05),(XG,GB,RIDGE+.05),26)
for y in (GY-.04,GB+.04):
    for s in (-1,1):G['timber'].beam((XG+s*(GH+.05),y,EAVE-.08),(XG-s*.16,y,RIDGE+.16*KG+.03),.08,.26,M['timber'],band=band())
G['timber'].box((XG,GY-.06,RIDGE+.15),(.16,.16,.8),M['timber'],.02,grain=True,band=band())     # finial
G['timber'].box((XG,GY-.06,RIDGE+.57),(.22,.22,.05),M['timber'],.02,grain=True,band=band())
G['detail'].beam((XG,GY-.06,RIDGE+.59),(XG,GY-.06,8.97),.05,.05,M['iron'],grain=False)
G['detail'].box((XG,GY-.06,RIDGE+.63),(.1,.1,.08),M['iron'],.01)
# ---- double-pile hipped ranges right of the wing --------------------------------------
LF,LS=math.hypot(RH,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
for i,yc in enumerate(RANGES):
    plane((0,yc-RH,EAVE),(1,0,0),(0,RH,RIDGE-EAVE),LF,lambda t:XG,lerp(EX,RX,LF),[in_wing,in_stack]+([in_dormer] if i==0 else []))
    plane((0,yc+RH,EAVE),(-1,0,0),(0,-RH,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lambda t:-XG,[in_wing,in_stack])
    plane((EX,yc,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-RH,0,LS),lerp(RH,0,LS),[in_stack])
    capped((XG+.3,yc,RIDGE+.05),(RX,yc,RIDGE+.05),9)
    for sy in (-1,1):capped((EX,yc+sy*RH,EAVE+.05),(RX,yc,RIDGE+.05),9)
for y in (RANGES[0]-RH-.02,RANGES[1]+RH+.02):G['timber'].beam((XG+GH,y,EAVE-.06),(EX,y,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((EX+.02,RANGES[0]-RH,EAVE-.06),(EX+.02,RANGES[1]+RH,EAVE-.06),.04,.2,M['timber'],band=band())
G['timber'].beam((XG-GH-.02,GY,EAVE-.06),(XG-GH-.02,GB,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- dormer ------------------------------------------------------------------------------
LD=math.hypot(DH,DR-DE)
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:-RANGES[0],lambda t:-YD+.16,[under_range])
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:RANGES[0],[under_range])
capped((XD,YD-.16,DR+.04),(XD,RANGES[0]-.3,DR+.04),4)
for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.18,M['timber'],band=band())
DB=range_z(XD,YD)-.1;DT=dormer_z(XD+DW)-.04
wall(G['plaster'],M['plaster'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[hole(0,.3,.15,.5)],th=.12)
fill(G['plaster'],[(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,DT),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,DT)],(0,1,0),.12,M['plaster'])
ym=RANGES[0]-(RIDGE-DT)/KF
for s in (-1,1):fill(G['plaster'],[(XD+s*DW,YD,DB),(XD+s*DW,YD,DT),(XD+s*DW,ym,DT)],(1,0,0),.08,M['plaster'])
for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.1,.1,DE-DB),M['timber'],.01,grain=True,band=band())
G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.12),M['timber'],.01,grain=True,band=band())
glazing((XD,YD,DB+.15),(0,-1,0),.3,.5,1,th=.12)
# ---- stack -------------------------------------------------------------------------------
G['stone'].box((XC,YC,7.52),(.8,.7,1.84),M['stone'],.01)
G['stone'].box((XC,YC,8.49),(.88,.78,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,8.6),(.94,.84,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.19,.19):G['detail'].vessel((XC+dx,YC,8.66),[(.1,0),(.095,.22),(.105,.27),(.12,.31),(.09,.31),(.075,.26),(0,.26)],M['pot'])
# ---- ground storey (squared stone) -------------------------------------------------------
STEP,DS,DOOR,XDOOR=.3,1.55,.9,-1.4   # door sill = top step, arch spring above it
WZ,WH,WW=1.0,1.2,.7
wall(G['stone'],M['stone'],(0,-D2,0),(0,-1,0),W2,JZ,[(XDOOR,DOOR,STEP,STEP+DS+DOOR/2)])
P,_=frame((XDOOR,-D2,STEP+DS),(0,-1,0));rad=DOOR/2        # spandrels over the round head
for s in (-1,1):fill(G['stone'],[P(s*rad,-.15,rad)]+[P(s*rad*math.cos(t),-.15,rad*math.sin(t)) for t in np.linspace(0,math.pi/2,9)],(0,1,0),.3,M['stone'])
wall(G['stone'],M['stone'],(0,D2,0),(0,1,0),W2,JZ,[hole(1.6,WW,WZ,WH),hole(-1.6,WW,WZ,WH)])
wall(G['stone'],M['stone'],(W2,0,0),(1,0,0),D2-.3,JZ,[hole(1.0,WW,WZ,WH)])
wall(G['stone'],M['stone'],(-W2,0,0),(-1,0,0),D2-.3,JZ,[hole(-1.0,WW,WZ,WH)])
G['stone'].box((0,D2+.02,.2),(2*W2+.08,.1,.4),M['stone'],.01)          # plinth, sides and rear
for s in (-1,1):G['stone'].box((s*(W2+.02),0,.2),(.1,2*D2,.4),M['stone'],.01)
for k in range(2):G['stone'].box((XDOOR,-D2-.17-.34*k,(2-k)*.075),(1.3+.2*k,.34,(2-k)*.15),M['stone'],.015)
door(G,M,(XDOOR,-D2,STEP),(0,-1,0),DOOR,DS,STONES,band=band())
for x,y,o in ((1.6,D2,(0,1,0)),(-1.6,D2,(0,1,0)),(W2,1.0,(1,0,0)),(-W2,1.0,(-1,0,0))):glazing((x,y,WZ),o,WW,WH)
lantern(G,M,(-2.42,-D2-.42,2.55),arm=((-2.42,-D2-.09,2.55),(0,-1,0)))
# ---- canted bay window -------------------------------------------------------------------
A0,A1,BP,Y0=-.32,2.55,.55,-D2
BAY=[Vector(p) for p in ((A0,Y0),(A0+BP,Y0-BP),(A1-BP,Y0-BP),(A1,Y0))]
def offset(pts,ds):
    """Offset an open outline (walls at both ends) outward by ds per edge; ends stay on the wall line."""
    out=[];lines=[]
    for (a,b),d in zip(zip(pts,pts[1:]),ds):
        e=(b-a).normalized();n=Vector((e.y,-e.x));lines.append((a+n*d,e))
    def meet(l1,l2):
        (p,e),(q,f)=l1,l2;den=e.x*f.y-e.y*f.x;t=((q-p).x*f.y-(q-p).y*f.x)/den;return p+e*t
    wall_line=(Vector((0,Y0+.02)),Vector((1,0)))
    return [meet(wall_line,lines[0])]+[meet(lines[i],lines[i+1]) for i in range(len(lines)-1)]+[meet(lines[-1],wall_line)]
def slab(geo,pts,z0,z1,mat):fill(geo,[(p.x,p.y,(z0+z1)/2) for p in pts],(0,0,1),z1-z0,mat)
slab(G['stone'],offset(BAY,[.04]*3),0,.22,M['stone'])
slab(G['timber'],offset(BAY,[0]*3),.22,.95,M['door'])
slab(G['timber'],offset(BAY,[.05]*3),.95,1.03,M['timber'])
slab(G['timber'],offset(BAY,[.06]*3),2.55,2.70,M['timber'])
for i,(a,b) in enumerate(zip(BAY,BAY[1:])):
    e=(b-a);L=e.length;e=e.normalized();o=Vector((e.y,-e.x,0));mid=(a+b)/2
    lights=2 if i==1 else 1;glazing((mid.x,mid.y,1.12),o,L-.2,1.34,lights,th=.12,sill=False)
    Pf,Rf=frame((mid.x,mid.y,0),o);n=3 if i==1 else 1;pw=(L-.1)/n   # carved oak panels: stiles, rails, lancet tracery
    for z in (.27,.9):G['timber'].box(Pf(0,.015,z),(L-.04,.03,.06),M['timber'],.006,Rf,grain=True,band=band())
    for k in range(n+1):G['timber'].box(Pf(-L/2+.05+k*pw,.015,.585),(.06,.03,.6),M['timber'],.006,Rf,grain=True,band=band())
    for k in range(n):
        xc=-L/2+.05+(k+.5)*pw;bw=pw/2-.07;sp=.62
        for s in (-1,1):
            G['timber'].beam(Pf(xc+s*bw,.02,.33),Pf(xc+s*bw,.02,sp),.025,.03,M['timber'],grain=False)
            arc=[Pf(xc-s*bw+s*2*bw*math.cos(t),.02,sp+2*bw*math.sin(t)) for t in np.linspace(0,math.pi/3,5)]
            for p0,p1 in zip(arc,arc[1:]):G['timber'].beam(p0,p1,.025,.03,M['timber'],grain=False)
for p in BAY[1:3]:G['timber'].box((p.x,p.y,1.79),(.13,.13,1.52),M['timber'],.01,grain=True,band=band())
EB=offset(BAY,[.1]*3);BRISE,BRUN=.5,BP+.1;pitch=math.atan2(BRISE,BRUN)
def corner_k(a,b,c):                     # how far a plane edge slides along its eave per unit inward run
    u,v=(a-b).normalized(),(c-b).normalized();ang=math.acos(max(-1,min(1,u.dot(v))));return ang
for i,(a,b) in enumerate(zip(EB,EB[1:])):
    e=(b-a);L=e.length;e3=Vector((e.x,e.y,0)).normalized();h=Vector((-e3.y,e3.x,0))
    ka=1/math.tan(corner_k(EB[i-1],a,b)/2) if i>0 else 1/math.tan(corner_k(Vector((a.x+1,a.y)),a,b))
    kb=1/math.tan(corner_k(a,b,EB[i+2])/2) if i<2 else 1/math.tan(corner_k(a,b,Vector((b.x-1,b.y))))
    r_end=BRUN if i==1 else L/(ka+kb);Ls=r_end/math.cos(pitch);c=math.cos(pitch)
    tile_plane(G['roof'],(a.x,a.y,2.7),e3,h*math.cos(pitch)+Vector((0,0,math.sin(pitch))),Ls,(lambda t,ka=ka:ka*t*c),(lambda t,kb=kb,L=L:L-kb*t*c),M['slate'],SLATES,lichen=lichen,tone=(.35,.8))
    deck(G['roof'],(a.x,a.y,2.7),e3,h*math.cos(pitch)+Vector((0,0,math.sin(pitch))),Ls,(lambda t,ka=ka:ka*t*c),(lambda t,kb=kb,L=L:L-kb*t*c),M['timber'])
for p,s in ((EB[1],1),(EB[2],-1)):
    top=Vector((p.x+s*BRUN*math.tan(math.radians(22.5)),Y0,2.7+BRISE));cap(Vector((p.x,p.y,2.74)),top+Vector((0,0,.04)),.16)
# ---- jetty: bressumer and carved corbels ---------------------------------------------------
G['timber'].box((0,-D2-.22,(JZ+FL)/2),(2*W2+.1,.52,FL-JZ),M['timber'],.015,grain=True,band=band())
for s in (-1,1):G['timber'].box((s*(W2-.15),-.125,(JZ+FL)/2),(.3,2*D2-.45,FL-JZ),M['timber'],.01,grain=True,band=band())
G['timber'].box((0,D2-.15,(JZ+FL)/2),(2*W2,.3,FL-JZ),M['timber'],.01,grain=True,band=band())
for x,low in ((-2.79,2.55),(-2.0,2.95),(-.58,2.95),(2.79,2.55)):
    fill(G['timber'],[(x,-D2+.01,JZ),(x,YF+.05,JZ),(x,YF+.05,JZ-.12),(x,-D2+.01,low)],(1,0,0),.14,M['timber'])
# ---- upper storey: plaster panels in an oak frame -----------------------------------------
GC=wing_z(XG+GW)-.05;PLATE=range_z(2,YF)-.04
UZ,UH=4.4,1.15                 # first-floor window sill and height
wall(G['plaster'],M['plaster'],(XG,YF,FL),(0,-1,0),GW,GC-FL,[hole(-.65-XG,1.82,UZ-FL,UH)])
XR=(XG+GW+W2)/2;HR=(W2-XG-GW)/2
wall(G['plaster'],M['plaster'],(XR,YF,FL),(0,-1,0),HR,PLATE-FL,[hole(1.67-XR,.55,UZ-FL,UH)])
wall(G['plaster'],M['plaster'],(XG,D2,FL),(0,1,0),GW,GC-FL,[hole(-(-1.7-XG),.6,UZ-FL,1.0)])
wall(G['plaster'],M['plaster'],(XR,D2,FL),(0,1,0),HR,PLATE-FL,[hole(-(1.95-XR),.6,UZ-FL,1.0)])
YS0,YS1=YF+.3,D2-.3;YSC=(YS0+YS1)/2;SIDE_TOP=5.7
wall(G['plaster'],M['plaster'],(W2,YSC,FL),(1,0,0),(YS1-YS0)/2,SIDE_TOP-FL,[hole(-1.5-YSC,.6,UZ-FL,1.0)])
wall(G['plaster'],M['plaster'],(-W2,YSC,FL),(-1,0,0),(YS1-YS0)/2,GC-FL,[hole(-(1.0-YSC),.6,UZ-FL,1.0)])
for yc in RANGES:              # right side wall up to the roof, one convex slab per range
    y0,y1=max(yc-RH,YF),min(yc+RH,D2)
    top=[(y,range_z(W2,y)-.06) for y in np.linspace(y0,y1,31)];top=[(y,z) for y,z in top if z>SIDE_TOP+.01]
    fill(G['plaster'],[(W2-.15,y,z) for y,z in [(top[-1][0],SIDE_TOP),(top[0][0],SIDE_TOP)]+top],(1,0,0),.28,M['plaster'])
def gx(z):return (RIDGE-.05-z)/KG
OB,OT,OH=6.46-.09,6.46+.8+.09,(.9+.18)/2   # front gable window aperture
for piece in ([(-GW,GC),(GW,GC),(gx(OB),OB),(-gx(OB),OB)],[(-gx(OB),OB),(-OH,OB),(-OH,OT),(-gx(OT),OT)],
              [(OH,OB),(gx(OB),OB),(gx(OT),OT),(OH,OT)],[(-gx(OT),OT),(gx(OT),OT),(0,RIDGE-.05)]):
    fill(G['plaster'],[(XG+x,YF+.15,z) for x,z in piece],(0,1,0),.3,M['plaster'])
fill(G['plaster'],[(XG-GW,D2-.15,GC),(XG+GW,D2-.15,GC),(XG,D2-.15,RIDGE-.05)],(0,1,0),.3,M['plaster'])
# front framing
y=YF-.03
for x in (-2.79,-1.72,.42,XG+GW,2.1,2.79):post(x,y,FL,GC if x<=XG+GW+.01 else PLATE,'y')
rail((-W2,y,FL+.07),(W2,y,FL+.07),.14)
for a,b in ((-W2,-1.72),(.42,W2)):rail((a,y,UZ-.2),(b,y,UZ-.2),.12)
rail((-W2,y,UZ+UH+.16),(W2,y,UZ+UH+.16),.12)
rail((XG-GW,y,GC-.07),(XG+GW,y,GC-.07),.16);rail((XG+GW,y,PLATE-.08),(W2,y,PLATE-.08),.16)
rail((-2.72,y,UZ-.1),(-1.8,y,UZ+UH+.08),.12);rail((.5,y,UZ-.1),(XG+GW-.08,y,UZ+UH+.08),.12);rail((2.17,y,FL+.15),(2.72,y,UZ-.27),.1)
# gable: studs beside the window, collar, king post and struts
for s in (-1,1):post(XG+s*(OH+.08),y,GC,OT+.02,'y',.12);rail((XG+s*(GW-.25),y,GC+.05),(XG+s*(OH+.2),y,OB-.02),.1)
zc=OT+.1;rail((XG-gx(zc)+.05,y,zc),(XG+gx(zc)-.05,y,zc),.14);post(XG,y,zc+.07,RIDGE-.1,'y',.14)
# sides and rear
for s in (-1,1):
    x=s*(W2+.03)
    top=GC if s<0 else SIDE_TOP
    for yy in (YF+.08,-.2,D2-.08):post(x,yy,FL,top-.02,'x')
    rail((x,YF,FL+.07),(x,D2,FL+.07),.14);rail((x,YF,top-.07),(x,D2,top-.07),.14);rail((x,YF,UZ-.2),(x,D2,UZ-.2),.12)
y=D2+.03
for x in (-2.79,XG+GW,2.79):post(x,y,FL,GC if x<XG+GW+.01 else PLATE,'y')
rail((-W2,y,FL+.07),(W2,y,FL+.07),.14);rail((-W2,y,UZ-.2),(W2,y,UZ-.2),.12)
rail((XG-GW,y,GC-.07),(XG+GW,y,GC-.07),.16);rail((XG+GW,y,PLATE-.08),(W2,y,PLATE-.08),.16);post(XG,y,GC,RIDGE-.1,'y',.14)
# ---- upper windows ---------------------------------------------------------------------
glazing((-.65,YF,UZ),(0,-1,0),1.82,UH,3,planter=True)
glazing((1.67,YF,UZ),(0,-1,0),.55,UH,1)
glazing((XG,YF,6.46),(0,-1,0),.9,.8,2)
for x in (-1.7,1.95):glazing((x,D2,UZ),(0,1,0),.6,1.0)
glazing((W2,-1.5,UZ),(1,0,0),.6,1.0);glazing((-W2,1.0,UZ),(-1,0,0),.6,1.0)
# ---- gutters and downpipes -------------------------------------------------------------
def pipe(pts):
    for a,b in zip(pts,pts[1:]):G['detail'].beam(a,b,.07,.07,M['iron'],grain=False)
G['detail'].box((EX+.08,(RANGES[0]-RH+RANGES[1]+RH)/2,EAVE-.12),(.1,2*RH+3.4,.1),M['iron'],.01)
G['detail'].box((XG-GH-.08,(GY+GB)/2,EAVE-.12),(.1,GB-GY,.1),M['iron'],.01)
for s,yy in ((1,-2.75),(-1,-2.62)):
    xg=EX+.08 if s>0 else XG-GH-.08
    pipe([(xg,yy,EAVE-.12),(xg,yy,5.3),(s*(W2+.08),yy,4.95),(s*(W2+.08),yy,.05)])
    for z in (1.0,2.6,4.6):G['detail'].box((s*(W2+.03),yy,z),(.1,.05,.05),M['iron'],.004)
# ---- bracket banner on the left wall: navy linen with a gilded stag ---------------------------
def stag(geo,origin,right,up,normal,s):
    o=Vector(origin)+Vector(normal)*.008;R,U,N=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+R*(a*s)+U*(z*s)
    fill(geo,[p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,M['gilt'])
    for sx in (-1,1):
        fill(geo,[p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
def trim(geo,pts,normal,w=.04):
    n=Vector(normal)*.008
    for a,b in zip(pts,pts[1:]+pts[:1]):geo.beam(Vector(a)+n,Vector(b)+n,.02,w,M['gilt'],grain=False)
AY,AZ,AX=-2.15,3.05,-W2-.95
G['detail'].beam((-W2+.05,AY,AZ),(AX,AY,AZ),.05,.05,M['iron'],grain=False)
G['detail'].beam((-W2+.05,AY,AZ-.7),(-W2-.65,AY,AZ-.02),.035,.035,M['iron'],grain=False)
G['detail'].box((-W2-.03,AY,AZ-.35),(.06,.1,.9),M['iron'],.004)
for k in range(8):
    t0,t1=math.tau*k/8,math.tau*(k+1)/8;c=Vector((-W2-.35,AY,AZ-.18));r=.13
    G['detail'].beam(c+Vector((r*math.cos(t0),0,r*math.sin(t0))),c+Vector((r*math.cos(t1),0,r*math.sin(t1))),.02,.02,M['iron'],grain=False)
bxc=AX+.32;bb=[(bxc-.27,AZ-.1),(bxc+.27,AZ-.1),(bxc+.27,AZ-1.0),(bxc,AZ-1.22),(bxc-.27,AZ-1.0)]
fill(G['detail'],[(x,AY,z) for x,z in bb],(0,1,0),.02,M['banner'])
for x in (bxc-.22,bxc+.22):G['detail'].beam((x,AY,AZ-.03),(x,AY,AZ-.1),.015,.015,M['iron'],grain=False)
for n in (-1,1):
    trim(G['detail'],[(x,AY+n*.01,z) for x,z in bb],(0,n,0))
    stag(G['detail'],(bxc,AY+n*.01,AZ-.68),(-n,0,0),(0,0,1),(0,n,0),.62)
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
from mathutils import Matrix
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'not grounded: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookTownhouse1'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
DOORS=kit.split_doors(str(OUT),'SM_SilverbrookTownhouse1')  # door leaves: own meshes, hinged
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-10.6,-14,4.4),(84,0,-37),lens=38);cam('Front',(0,-30,4.5),(90,0,0),9.6);cam('Side',(-30,0,4.5),(90,0,-90),9.6);cam('Rear',(0,30,4.5),(90,0,180),9.6)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Townhouse1-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120_000,f'house triangle budget: {tris}'
manifest=dict(asset='SilverbrookTownhouse1',asset_id='BLD-GreenlandsTownhouse-01',variant=1,reference=dict(file='Townhouse1-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_townhouse1.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookTownhouse1',parts=DOORS,slot_m=list(SLOT),outputs=['SilverbrookTownhouse1.blend','SilverbrookTownhouse1.glb','SilverbrookTownhouse1.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('TOWNHOUSE1_BUILT',manifest['bounds_m'],tris)
