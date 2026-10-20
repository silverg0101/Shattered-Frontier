"""Silverbrook Townhouse 6 from docs/Game-Design/concept-art/Silverbrook/Townhouse-6.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_townhouse6.py
Original procedural geometry interpreting the elevation (6 m wide, ~11 m to the finial as drawn,
compressed to the 9 m slot): narrow two-storey townhouse, ashlar ground storey with a canted
bay window under a slate pent roof and an arched door up two steps; jettied half-timbered upper
storey on a bressumer and corbels; a tall front cross gable (above the main ridge) carrying a
canted oriel; steep-hipped slate roof with a right dormer, left stack and a stag banner bracket.
"""
import sys,math,json,hashlib,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import silverbrook_kit as kit
from silverbrook_kit import Geo,Vector,tile_plane,deck,wall,window,door,lantern
import bpy
import numpy as np
scene=kit.reset(4126)
REF=HERE.parents[3]/'docs/Game-Design/concept-art/Silverbrook/Townhouse-6.png'
asset=bpy.data.collections.new('SilverbrookTownhouse6');scene.collection.children.link(asset)

M=dict(
    slate=kit.scan('SB_Slate','grey_roof_tiles_02',dark=(.20,.22,.28),lichen='lichen_rock',damp=False),
    stone=kit.scan('SB_Townhouse6_Ashlar','sandstone_blocks_05',dark=(.42,.38,.31)),
    plaster=kit.scan('SB_LimePlaster','patterned_clay_plaster'),
    timber=kit.scan('SB_OakTimber','weathered_brown_planks',dark=(.32,.30,.28),aged=.6),
    door=kit.scan('SB_DoorBoards','old_planks_02',aged=.15),
    banner=kit.scan('SB_Townhouse6_Banner','denim_fabric_03',dark=(.12,.14,.30),damp=False,aged=.75),
    iron=kit.solid('SB_ForgedIron',(.03,.026,.022),.6,.9),
    lead=kit.solid('SB_LeadCame',(.05,.05,.05),.45,.8),
    glass=kit.solid('SB_LitGlass',(.02,.02,.02),.15,0,(1.0,.5,.14),3),
    lamp=kit.solid('SB_LanternGlow',(.3,.2,.1),.3,0,(1.0,.38,.08),5),
    soil=kit.solid('SB_PlanterSoil',(.06,.04,.025),.95),
    pot=kit.solid('SB_ChimneyPot',(.20,.085,.05),.75),
    gilt=kit.solid('SB_Townhouse6_Gilt',(.80,.55,.18),.35,1.0))
M['ashlar']=M['stone'];M['planks']=M['timber']
SLATES=kit.patches('grey_roof_tiles_02',(.07,.045))
STONES=kit.patches('sandstone_blocks_05',(.05,.035),dark=.7,frac=.01,std=.22)
BANDS=kit.bands('weathered_brown_planks')
def band():return kit.rng.choice(BANDS)

class Geo6(Geo):
    """Geo whose prisms (wall arch spandrels, door leaf, gables) must come out with positive volume."""
    def prism(self,points,axis,thickness,mat,**kw):
        n=len(self.f);super().prism(points,axis,thickness,mat,**kw);vol=0
        for f in self.f[n:]:
            p=[self.v[i] for i in f];vol+=sum(p[0].dot(p[j].cross(p[j+1])) for j in range(1,len(p)-1))
        assert vol>0,'inside-out prism'
G=dict(stone=Geo6('SM_SBTownhouse6_Masonry',asset),plaster=Geo6('SM_SBTownhouse6_Plaster',asset),timber=Geo6('SM_SBTownhouse6_Timber',asset),roof=Geo6('SM_SBTownhouse6_Roof',asset),detail=Geo6('SM_SBTownhouse6_Details',asset))
# ---- measured layout (metres; elevation read at 98 px/m, width x0.92 and heights ~x0.75 to fit 9 m) ----
SLOT=(8,7,9)                   # town-layout slot w x d x h (concept README 'Fits')
W2,D2=2.75,2.5                 # outer wall half width; upper storey / rear wall half depth
YG,YF=-2.15,-2.5               # ground-storey front face; jettied upper front face
JZ0,JZ=3.1,3.3                 # ground wall top under the bressumer; upper storey base
EAVE,RIDGE=5.6,7.8             # main hip roof
EX,EY,RX=W2+.3,D2+.3,2.2       # eave half-extents, ridge half-length (steep hips, as drawn)
KF,KS=(RIDGE-EAVE)/EY,(RIDGE-EAVE)/(EX-RX)
XG,GW,GH,GE,GR=-.6,1.7,2.0,EAVE,8.4   # front cross gable: centre x, wall half, roof half, eave, ridge (above main ridge)
KG=(GR-GE)/GH;GY,GB=YF-.35,D2+.35
XD,DH,DW,DE,DR=1.95,.5,.4,6.85,7.55   # dormer
YD=-(RIDGE-6.06)/KF;KD=(DR-DE)/DH
XC,YC=-2.15,1.2                # stack (left, behind the gable's left slope)
def main_z(x,y):return min(RIDGE-abs(y)*KF,RIDGE-max(0,abs(x)-RX)*KS)
def gable_z(x):return GR-abs(x-XG)*KG
def dormer_z(x):return DR-abs(x-XD)*KD
in_gable=lambda p:abs(p.x-XG)<GW+.04 and p.z<gable_z(p.x)-.005
in_dormer=lambda p:abs(p.x-XD)<DH and YD<p.y<0 and p.z<dormer_z(p.x)-.005
in_stack=lambda p:abs(p.x-XC)<.47 and abs(p.y-YC)<.42
under_main=lambda p:p.z<main_z(p.x,p.y)-.01
def lichen(p):return kit.rng.uniform(.5,1) if kit.rng.random()<.45+.3*max(0,1-(p.z-EAVE)/2) else 0
def lerp(a,b,L):return lambda t:a+(b-a)*t/L
def plane(O,e,toward,L,lo,hi,covered,**kw):
    u=Vector(toward).normalized();tile_plane(G['roof'],O,e,u,L,lo,hi,M['slate'],SLATES,covered,lichen=lichen,tone=(.35,.8),**kw)
    deck(G['roof'],O,e,u,L,lo,hi,M['timber'],covered)
def cap(a,b,w=.26):G['roof'].beam(a,b,w,.05,M['slate'],grain=False,rect=kit.rng.choice(SLATES),w=(kit.rng.uniform(.5,.8),.3))
def capped(a,b,n):
    a,b=Vector(a),Vector(b)
    for i in range(n):cap(a+(b-a)*i/n,a+(b-a)*(i+1.04)/n)
def post(x,y,z0,z1,o,s=.2):G['timber'].box((x,y,(z0+z1)/2),(s if o=='y' else .12,.12 if o=='y' else s,z1-z0),M['timber'],.012,grain=True,band=band())
def rail(a,b,s=.16):G['timber'].beam(a,b,.12,s,M['timber'],band=band())
# ---- main hip roof ----------------------------------------------------------------------
LF,LS=math.hypot(EY,RIDGE-EAVE),math.hypot(EX-RX,RIDGE-EAVE)
plane((0,-EY,EAVE),(1,0,0),(0,EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,in_dormer,in_stack])
plane((0,EY,EAVE),(-1,0,0),(0,-EY,RIDGE-EAVE),LF,lerp(-EX,-RX,LF),lerp(EX,RX,LF),[in_gable,in_stack])
plane((EX,0,EAVE),(0,1,0),(RX-EX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[])
plane((-EX,0,EAVE),(0,-1,0),(EX-RX,0,RIDGE-EAVE),LS,lerp(-EY,0,LS),lerp(EY,0,LS),[in_gable,in_stack])
GX=(GR-RIDGE)/KG               # main ridge disappears into the gable roof within GX of XG
capped((-RX,0,RIDGE+.05),(XG-GX,0,RIDGE+.05),5);capped((XG+GX,0,RIDGE+.05),(RX,0,RIDGE+.05),9)
for sx in (-1,1):
    for sy in (-1,1):capped((sx*EX,sy*EY,EAVE+.05),(sx*RX,0,RIDGE+.05),12)
G['timber'].box((RX,0,RIDGE+.35),(.14,.14,.6),M['timber'],.02,grain=True,band=band())   # right ridge-end finial
G['timber'].box((RX,0,RIDGE+.67),(.2,.2,.05),M['timber'],.02,grain=True,band=band())
for sy in (-1,1):
    for a,b in ((-EX,XG-GH),(XG+GH,EX)):G['timber'].beam((a,sy*(EY+.02),EAVE-.06),(b,sy*(EY+.02),EAVE-.06),.04,.2,M['timber'],band=band())
for sx in (-1,1):G['timber'].beam((sx*(EX+.02),-EY,EAVE-.06),(sx*(EX+.02),EY,EAVE-.06),.04,.2,M['timber'],band=band())
# ---- cross gable roof: runs front to back, taller than the main ridge ---------------------
LG=math.hypot(GH,GR-GE)
plane((XG-GH,0,GE),(0,-1,0),(GH,0,GR-GE),LG,lambda t:-GB,lambda t:-GY,[under_main,in_stack])
plane((XG+GH,0,GE),(0,1,0),(-GH,0,GR-GE),LG,lambda t:GY,lambda t:GB,[under_main,in_stack])
capped((XG,GY,GR+.05),(XG,GB,GR+.05),22)
for y in (GY,GB):
    yb=y-.04 if y<0 else y+.04
    for s in (-1,1):G['timber'].beam((XG+s*(GH+.05),yb,GE-.08),(XG-s*.16,yb,GR+.16*KG+.03),.08,.24,M['timber'],band=band())
G['timber'].box((XG,GY-.06,8.475),(.14,.14,.55),M['timber'],.02,grain=True,band=band())     # finial
G['timber'].box((XG,GY-.06,8.775),(.2,.2,.05),M['timber'],.02,grain=True,band=band())
G['detail'].beam((XG,GY-.06,8.8),(XG,GY-.06,8.97),.04,.04,M['iron'],grain=False)
G['detail'].box((XG,GY-.06,8.86),(.09,.09,.07),M['iron'],.01)
GC=gable_z(XG+GW)-.05          # gable wall height at its corners
for sy,y0 in ((-1,YF+.02),(1,D2-.02)):   # cheek walls close each gable face down to the main slope
    yq=sy*(RIDGE-GC)/KF
    for s in (-1,1):
        x=XG+s*GW;G['plaster'].prism([(x,y0,main_z(x,y0)-.04),(x,y0,GC),(x,yq,GC)],(1,0,0),.1,M['plaster'])
# ---- dormer ------------------------------------------------------------------------------
LD=math.hypot(DH,DR-DE)
plane((XD-DH,0,DE),(0,-1,0),(DH,0,DR-DE),LD,lambda t:.6,lambda t:-YD+.16,[under_main],size=(.22,.32,.02),exposure=.15)
plane((XD+DH,0,DE),(0,1,0),(-DH,0,DR-DE),LD,lambda t:YD-.16,lambda t:-.6,[under_main],size=(.22,.32,.02),exposure=.15)
capped((XD,YD-.16,DR+.04),(XD,-.8,DR+.04),5)
for s in (-1,1):G['timber'].beam((XD+s*(DH+.03),YD-.18,DE-.05),(XD-s*.1,YD-.18,DR+.1*KD+.02),.06,.16,M['timber'],band=band())
G['timber'].box((XD,YD-.2,DR+.2),(.08,.08,.45),M['timber'],.012,grain=True,band=band())
G['detail'].beam((XD,YD-.2,DR+.42),(XD,YD-.2,DR+.6),.03,.03,M['iron'],grain=False)
DB=main_z(XD,YD)-.1
wall(G['plaster'],M['plaster'],(XD,YD,DB),(0,-1,0),DW,DE+.02-DB,[(0,.63,.13,.76)],th=.12)
G['plaster'].prism([(XD-DW,YD+.06,DE+.02),(XD+DW,YD+.06,DE+.02),(XD+DW,YD+.06,dormer_z(XD+DW)-.04),(XD,YD+.06,DR-.04),(XD-DW,YD+.06,dormer_z(XD+DW)-.04)],(0,1,0),.12,M['plaster'])
ym=-(RIDGE-(dormer_z(XD+DW)-.04))/KF
for s in (-1,1):G['plaster'].prism([(XD+s*DW,YD,DB),(XD+s*DW,YD,dormer_z(XD+DW)-.04),(XD+s*DW,ym,dormer_z(XD+DW)-.04)],(1,0,0),.08,M['plaster'])
for s in (-1,1):G['timber'].box((XD+s*(DW-.05),YD-.02,(DB+DE)/2),(.1,.12,DE-DB),M['timber'],.01,grain=True,band=band())
G['timber'].box((XD,YD-.03,DE-.02),(2*DW+.08,.14,.12),M['timber'],.01,grain=True,band=band())
window(G,M,(XD,YD,DB+.22),(0,-1,0),.45,.45,shutters=False,wall_th=.12,band=band())
# ---- stack -------------------------------------------------------------------------------
G['stone'].box((XC,YC,6.9),(.8,.7,2.6),M['stone'],.01)
G['stone'].box((XC,YC,8.0),(.86,.76,.1),M['ashlar'],.015,rect=kit.rng.choice(STONES))
G['stone'].box((XC,YC,8.26),(.9,.8,.12),M['ashlar'],.015,rect=kit.rng.choice(STONES),w=(.45,0))
for dx in (-.18,.18):G['detail'].vessel((XC+dx,YC,8.32),[(.1,0),(.095,.4),(.105,.47),(.12,.52),(.095,.52),(.08,.46),(0,.46)],M['pot'])
# ---- canted bays (ground bay window and upper oriel) ------------------------------------------
def trap(cx,wy,fh,d,o=0):
    """Plan outline of a 45-degree canted bay on the wall line wy, offset outward by o: BL, FL, FR, BR."""
    return [Vector((cx-fh-d-1.414*o,wy,0)),Vector((cx-fh-.414*o,wy-d-o,0)),Vector((cx+fh+.414*o,wy-d-o,0)),Vector((cx+fh+d+1.414*o,wy,0))]
def slab(geo,cx,wy,fh,d,o,z0,z1,mat):
    pts=trap(cx,wy,fh,d,o);pts=[pts[0]+Vector((0,.05,0))]+pts+[pts[3]+Vector((0,.05,0))]   # tuck the back into the wall
    geo.prism([p+Vector((0,0,(z0+z1)/2)) for p in pts],(0,0,1),z1-z0,mat)
def bay(cx,wy,fh,d,zs,h,front,side,planter,lid_mat):
    """Glazed canted bay: leaded windows on each face, corner posts, opaque core and a lid; returns lid top."""
    zh=zs+h+.09;fb=trap(cx,wy,fh,d)
    slab(G['plaster'],cx,wy,fh,d,-.16,zs-.2,zh,M['plaster'])                # core behind the glass
    for a,b in zip(fb,fb[1:]):
        n=(b-a).cross(Vector((0,0,1))).normalized();mid=(a+b)/2
        if a.y==b.y:
            for off in front:window(G,M,(mid.x+off[0],mid.y,zs),n,off[1],h,shutters=False,wall_th=.2,band=band())
            if planter:   # one box across the front, set against the stone base (two kit planters would overlap)
                G['timber'].box((mid.x,mid.y-.11,zs-.39),(2*fh+.1,.24,.2),M['planks'],.006)
                G['detail'].box((mid.x,mid.y-.11,zs-.285),(2*fh+.04,.19,.02),M['soil'],0)
        else:window(G,M,(mid.x,mid.y,zs),n,side,h,shutters=False,wall_th=.2,band=band())
    for p in fb[1:3]:G['timber'].box((p.x,p.y,(zs-.2+zh)/2),(.13,.13,zh-zs+.2),M['timber'],.012,grain=True,band=band())
    slab(G['timber'],cx,wy,fh,d,.05,zh,zh+.1,lid_mat);return zh+.1
def canted_roof(cx,wy,fh,d,zlid,o,k=.75):
    """Hipped slate roof over a canted bay: three planes rising at slope k into the wall at wy."""
    ze=zlid-k*(o-.05)-.01;   # deck passes over the lid edge
    r_max=d+o;q=math.sqrt(1+k*k);L=r_max*q;inwall=lambda p:p.y>wy+.02
    BL,FL,FR,BR=trap(cx,wy,fh,d,o)
    for (A,B),grow in (((BL,FL),(1,0)),((FL,FR),(0,0)),((FR,BR),(0,1))):
        e=(B-A).normalized();inward=Vector((0,0,1)).cross(e);Le=(B-A).length;O=A+Vector((0,0,ze))
        lo=lambda t,g=grow[0]:-t/q if g else .414*t/q
        hi=lambda t,g=grow[1],Le=Le:Le+t/q if g else Le-.414*t/q
        plane(O,e,inward+Vector((0,0,k)),L,lo,hi,[inwall],size=(.2,.28,.018),exposure=.13)
    for C,s in ((FL,1),(FR,-1)):
        top=Vector((C.x+s*.414*r_max,wy,ze+k*r_max));cap(C+Vector((0,0,ze+.04)),top+Vector((0,0,.04)),.16)
    G['timber'].beam(BL+Vector((0,-.05,ze+k*r_max+.02)),BR+Vector((0,-.05,ze+k*r_max+.02)),.08,.1,M['timber'],band=band())
    return ze+k*r_max
# ---- walls -------------------------------------------------------------------------------
STEP,DS,DOOR,DX=.3,1.55,.95,1.55          # door sill = top step; arch spring above the sill; width; centre
wall(G['stone'],M['stone'],(0,YG,0),(0,-1,0),W2,JZ0,[(DX,DOOR,STEP,STEP+DS+DOOR/2)],arch=(DX,DOOR,STEP+DS))
WZ,WH=1.0,1.1
def hole(a,w,z=WZ,h=WH,base=0):return (a,w+.18,z-.09-base,z+h+.09-base)
for s in (-1,1):wall(G['stone'],M['stone'],(s*W2,.175,0),(s,0,0),2.025,JZ,[hole(s*(.6-.175),.6)])
wall(G['stone'],M['stone'],(0,D2,0),(0,1,0),W2,JZ,[hole(-x,.7) for x in (-1.5,1.5)])
PLATE=main_z(0,YF)-.04
UW=[(-W2,XG-GW,None),(XG+GW,W2,1.9)]       # upper front/rear plain sections either side of the gable wall
FLANK=[(-2.0,.28),(.8,.28)];FZ,FH=4.4,.7  # narrow lights either side of the oriel
for a0,a1,x in UW:wall(G['plaster'],M['plaster'],((a0+a1)/2,YF,JZ),(0,-1,0),(a1-a0)/2,PLATE-JZ,[hole(x-(a0+a1)/2,.55,4.15,1.05,JZ)] if x else [])
wall(G['plaster'],M['plaster'],(XG,YF,JZ),(0,-1,0),GW,GC-JZ,[hole(x-XG,w,FZ,FH,JZ) for x,w in FLANK])
G['plaster'].prism([(XG-GW,YF+.15,GC),(XG+GW,YF+.15,GC),(XG,YF+.15,GR-.05)],(0,1,0),.3,M['plaster'])
for a0,a1,x in UW:wall(G['plaster'],M['plaster'],((a0+a1)/2,D2,JZ),(0,1,0),(a1-a0)/2,PLATE-JZ,[hole(-(x-(a0+a1)/2),.55,4.2,.9,JZ)] if x else [])
wall(G['plaster'],M['plaster'],(XG,D2,JZ),(0,1,0),GW,GC-JZ,[hole(0,.7,4.1,1.0,JZ)])
G['plaster'].prism([(XG-GW,D2-.15,GC),(XG+GW,D2-.15,GC),(XG,D2-.15,GR-.05)],(0,1,0),.3,M['plaster'])
YS=D2-.3;ZS=main_z(W2,YS)-.05             # upper side walls: rectangle, then the roof profile
for s in (-1,1):
    wall(G['plaster'],M['plaster'],(s*W2,0,JZ),(s,0,0),YS,ZS-JZ,[hole(0,.55,4.2,.85,JZ)])
    top=[(y,main_z(W2,y)-.05) for y in np.linspace(-YS,YS,41)]
    G['plaster'].prism([(s*(W2-.15),y,z) for y,z in [(YS,ZS),(-YS,ZS)]+top],(1,0,0),.3,M['plaster'])
# ---- jetty: bressumer and stepped corbels -----------------------------------------------------
G['timber'].box((0,-2.19,3.2),(2*W2+.1,.78,.2),M['timber'],.015,grain=True,band=band())
for x in np.linspace(-2.55,2.55,8):
    G['timber'].box((x,YG-.18,3.0),(.16,.36,.2),M['timber'],.012,grain=True,band=band())
    G['timber'].box((x,YG-.11,2.8),(.14,.22,.2),M['timber'],.012,grain=True,band=band())
# ---- ground bay window under a slate pent roof --------------------------------------------------
BX,BF,BD=-.86,.8,.5            # centre, front half width, projection
slab(G['stone'],BX,YG,BF,BD,0,0,.72,M['stone'])
canted_roof(BX,YG,BF,BD,bay(BX,YG,BF,BD,.84,1.14,[(-.4,.58),(.4,.58)],.45,True,M['timber']),.15)
# ---- upper oriel on corbels, with a planter -------------------------------------------------
OF,OD=.65,.45
slab(G['timber'],XG,YF,OF,OD,.25,3.98,4.13,M['timber'])
for x in (XG-.55,XG,XG+.55):
    G['timber'].beam((x,YF-.02,3.4),(x,YF-OD-.12,3.97),.1,.12,M['timber'],band=band())
    G['timber'].box((x,YF-.1,3.67),(.12,.2,.54),M['timber'],.012,grain=True,band=band())
G['timber'].box((XG,YF-OD-.13,4.23),(2*OF+.1,.2,.2),M['planks'],.006)
G['detail'].box((XG,YF-OD-.13,4.33),(2*OF+.04,.15,.02),M['soil'],0)
canted_roof(XG,YF,OF,OD,bay(XG,YF,OF,OD,4.32,1.12,[(0,1.0)],.4,False,M['timber']),.12)
# ---- timber framing ----------------------------------------------------------------------------
y=YF-.03
for x,z1 in ((-2.68,PLATE),(XG-GW,GC),(XG+GW,GC),(2.68,PLATE)):post(x,y,JZ,z1,'y',.14)
rail((-W2,y,3.38),(W2,y,3.38),.12)
for a0,a1 in ((-W2,-1.72),(.48,XG+GW)):rail((a0,y,4.22),(a1,y,4.22),.12);rail((a0,y,5.27),(a1,y,5.27),.12)
for a0,a1 in ((-W2,XG-GW),(XG+GW,W2)):rail((a0,y,PLATE-.08),(a1,y,PLATE-.08),.16)
rail((2.68,y,3.45),(2.38,y,3.98),.1);rail((2.68,y,5.62),(2.38,y,5.3),.1)
def gable_frame(y):
    hx=(GR-.05-6.13)/KG;rail((XG-hx+.05,y,6.13),(XG+hx-.05,y,6.13),.12)
    hc=(GR-.05-7.3)/KG;rail((XG-hc+.05,y,7.3),(XG+hc-.05,y,7.3),.12);post(XG,y,6.19,GR-.08,'y',.14)
    for s in (-1,1):post(XG+s*.75,y,6.19,7.24,'y',.12);rail((XG+s*1.35,y,6.2),(XG+s*.8,y,6.75),.1);rail((XG+s*.1,y,7.36),(XG+s*.5,y,GR-.05-.5*KG-.05),.1)
gable_frame(y)
y=D2+.03                       # rear framing
for x,z1 in ((-2.68,PLATE),(XG-GW,GC),(XG+GW,GC),(2.68,PLATE)):post(x,y,JZ,z1,'y',.14)
rail((-W2,y,JZ),(W2,y,JZ),.2);rail((XG-GW,y,5.3),(XG+GW,y,5.3),.12)
for a0,a1 in ((-W2,XG-GW),(XG+GW,W2)):rail((a0,y,PLATE-.08),(a1,y,PLATE-.08),.16)
gable_frame(y)
for s in (-1,1):               # side framing
    x=s*(W2+.03);rail((x,YF,JZ),(x,D2,JZ),.2)
    for yy in (-YS,-1.0,1.0,YS):post(x,yy,JZ+.1,main_z(W2,yy)-.12,'x',.14)
    rail((x,-YS,4.0),(x,-.45,4.0),.12);rail((x,.45,4.0),(x,YS,4.0),.12)
# ---- openings ------------------------------------------------------------------------------
for s in (-1,1):window(G,M,(s*W2,.6,WZ),(s,0,0),.6,WH,shutters=False,band=band())
for x in (-1.5,1.5):window(G,M,(x,D2,WZ),(0,1,0),.7,WH,shutters=False,band=band())
window(G,M,(1.9,YF,4.15),(0,-1,0),.55,1.05,shutters=False,planter=True,band=band())
for x,w in FLANK:window(G,M,(x,YF,FZ),(0,-1,0),w,FH,shutters=False,band=band())
window(G,M,(1.9,D2,4.2),(0,1,0),.55,.9,shutters=False,band=band())
window(G,M,(XG,D2,4.1),(0,1,0),.7,1.0,shutters=False,band=band())
for s in (-1,1):window(G,M,(s*W2,0,4.2),(s,0,0),.55,.85,shutters=False,band=band())
door(G,M,(DX,YG,STEP),(0,-1,0),DOOR,DS,STONES,band=band())
G['stone'].box((DX,YG-.175,.15),(1.3,.35,.3),M['stone'],.015)            # two steps up to the sill
G['stone'].box((DX,YG-.525,.075),(1.3,.35,.15),M['stone'],.015)
G['stone'].box((2.45,YG-.22,.42),(.4,.44,.84),M['ashlar'],.015,rect=kit.rng.choice(STONES))   # planter pier beside the steps
G['stone'].box((2.45,YG-.22,.87),(.48,.52,.06),M['ashlar'],.012,rect=kit.rng.choice(STONES))
G['detail'].box((2.45,YG-.22,.91),(.38,.42,.02),M['soil'],0)
# ---- downpipes at the front corners, hoppers on the side fascia ------------------------------------
for s in (-1,1):
    x,yp=s*(W2+.11),-2.05
    G['detail'].beam((x,yp,0),(x,yp,5.0),.08,.08,M['iron'],grain=False)
    G['detail'].beam((x,yp,4.96),(s*2.99,yp,5.36),.08,.08,M['iron'],grain=False)
    G['detail'].box((s*2.99,yp,5.42),(.14,.16,.14),M['iron'],.01)
    for z in (.4,1.6,2.8,4.0):G['detail'].box((s*(W2+.07),yp,z),(.14,.11,.04),M['iron'],.004)
lantern(G,M,(2.6,YG-.42,2.15),arm=((2.6,YG-.09,2.15),(0,-1,0)))
# ---- stag banner on a forged bracket off the right wall ----------------------------------------
def stag(geo,origin,right,up,normal,s):
    o=Vector(origin)+Vector(normal)*.008;R,U,N=Vector(right),Vector(up),Vector(normal)
    def p(a,z):return o+R*(a*s)+U*(z*s)
    geo.prism([p(a,z) for a,z in [(-.12,.25),(.12,.25),(.09,-.05),(.04,-.3),(-.04,-.3),(-.09,-.05)]],N,.02,M['gilt'])
    for sx in (-1,1):
        geo.prism([p(sx*a,z) for a,z in [(.1,.2),(.33,.31),(.13,.1)]],N,.02,M['gilt'])
        for (a0,z0),(a1,z1) in [((.07,.22),(.25,.45)),((.25,.45),(.31,.72)),((.22,.4),(.39,.52)),((.28,.58),(.44,.67)),((.17,.34),(.1,.55))]:
            geo.beam(p(sx*a0,z0),p(sx*a1,z1),.02,.045*s,M['gilt'],grain=False)
def trim(geo,pts,normal,w=.03):
    n=Vector(normal)*.008
    for a,b in zip(pts,pts[1:]+pts[:1]):geo.beam(Vector(a)+n,Vector(b)+n,.02,w,M['gilt'],grain=False)
AY,AZ,AX=-1.7,2.85,W2+1.0
G['detail'].beam((W2-.05,AY,AZ),(AX,AY,AZ),.045,.045,M['iron'],grain=False)
G['detail'].beam((W2-.05,AY,AZ-.75),(W2+.75,AY,AZ-.02),.03,.03,M['iron'],grain=False)
G['detail'].box((W2+.03,AY,AZ-.35),(.06,.1,.9),M['iron'],.004)
for k in range(8):             # scroll under the arm
    t0,t1=math.tau*k/8,math.tau*(k+1)/8;c=Vector((W2+.32,AY,AZ-.2));r=.14
    G['detail'].beam(c+Vector((r*math.cos(t0),0,r*math.sin(t0))),c+Vector((r*math.cos(t1),0,r*math.sin(t1))),.02,.02,M['iron'],grain=False)
G['detail'].box((AX-.04,AY,AZ+.08),(.05,.05,.1),M['iron'],.004)     # finial knob at the arm tip
BXC=W2+.68;bn=[(BXC-.27,2.72),(BXC+.27,2.72),(BXC+.27,1.6),(BXC,1.3),(BXC-.27,1.6)]
G['detail'].prism([(x,AY,z) for x,z in bn],(0,1,0),.02,M['banner'])
G['detail'].beam((BXC-.33,AY,2.74),(BXC+.33,AY,2.74),.03,.03,M['iron'],grain=False)
for x in (BXC-.25,BXC+.25):G['detail'].beam((x,AY,AZ-.02),(x,AY,2.75),.015,.015,M['iron'],grain=False)
for n in (-1,1):
    trim(G['detail'],[(x,AY+n*.01,z) for x,z in bn],(0,n,0))
    stag(G['detail'],(BXC,AY+n*.01,2.15),(-n,0,0),(0,0,1),(0,n,0),.55)
# ---- export, preview scene, manifest ---------------------------------------------
objs=[g.object() for g in G.values()]
from mathutils import Matrix
lo=[min((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)];hi=[max((o.matrix_world@Vector(c))[k] for o in objs for c in o.bound_box) for k in range(3)]
for o in objs:o.data.transform(Matrix.Translation((-(lo[0]+hi[0])/2,-(lo[1]+hi[1])/2,0)))
assert abs(lo[2])<1e-3,f'not grounded: {lo[2]}'
assert all(h-l<=s+1e-3 for l,h,s in zip(lo,hi,SLOT)),f'exceeds slot {SLOT}: {[round(h-l,3) for l,h in zip(lo,hi)]}'
OUT=HERE/'SilverbrookTownhouse6'
for o in bpy.data.objects:o.select_set(o in objs)
bpy.context.view_layer.objects.active=objs[0]
bpy.ops.export_scene.gltf(filepath=str(OUT)+'.glb',export_format='GLB',use_selection=True,export_image_format='NONE')
DOORS=kit.split_doors(str(OUT),'SM_SilverbrookTownhouse6')  # door leaves: own meshes, hinged
bpy.ops.export_scene.fbx(filepath=str(OUT)+'.fbx',use_selection=True,path_mode='RELATIVE',colors_type='LINEAR',add_leaf_bones=False)
pre=bpy.data.collections.new('Preview');scene.collection.children.link(pre)
ground=bpy.data.meshes.new('Ground');ground.from_pydata([(-30,-30,0),(30,-30,0),(30,30,0),(-30,30,0)],[],[(0,1,2,3)]);g=bpy.data.objects.new('Ground',ground);pre.objects.link(g);g.data.materials.append(kit.solid('Preview_Ground',(.11,.12,.07),.95))
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.color=(1,.94,.86);sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));pre.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def cam(name,loc,rot,ortho=None,lens=40):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=[math.radians(a) for a in rot];pre.objects.link(o)
    if ortho:c.type='ORTHO';c.ortho_scale=ortho
    else:c.lens=lens
cam('Beauty',(-11,-14.5,5.6),(81,0,-37),lens=38);cam('Front',(0,-30,4.5),(90,0,0),9.8);cam('Side',(-30,0,4.5),(90,0,-90),9.8);cam('Rear',(0,30,4.5),(90,0,180),9.8)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=scene.render.resolution_y=1100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT)+'.blend',relative_remap=True)
shutil.copy2(REF,HERE/'Townhouse6-reference.png')
mins=Vector([min(min((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)]);maxs=Vector([max(max((o.matrix_world@Vector(c))[k] for c in o.bound_box) for o in objs) for k in range(3)])
tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in objs)
assert tris<=120_000,f'house triangle budget: {tris}'
manifest=dict(asset='SilverbrookTownhouse6',reference=dict(file='Townhouse6-reference.png',sha256=hashlib.sha256(REF.read_bytes()).hexdigest()),
    blender=bpy.app.version_string,generator='build_townhouse6.py',method='procedural original geometry (concept interpretation, not image reconstruction)',
    units='metres, Z up, front -Y, origin ground centre',bounds_m=[round(v,3) for v in maxs-mins],
    meshes={o.name:dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials]) for o in objs},triangles=tris,
    materials=[r for r in kit.ROWS if not r['name'].startswith('Preview_')],sources='../materials-provenance.json (CC0 Poly Haven scans)',unreal_mesh='SM_SilverbrookTownhouse6',parts=DOORS,slot_m=list(SLOT),outputs=['SilverbrookTownhouse6.blend','SilverbrookTownhouse6.glb','SilverbrookTownhouse6.fbx'])
(HERE/'asset-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('TOWNHOUSE6_BUILT',manifest['bounds_m'],tris)
